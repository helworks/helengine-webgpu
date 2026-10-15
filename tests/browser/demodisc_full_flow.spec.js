const {test,expect}=require('@playwright/test');
const {boot,choose,scene,frames,key}=require('./demodisc-controls');
const cases=require('./demodisc_scene_cases');
const {readback}=require('./render-readback');

async function observeGpu(page) {
  await page.addInitScript(()=>{
    window.sceneGpu={draws:0,meshDraws:0,shadowDraws:0,uploads:0,hashes:[],traceUploads:0,traceHashes:[],triggerKinds:[],errors:[]};
    window.sceneResources={buffers:[],textures:[]};
    function observeTriggers(){for(const kind of window.helengineDiagnostics?.triggerEvents||[]) if(!window.sceneGpu.triggerKinds.includes(kind)) window.sceneGpu.triggerKinds.push(kind);requestAnimationFrame(observeTriggers);}requestAnimationFrame(observeTriggers);
    const requestAdapter=navigator.gpu.requestAdapter.bind(navigator.gpu);
    navigator.gpu.requestAdapter=async(...args)=>{
      const adapter=await requestAdapter(...args);const requestDevice=adapter.requestDevice.bind(adapter);
      adapter.requestDevice=async(...args)=>{
        const device=await requestDevice(...args);
        window.sceneGpuDevice=device;
        device.addEventListener('uncapturederror',event=>window.sceneGpu.errors.push(event.error.message));
        const createBuffer=device.createBuffer.bind(device);
        device.createBuffer=descriptor=>{const buffer=createBuffer(descriptor);window.sceneResources.buffers.push(new WeakRef(buffer));return buffer;};
        const pipelines=new WeakMap();const createPipeline=device.createRenderPipeline.bind(device);
        device.createRenderPipeline=descriptor=>{const pipeline=createPipeline(descriptor);pipelines.set(pipeline,{mesh:descriptor.vertex.buffers?.[0]?.arrayStride===32,shadow:!descriptor.fragment?.targets?.length});return pipeline;};
        const sizes=new WeakMap();const createTexture=device.createTexture.bind(device);
        device.createTexture=descriptor=>{const texture=createTexture(descriptor);window.sceneResources.textures.push(new WeakRef(texture));const size=descriptor.size;sizes.set(texture,Array.isArray(size)?size:[size.width,size.height]);return texture;};
        const write=device.queue.writeTexture.bind(device.queue);
        device.queue.writeTexture=(destination,data,...rest)=>{
          const bytes=ArrayBuffer.isView(data)?new Uint8Array(data.buffer,data.byteOffset,data.byteLength):new Uint8Array(data);
          let hash=2166136261;for(let index=0;index<bytes.length;index+=Math.max(1,Math.floor(bytes.length/4096))) hash=Math.imul(hash^bytes[index],16777619);
          window.sceneGpu.uploads++;if(window.sceneGpu.hashes.length<4096) window.sceneGpu.hashes.push(hash>>>0);
          const size=sizes.get(destination.texture);if(size?.[0]===320 && size?.[1]===240){window.sceneGpu.traceUploads++;if(window.sceneGpu.traceHashes.length<4096) window.sceneGpu.traceHashes.push(hash>>>0);}
          return write(destination,data,...rest);
        };
        const create=device.createCommandEncoder.bind(device);
        device.createCommandEncoder=(...args)=>{
          const encoder=create(...args);const begin=encoder.beginRenderPass.bind(encoder);
          encoder.beginRenderPass=(...args)=>{
            const pass=begin(...args);let current;const setPipeline=pass.setPipeline.bind(pass);
            pass.setPipeline=pipeline=>{current=pipelines.get(pipeline);return setPipeline(pipeline);};
            for(const name of ['draw','drawIndexed']) {const draw=pass[name].bind(pass);pass[name]=(...args)=>{window.sceneGpu.draws++;if(current?.mesh){if(current.shadow) window.sceneGpu.shadowDraws++;else window.sceneGpu.meshDraws++;}return draw(...args);};}return pass;
          };return encoder;
        };return device;
      };return adapter;
    };
  });
}

for(const id of ['HelenOfCodeSplash','DemoDiscMainMenu','SceneLoadingScreen']) test('scene '+id,async({page})=>{
  test.setTimeout(90000);await observeGpu(page);await page.goto('http://127.0.0.1:8197/game/');await scene(page,id);await frames(page,3);
  expect(await page.evaluate(()=>window.sceneGpu.errors)).toEqual([]);
});

for(const entry of cases) test('scene '+entry.id,async({page})=>{
  test.setTimeout(120000);const errors=[];page.on('pageerror',error=>errors.push(error.message));await observeGpu(page);await boot(page);
  await choose(page,entry.category,entry.item);
  if(entry.kind==='ray') {for(let i=0;i<entry.rayItem;i++) await key(page,'ArrowDown');await key(page,'Enter');}
  if(entry.kind==='tilt-level') {
    await scene(page,'tilt_trial');await frames(page,6);await key(page,'Enter');
    for(let i=0;i<entry.level;i++) await key(page,'ArrowDown');await key(page,'Enter');await key(page,'Enter');
  }
  await scene(page,entry.id);await frames(page,6);
  const before=await page.evaluate(()=>({state:window.helengineDiagnostics,gpu:{...window.sceneGpu,hashes:[...window.sceneGpu.hashes]}}));
  expect(before.state.components.some(name=>name.includes('CameraComponent'))).toBe(true);
  if(entry.kind==='ray') {
    expect(before.state.components.some(name=>name.includes('SoftwarePathTracerComponent'))).toBe(true);
    const spp=state=>Math.max(0,...state.texts.filter(text=>/^SPP: /.test(text)).map(text=>Number(text.slice(5))));
    await expect.poll(async()=>spp(await page.evaluate(()=>window.helengineDiagnostics)),{timeout:60000}).toBeGreaterThan(spp(before.state));
    const after=await page.evaluate(()=>window.sceneGpu);
    expect(after.traceUploads).toBeGreaterThan(before.gpu.traceUploads);expect(new Set(after.traceHashes.slice(before.gpu.traceHashes.length)).size).toBeGreaterThan(1);
  } else if(entry.kind==='physics') {
    await expect.poll(()=>page.evaluate(()=>window.helengineRuntimeConfiguration.physicsType)).toContain('HelPhysics');
    expect(before.state.components.some(name=>/RigidBody|CharacterController/.test(name))).toBe(true);
    await frames(page,40);
    const after=await page.evaluate(()=>window.helengineDiagnostics);
    const moved=Object.entries(before.state.entities).some(([id,entity])=>entity.components.some(name=>/RigidBody|CharacterController|KinematicMotion/.test(name)) && after.entities[id] && entity.position.some((value,index)=>Math.abs(value-after.entities[id].position[index])>.0001));
    // Controllers in the authored content move autonomously; spawn markers alone do not create controllers.
    if(before.state.components.some(name=>name.includes('CharacterController3DComponent'))) {
      expect(Object.entries(before.state.entities).some(([id,entity])=>entity.components.some(name=>/CharacterController/.test(name)) && after.entities[id] && entity.position.some((value,index)=>Math.abs(value-after.entities[id].position[index])>.001))).toBe(true);
    }
    else expect(moved).toBe(true);
    if(entry.id.includes('moving_platform') || entry.id.includes('kinematic_push')) expect(Object.entries(before.state.entities).some(([id,entity])=>entity.components.some(name=>/KinematicMotion/.test(name)) && after.entities[id] && entity.position.some((value,index)=>Math.abs(value-after.entities[id].position[index])>.001))).toBe(true);

  } else if(entry.kind==='physics-layout') {
    expect(before.state.components.some(name=>/RigidBody3DComponent|CharacterController3DComponent/.test(name))).toBe(false);
    expect(before.state.components.filter(name=>name.includes('MeshComponent')).length).toBeGreaterThan(1);
  } else if(entry.kind==='tilt-level') {
    expect(before.state.components.some(name=>name.includes('TiltTrialSessionComponent'))).toBe(true);await key(page,'Enter');
    const start=await page.evaluate(()=>window.helengineDiagnostics.entities);const texts=await page.evaluate(()=>window.helengineDiagnostics.texts);
    await page.keyboard.down('d');await frames(page,45);await page.keyboard.up('d');
    const end=await page.evaluate(()=>window.helengineDiagnostics.entities);
    expect(Object.entries(start).some(([id,entity])=>entity.components.some(name=>name.includes('RigidBody3DComponent')) && end[id] && entity.position.some((value,index)=>Math.abs(value-end[id].position[index])>.001))).toBe(true);
    expect(await page.evaluate(()=>window.helengineDiagnostics.texts)).not.toEqual(texts);
  } else if(entry.kind==='zombie') {
    expect(before.state.components.some(name=>name.includes('ZombislayerFpsControllerComponent'))).toBe(true);
    const player=Object.entries(before.state.entities).find(([,entity])=>entity.components.some(name=>name.includes('ZombislayerFpsControllerComponent')));expect(player).toBeTruthy();
    await page.keyboard.down('w');await frames(page,40);await page.keyboard.up('w');
    expect(await page.evaluate(id=>window.helengineDiagnostics.entities[id].position,player[0])).not.toEqual(player[1].position);
    await key(page,'Escape');const paused=await page.evaluate(id=>window.helengineDiagnostics.entities[id].position,player[0]);
    expect(await page.evaluate(()=>window.helengineDiagnostics.texts.some(text=>/pause/i.test(text)))).toBe(true);
    expect((await readback(page)).uiPixels).toBeGreaterThan(0);
    await page.keyboard.down('w');await frames(page,20);await page.keyboard.up('w');
    expect(await page.evaluate(id=>window.helengineDiagnostics.entities[id].position,player[0])).toEqual(paused);
    await key(page,'Enter');
    expect(await page.evaluate(()=>window.helengineDiagnostics.texts.some(text=>/pause/i.test(text)))).toBe(false);
  } else if(entry.kind==='benchmark') {
    const objectCount=texts=>Math.max(0,...texts.filter(text=>text.startsWith('Objects: ')).map(text=>Number(/^Objects: (\d+)/.exec(text)?.[1]||0)));
    const count=objectCount(before.state.texts);
    await expect.poll(async()=>objectCount(await page.evaluate(()=>window.helengineDiagnostics.texts)),{timeout:10000}).toBeGreaterThan(count);
  }
  const pixels=await readback(page);
  if(entry.kind==='rendering') {
    expect(before.state.components.some(name=>name.includes('MeshComponent'))).toBe(true);
    expect(await page.evaluate(()=>window.sceneGpu.meshDraws)).toBeGreaterThan(before.gpu.meshDraws);
    expect(pixels.scenePixels).toBeGreaterThan(20);expect(pixels.sceneColors).toBeGreaterThan(2);
    if(entry.id==='pbr_material_gallery') expect(before.state.components.filter(name=>name.includes('MeshComponent')).length).toBeGreaterThanOrEqual(25);
    if(['directional_shadow_plaza','pbr_shadow_theater'].includes(entry.id)) expect(await page.evaluate(()=>window.sceneGpu.shadowDraws)).toBeGreaterThan(before.gpu.shadowDraws);
  }
  if(entry.kind==='zombie') {
    // The authored gameplay view has no HUD; its unbound model slots use the desktop magenta shader.
    expect(pixels.background).toEqual([255,0,255,255]);
    expect(pixels.center).not.toEqual(pixels.background);
  } else expect(pixels.uiPixels).toBeGreaterThan(0);
  expect(await page.evaluate(()=>window.sceneGpu.draws)).toBeGreaterThan(before.gpu.draws);
  expect(await page.evaluate(()=>window.sceneGpu.errors)).toEqual([]);expect(errors).toEqual([]);
  console.log(JSON.stringify({scene:entry.id,components:before.state.components.filter((value,index,list)=>list.indexOf(value)===index),texts:before.state.texts,pixels}));
  if(entry.kind==='tilt-menu') {await key(page,'ArrowUp');await key(page,'Enter');}
  else if(entry.kind==='tilt-level') {await key(page,'Escape');await scene(page,'tilt_trial');await frames(page,4);await key(page,'ArrowUp');await key(page,'Enter');}
  else if(entry.kind==='zombie') {await key(page,'Escape');await key(page,'Backspace');}
  else await key(page,'Escape');
  await scene(page,'DemoDiscMainMenu');await frames(page,3);
  expect(await page.evaluate(()=>window.helengineDiagnostics.sceneIds)).not.toContain(entry.id);
});

test('three menu cycles retire physics scenes and stop original ray uploads',async({page})=>{
  test.setTimeout(180000);await observeGpu(page);await boot(page);
  const cdp=await page.context().newCDPSession(page);const physicsIdentities=[];const retained=[];
  for(let cycle=0;cycle<3;cycle++) {
    await choose(page,0,0);await scene(page,'cube_test');await frames(page,4);
    await key(page,'Escape');await scene(page,'DemoDiscMainMenu');await frames(page,4);
    await choose(page,1,0);await scene(page,'test_scene_dynamic_stack_boxes');await frames(page,10);
    expect(await page.evaluate(()=>window.helengineRuntimeConfiguration.physicsType)).toContain('HelPhysics');
    const currentIdentity=await page.evaluate(()=>window.helengineRuntimeConfiguration.physicsIdentity);
    expect(currentIdentity).toBeGreaterThan(0);physicsIdentities.push(currentIdentity);
    await frames(page,10);expect(await page.evaluate(()=>window.helengineRuntimeConfiguration.physicsIdentity)).toBe(currentIdentity);
    await key(page,'Escape');await scene(page,'DemoDiscMainMenu');await frames(page,4);
    expect(await page.evaluate(()=>window.helengineDiagnostics.components.some(name=>name.includes('RigidBody3DComponent')))).toBe(false);
    // HelPhysics intentionally releases its scene-owned world when returning to a physics-free menu.
    expect(await page.evaluate(()=>window.helengineRuntimeConfiguration.physicsIdentity)).toBe(0);
    expect(await page.evaluate(()=>window.helengineRuntimeConfiguration.physicsType)).toBe('');
    await choose(page,0,10);await key(page,'Enter');await scene(page,'software_path_tracer');
    await expect.poll(()=>page.evaluate(()=>window.helengineDiagnostics.texts.some(text=>/^SPP: [1-9]/.test(text))),{timeout:60000}).toBe(true);
    await key(page,'Escape');await scene(page,'DemoDiscMainMenu');await frames(page,4);
    const uploads=await page.evaluate(()=>window.sceneGpu.traceUploads);await frames(page,20);
    expect(await page.evaluate(()=>window.sceneGpu.traceUploads)).toBe(uploads);
    expect(await page.evaluate(()=>window.helengineDiagnostics.components.some(name=>name.includes('SoftwarePathTracerComponent')))).toBe(false);
    expect(await page.evaluate(()=>window.sceneGpu.errors)).toEqual([]);
    await page.evaluate(()=>window.sceneGpuDevice.queue.onSubmittedWorkDone());await cdp.send('HeapProfiler.collectGarbage');await frames(page,4);
    await page.evaluate(()=>window.sceneGpuDevice.queue.onSubmittedWorkDone());await cdp.send('HeapProfiler.collectGarbage');
    retained.push(await page.evaluate(()=>({buffers:window.sceneResources.buffers.filter(value=>value.deref()).length,textures:window.sceneResources.textures.filter(value=>value.deref()).length})));
  }
  expect(retained[2].buffers).toBeLessThanOrEqual(retained[1].buffers);expect(retained[2].textures).toBeLessThanOrEqual(retained[1].textures);
  console.log(JSON.stringify({physicsIdentities,retainedGpuResources:retained}));await cdp.detach();
});

test('original Tilt gameplay collects a coin and emits trigger enter and exit',async({page})=>{
  test.setTimeout(90000);await observeGpu(page);await boot(page);await choose(page,2,0);
  await scene(page,'tilt_trial');await frames(page,6);await key(page,'Enter');await key(page,'Enter');await key(page,'Enter');
  await scene(page,'tilt_trial_level_01');await frames(page,6);await key(page,'Enter');
  await page.keyboard.down('w');
  try {
    await expect.poll(()=>page.evaluate(()=>window.helengineDiagnostics.texts.some(text=>/^Coins [1-3]\/3$/.test(text))),{timeout:15000}).toBe(true);
    await expect.poll(()=>page.evaluate(()=>window.sceneGpu.triggerKinds.includes(0) && window.sceneGpu.triggerKinds.includes(2)),{timeout:15000}).toBe(true);
  } finally {await page.keyboard.up('w');}
  expect(await page.evaluate(()=>window.sceneGpu.errors)).toEqual([]);
  console.log(JSON.stringify(await page.evaluate(()=>({triggerKinds:window.sceneGpu.triggerKinds,texts:window.helengineDiagnostics.texts}))));
  await key(page,'Escape');await scene(page,'tilt_trial');
});

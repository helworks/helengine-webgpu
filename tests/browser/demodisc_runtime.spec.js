const {test,expect}=require('@playwright/test');
const {boot,choose,scene,frames,key}=require('./demodisc-controls');

test('complete catalog reaches the original desktop menu',async({page})=>{
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto('http://127.0.0.1:8197/game/');
  await page.waitForFunction(()=>window.helengineDiagnostics?.sceneIds?.includes('DemoDiscMainMenu') || document.getElementById('helengine-error')?.textContent,{},{timeout:60000});
  expect(await page.locator('#helengine-error').textContent()).toBe('');
  expect(await page.evaluate(()=>window.helengineDiagnostics.sceneIds)).toContain('DemoDiscMainMenu');
  expect(errors).toEqual([]);
});

test('runtime exposes all original modules and the project-selected physics registration',async({page})=>{
  await page.goto('http://127.0.0.1:8197/game/');
  await page.waitForFunction(()=>window.helengineDiagnostics?.frames>3 || document.getElementById('helengine-error')?.textContent,{},{timeout:60000});
  expect(await page.locator('#helengine-error').textContent()).toBe('');
  const runtime=await page.evaluate(()=>window.helengineRuntimeConfiguration);
  expect(runtime.sceneCount).toBe(37);
  expect(runtime.moduleIds.sort()).toEqual(['DemoDisc','Raytracing','TiltPlay','Zombislayer']);
  await boot(page); await choose(page,1,0); await scene(page,'test_scene_dynamic_stack_boxes');
  await expect.poll(()=>page.evaluate(()=>window.helengineRuntimeConfiguration.physicsType)).toContain('HelPhysics');
});


test('browser standard gamepad opens the original Rendering panel',async({page})=>{
  test.setTimeout(90000);
  await page.addInitScript(()=>{
    window.testPadButtons=Array.from({length:17},()=>({pressed:false,touched:false,value:0}));
    Object.defineProperty(navigator,'getGamepads',{value:()=>[{id:'Acceptance standard controller',index:0,mapping:'standard',connected:true,axes:[0,0,0,0],buttons:window.testPadButtons,timestamp:performance.now()}]});
  });
  await boot(page);
  await page.evaluate(()=>{window.testPadButtons[0]={pressed:true,touched:true,value:1};});
  await frames(page,4);
  await page.evaluate(()=>{window.testPadButtons[0]={pressed:false,touched:false,value:0};});
  await frames(page,4);
  expect(await page.evaluate(()=>window.helengineDiagnostics.texts)).toContain('Cube Test');
});

const {expect}=require('@playwright/test');
async function frames(page,count=2) {
  const start=await page.evaluate(()=>window.helengineDiagnostics?.frames||0);
  await page.waitForFunction(({start,count})=>window.helengineDiagnostics?.frames>=start+count || document.getElementById('helengine-error').textContent,{start,count},{timeout:60000});
  expect(await page.locator('#helengine-error').textContent()).toBe('');
}
async function key(page,name) { await page.keyboard.down(name); await frames(page); await page.keyboard.up(name); await frames(page); }
async function scene(page,id) {
  await page.waitForFunction(id=>window.helengineDiagnostics?.sceneIds.includes(id) || document.getElementById('helengine-error').textContent,id,{timeout:60000});
  expect(await page.locator('#helengine-error').textContent()).toBe('');
  expect(await page.evaluate(()=>window.helengineDiagnostics.sceneIds)).toContain(id);
}
async function boot(page) {
  await page.goto('http://127.0.0.1:8197/game/'); await scene(page,'DemoDiscMainMenu');
  await page.waitForFunction(()=>!window.helengineDiagnostics.sceneIds.includes('HelenOfCodeSplash'),null,{timeout:60000});
  await page.locator('#helengine-canvas').focus(); await frames(page,4);
}
async function choose(page,category,item) {
  for(let i=0;i<category;i++) await key(page,'ArrowDown');
  await key(page,'Enter'); for(let i=0;i<item;i++) await key(page,'ArrowDown'); await key(page,'Enter');
}
async function openCube(page) {
  const response=await page.goto('http://127.0.0.1:8197/game/');
  await page.waitForFunction(()=>window.helengineDiagnostics?.frames>3 || document.getElementById('helengine-error').textContent,null,{timeout:60000});
  expect(await page.locator('#helengine-error').textContent()).toBe('');
  if(!(await page.evaluate(()=>window.helengineDiagnostics.sceneIds.includes('cube_test')))) {
    await scene(page,'DemoDiscMainMenu');
    await page.waitForFunction(()=>!window.helengineDiagnostics.sceneIds.includes('HelenOfCodeSplash'),null,{timeout:60000});
    await page.locator('#helengine-canvas').focus();await frames(page,3);await choose(page,0,0);await scene(page,'cube_test');await frames(page,3);
  }
  return response;
}
module.exports={frames,key,scene,boot,choose,openCube};

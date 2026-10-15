const {test,expect}=require('@playwright/test');
const {readback}=require('./render-readback');

test('camera targets preserve color across load and safely retire after a later camera samples them',async({page})=>{
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{
    window.seenPrimitiveTopologies=[];
    const create=GPUDevice.prototype.createRenderPipeline;
    GPUDevice.prototype.createRenderPipeline=function(descriptor){window.seenPrimitiveTopologies.push(descriptor.primitive?.topology||'triangle-list');return create.call(this,descriptor);};
  });
  await page.goto('http://127.0.0.1:8197/shaders/targets.html');
  await page.waitForFunction(()=>window.renderTargetError || window.renderTargetFrames>4);
  expect(await page.evaluate(()=>window.renderTargetError||'')).toBe('');
  const pixels=await readback(page);
  expect(pixels.center).toEqual([16,32,48,255]);
  expect(pixels.upperRight).toEqual([16,32,48,255]);
  expect(pixels.background).toEqual([0,0,0,255]);
  expect(await page.evaluate(()=>window.renderTargetUniformGrew)).toBe(true);
  expect(await page.evaluate(()=>window.renderTargetRetired)).toBe(true);
  expect(await page.evaluate(()=>window.renderTargetValidation)).toEqual({invalidSizeRejected:true,missingColorStreamRejected:true});
  expect(await page.evaluate(()=>window.seenPrimitiveTopologies)).toContain('line-list');
  expect(errors).toEqual([]);
});

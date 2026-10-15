const { test, expect } = require('@playwright/test');
const { readback } = require('./render-readback');

for (const face of [0,1,2,3,4,5,6]) {
  test(`radial point shadows sample cube face ${face} through ForwardStandard`, async ({ page }) => {
    const values={}; const errors=[]; page.on('pageerror',e=>errors.push(e.message));
    for(const mode of ['shadow','no-caster','no-receiver','zero-strength']) {
      await page.goto(`http://127.0.0.1:8197/shaders/atlas.html?light=point&face=${face}&mode=${mode}`);
      await page.waitForFunction(()=>window.atlasShadowError || window.atlasShadowFrames>2);
      expect(await page.evaluate(()=>window.atlasShadowError||'')).toBe(''); values[mode]=(await readback(page)).center[0];
    }
    expect(values['no-caster']).toBeGreaterThan(values.shadow+20);
    expect(Math.abs(values['no-receiver']-values['no-caster'])).toBeLessThanOrEqual(2);
    expect(Math.abs(values['zero-strength']-values['no-caster'])).toBeLessThanOrEqual(2);
    expect(errors).toEqual([]); console.log('face',face,JSON.stringify(values));
  });
}

test('two point lights coexist with atlas shadows', async ({page})=>{
  const values={};
  for(const mode of ['shadow','no-caster']) {
    await page.goto(`http://127.0.0.1:8197/shaders/atlas.html?light=point-mixed&face=4&mode=${mode}`);
    await page.waitForFunction(()=>window.atlasShadowError || window.atlasShadowFrames>2);
    expect(await page.evaluate(()=>window.atlasShadowError||'')).toBe(''); values[mode]=(await readback(page)).center[0];
  }
  expect(values['no-caster']).toBeGreaterThan(values.shadow+20);
});

test('removing an earlier point light and changing range reassigns the actual cube slot',async({page})=>{
  await page.goto('http://127.0.0.1:8197/shaders/atlas.html?light=point-mixed&face=4&mode=shadow');
  await page.waitForFunction(()=>window.atlasShadowError || window.atlasShadowFrames>2);
  expect(await page.evaluate(()=>window.atlasShadowError||'')).toBe('');
  expect((await readback(page)).center[0]).toBeLessThan(50);
  await page.evaluate(()=>{window.pointLightPhase=1;});
  await page.waitForFunction(()=>window.pointLightPhaseApplied===1);
  const state=await page.evaluate(()=>window.pointShadowState);
  expect(state.lightMetadata[0]).toBe(2);
  expect(state.shadowLight0Metadata).toEqual([1,1,2,0]);
  expect(state.shadowLight2Metadata).toEqual([0,0,0,0]);
  expect((await readback(page)).center[0]).toBeLessThan(50);
  expect(await page.evaluate(()=>window.atlasShadowError||'')).toBe('');
});

const { test, expect } = require('@playwright/test');
const { readback } = require('./render-readback');

for (const light of ['directional', 'spot', 'mixed', 'directional&off-camera=1']) {
  test(`${light} shadows use real depth passes and original ForwardStandard receivers`, async ({ page }) => {
    const errors=[]; page.on('pageerror', e=>errors.push(e.message)); page.on('console', m=>{if(m.type()==='error')errors.push(m.text());});
    const pixels={};
    for(const mode of ['shadow','no-caster','no-receiver','zero-strength']) {
      await page.goto(`http://127.0.0.1:8197/shaders/atlas.html?light=${light}&mode=${mode}`);
      await expect.poll(()=>page.evaluate(()=>window.atlasShadowFrames||0)).toBeGreaterThan(2);
      expect(await page.evaluate(()=>window.atlasShadowError||'')).toBe('');
      pixels[mode]=(await readback(page)).center[0];
    }
    expect(pixels['no-caster']).toBeGreaterThan(pixels.shadow+20);
    expect(Math.abs(pixels['no-receiver']-pixels['no-caster'])).toBeLessThanOrEqual(2);
    expect(Math.abs(pixels['zero-strength']-pixels['no-caster'])).toBeLessThanOrEqual(2);
    console.log(light, JSON.stringify(pixels));
    expect(errors).toEqual([]);
  });
}

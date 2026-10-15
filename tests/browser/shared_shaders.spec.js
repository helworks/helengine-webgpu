const {openCube}=require('./demodisc-controls');
const { test, expect } = require('@playwright/test');
const { readback } = require('./render-readback');

test('shared shaders load both original UI variants and the authored Forward selection', async ({ page }) => {
  await openCube(page);
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0), { timeout: 60000 }).toBeGreaterThan(5);
  const selections = await page.evaluate(() => window.helengineShaderDiagnostics);
  expect(selections.some(s => s.shaderAssetId === 'SpriteBatchShader' && s.variant === 'Textured')).toBe(true);
  expect(selections.some(s => s.shaderAssetId === 'SpriteBatchShader' && s.variant === 'RoundedShape')).toBe(true);
  expect(selections.some(s => s.shaderAssetId === 'ForwardStandardShader' && s.vertexEntryPoint === 'VS')).toBe(true);
  expect((await readback(page)).uiPixels).toBeGreaterThan(300);
});

test('custom b0 t0 s0 matrix and array fixture renders through the retained C++ library', async ({ page }) => {
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto('http://127.0.0.1:8197/shaders/');
  await expect.poll(() => page.evaluate(() => window.sharedShaderFrames || 0)).toBeGreaterThan(3);
  expect(await page.evaluate(() => window.sharedShaderError || '')).toBe('');
  const pixels = await readback(page);
  expect(pixels.background).toEqual([0, 0, 0, 255]);
  expect(pixels.center).toEqual([32, 64, 96, 255]);
  expect(await page.evaluate(() => window.helengineShaderDiagnostics.map(s => ({ asset: s.shaderAssetId, variant: s.variant })))).toEqual([{ asset: 'matrix_fixture', variant: 'Mesh' }]);
  expect(errors).toEqual([]);
});

test('WGPM alpha culling and depth state reach the actual engine RuntimeMaterial', async ({ page }) => {
  await page.route('**/helengine_webgpu.data', async route => {
    const response = await route.fetch(); const bytes = await response.body();
    let start = bytes.indexOf(Buffer.from('WGPM')); expect(start).toBeGreaterThanOrEqual(0);
    while (start >= 0) {
      const length = bytes.readUInt32LE(start + 12); const offset = start + 16;
      const material = JSON.parse(bytes.subarray(offset, offset + length));
      material.materialAssetId = 'state-fixture';
      Object.assign(material.renderState, { alphaMode: 'alpha-blend', doubleSided: true, depthTest: false, depthWrite: false });
      const json = Buffer.from(JSON.stringify(material)); expect(json.length).toBeLessThanOrEqual(length);
      bytes.fill(32, offset, offset + length); json.copy(bytes, offset);
      start = bytes.indexOf(Buffer.from('WGPM'), offset + length);
    }
    await route.fulfill({ response, body: bytes });
  });
  await openCube(page);
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0)).toBeGreaterThan(3);
  expect(await page.evaluate(() => window.helengineMaterialDiagnostics))
    .toContainEqual({ materialAssetId: 'state-fixture', blendMode: 1, cullMode: 0, depthTest: false, depthWrite: false });
});

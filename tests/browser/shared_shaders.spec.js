const { test, expect } = require('@playwright/test');
const { readback } = require('./render-readback');

test('shared shaders load both original UI variants and the authored Forward selection', async ({ page }) => {
  await page.goto('http://127.0.0.1:8197/game/');
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
  expect(await page.evaluate(() => window.helengineShaderDiagnostics.map(s => s.shaderAssetId))).toEqual(['matrix_fixture']);
  expect(errors).toEqual([]);
});

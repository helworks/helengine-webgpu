const { test, expect } = require('@playwright/test');
const { readback } = require('./render-readback');
test('mesh_rendering repeated draws reuse buffers textures and pipelines', async ({ page }) => {
  await page.addInitScript(() => {
    window.gpuAllocations = { createBuffer: 0, createTexture: 0, createRenderPipeline: 0 };
    const requestAdapter = navigator.gpu.requestAdapter.bind(navigator.gpu);
    navigator.gpu.requestAdapter = async (...args) => {
      const adapter = await requestAdapter(...args);
      const requestDevice = adapter.requestDevice.bind(adapter);
      adapter.requestDevice = async (...args) => {
        const device = await requestDevice(...args);
        for (const name of Object.keys(window.gpuAllocations)) {
          const create = device[name].bind(device);
          device[name] = (...args) => { window.gpuAllocations[name]++; return create(...args); };
        }
        return device;
      };
      return adapter;
    };
  });
  await page.goto('http://127.0.0.1:8197/game/');
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0)).toBeGreaterThan(30);
  const before = await page.evaluate(() => ({ counts: window.gpuAllocations, frame: window.helengineDiagnostics.frames }));
  expect(before.counts.createBuffer).toBeGreaterThan(0);
  expect(before.counts.createTexture).toBeGreaterThan(0);
  expect(before.counts.createRenderPipeline).toBeGreaterThan(0);
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics.frames)).toBeGreaterThan(before.frame + 40);
  expect(await page.evaluate(() => window.gpuAllocations)).toEqual(before.counts);
});
test('mesh_rendering draws shaded geometry and the authored UI into real GPU pixels', async ({ page }) => {
  const gpuErrors = [];
  page.on('console', message => { if (/validation|invalid|shader.*error/i.test(message.text())) gpuErrors.push(message.text()); });
  await page.goto('http://127.0.0.1:8197/game/');
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0)).toBeGreaterThan(5);
  const pixels = await readback(page);
  expect(pixels.background).toEqual([100, 149, 237, 255]);
  expect(pixels.center).not.toEqual(pixels.background);
  expect(pixels.center[3]).toBe(255);
  expect(pixels.uiPixels).toBeGreaterThan(300);
  expect(gpuErrors).toEqual([]);
});
test('mesh_rendering directional lighting changes with converted cube rotation', async ({ page }) => {
  await page.goto('http://127.0.0.1:8197/game/');
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0)).toBeGreaterThan(5);
  const brightness = [];
  for (let i = 0; i < 8; i++) { brightness.push((await readback(page)).center[0]); await page.waitForTimeout(120); }
  expect(Math.max(...brightness) - Math.min(...brightness)).toBeGreaterThan(10);
});

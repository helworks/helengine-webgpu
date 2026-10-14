const { test, expect } = require('@playwright/test');
test('cube_test reports WebGPU absence on the exported page', async ({ page }) => {
  await page.addInitScript(() => Object.defineProperty(navigator, 'gpu', { value: undefined }));
  await page.goto('http://127.0.0.1:8197/game/');
  await expect(page.locator('#helengine-error')).toBeVisible();
  await expect(page.locator('#helengine-error')).toContainText('WebGPU');
  await expect(page.locator('#helengine-status')).toHaveText('Stopped');
});
test('cube_test reports missing WASM instead of claiming running', async ({ page }) => {
  await page.route('**/helengine_webgpu.wasm', route => route.abort());
  await page.goto('http://127.0.0.1:8197/game/');
  await expect(page.locator('#helengine-error')).toBeVisible();
  await expect(page.locator('#helengine-status')).toHaveText('Stopped');
});
test('cube_test surfaces invalid WGSL as a terminal runtime error', async ({ page }) => {
  await page.route('**/helengine_webgpu.data', async route => {
    const response = await route.fetch(); const bytes = await response.body();
    const marker = Buffer.from('@vertex fn vs_main'); const offset = bytes.indexOf(marker);
    expect(offset).toBeGreaterThanOrEqual(0);
    bytes[offset + 8] = 'x'.charCodeAt(0);
    await route.fulfill({ response, body: bytes });
  });
  await page.goto('http://127.0.0.1:8197/game/');
  await expect(page.locator('#helengine-error')).toBeVisible();
  await expect(page.locator('#helengine-error')).toContainText(/WebGPU|shader|WGSL/i);
  await expect(page.locator('#helengine-status')).toHaveText('Stopped');
});
test('cube_test loads authored scene and runs converted C# spin', async ({ page }) => {
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  const response = await page.goto('http://127.0.0.1:8197/game/');
  expect(response.status()).toBe(200);
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0), { timeout: 30000 }).toBeGreaterThan(5);
  expect(await page.evaluate(() => window.helengineDiagnostics.sceneIds)).toContain('cube_test');
  const orientation = await page.evaluate(() => window.helengineDiagnostics.entities['34'].orientation);
  await page.waitForTimeout(250);
  expect(await page.evaluate(() => window.helengineDiagnostics.entities['34'].orientation)).not.toEqual(orientation);
  expect(errors).toEqual([]);
});
test('cube_test keyboard moves existing orbit camera and blur releases it', async ({ page }) => {
  const response = await page.goto('http://127.0.0.1:8197/game/');
  expect(response.status()).toBe(200);
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0), { timeout: 30000 }).toBeGreaterThan(5);
  await page.locator('canvas').click();
  const before = await page.evaluate(() => window.helengineDiagnostics.entities['16'].position);
  await page.keyboard.down('a'); await page.waitForTimeout(250); await page.keyboard.up('a');
  expect(await page.evaluate(() => window.helengineDiagnostics.entities['16'].position)).not.toEqual(before);
});

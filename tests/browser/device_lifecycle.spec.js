const { test, expect } = require('@playwright/test');
test.beforeEach(async ({ page }) => {
  await page.goto('http://127.0.0.1:8197');
  await page.waitForFunction(() => window.moduleReady);
});
test('device_lifecycle initializes a real browser WebGPU device', async ({ page }) => {
  const info = await page.evaluate(async () => {
    const adapter = await navigator.gpu.requestAdapter();
    return { vendor: adapter.info.vendor, architecture: adapter.info.architecture, device: adapter.info.device, description: adapter.info.description, fallback: adapter.info.isFallbackAdapter };
  });
  console.log('WebGPU adapter:', JSON.stringify(info));
  await page.evaluate(() => Module._he_test_start_gpu());
  await expect.poll(() => page.evaluate(() => Module._he_test_gpu_state())).toBe(2);
  expect(await page.evaluate(() => Module._he_test_resize(640, 360))).toBe(1);
});
test('device_lifecycle missing_adapter_reports_error', async ({ page }) => {
  await page.evaluate(() => { navigator.gpu.requestAdapter = async () => null; Module._he_test_start_gpu(); });
  await expect.poll(() => page.evaluate(() => Module._he_test_gpu_state())).toBe(-1);
  expect(await page.evaluate(() => Module.UTF8ToString(Module._he_test_gpu_error()))).toContain('WebGPU');
});
test('device_lifecycle zero_canvas_suspends_and_resize_resumes', async ({ page }) => {
  await page.evaluate(() => Module._he_test_start_gpu());
  await expect.poll(() => page.evaluate(() => Module._he_test_gpu_state())).toBe(2);
  expect(await page.evaluate(() => Module._he_test_resize(0, 0))).toBe(0);
  expect(await page.evaluate(() => Module._he_test_resize(320, 240))).toBe(1);
});
test('device_lifecycle late_ready_after_shutdown_is_ignored', async ({ page }) => {
  await page.evaluate(() => {
    const request = navigator.gpu.requestAdapter.bind(navigator.gpu);
    navigator.gpu.requestAdapter = () => new Promise(resolve => { window.resolveAdapter = async () => resolve(await request()); });
    Module._he_test_start_gpu(); Module._he_test_stop_gpu();
  });
  await page.evaluate(async () => window.resolveAdapter());
  await page.waitForTimeout(100);
  expect(await page.evaluate(() => Module._he_test_gpu_state())).toBe(0);
});
test('device_lifecycle device_loss_stops_presentation', async ({ page }) => {
  await page.evaluate(() => Module._he_test_start_gpu());
  await expect.poll(() => page.evaluate(() => Module._he_test_gpu_state())).toBe(2);
  await page.evaluate(() => Module._he_test_destroy_device());
  await expect.poll(() => page.evaluate(() => Module._he_test_gpu_state())).toBe(-1);
});

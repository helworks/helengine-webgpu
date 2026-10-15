const {openCube}=require('./demodisc-controls');
const { test, expect } = require('@playwright/test');
const { readback } = require('./render-readback');
async function running(page) {
  await openCube(page);
  await expect.poll(() => page.evaluate(() => window.helengineDiagnostics?.frames || 0)).toBeGreaterThan(5);
}
test('scene_input_ui resize retains actual rendered text and panel output', async ({ page }) => {
  await running(page);
  await page.setViewportSize({ width: 900, height: 700 });
  await page.waitForTimeout(100);
  const pixels = await readback(page);
  expect(pixels.width).toBe(900);
  expect(pixels.uiPixels).toBeGreaterThan(100);
  expect(await page.locator('#helengine-error').textContent()).toBe('');
});
test('scene_input_ui density change updates canvas pixels without CSS resize', async ({ page, context }) => {
  await running(page);
  const session = await context.newCDPSession(page);
  const viewport = page.viewportSize();
  const originalDensity = await page.evaluate(() => window.devicePixelRatio);
  await session.send('Emulation.setDeviceMetricsOverride', { ...viewport, deviceScaleFactor: 2, mobile: false });
  await expect.poll(() => page.evaluate(() => window.devicePixelRatio)).toBe(2);
  await expect.poll(() => page.evaluate(() => document.querySelector('canvas').width)).toBe(viewport.width * 2);
  await session.send('Emulation.clearDeviceMetricsOverride');
  await expect.poll(() => page.evaluate(() => window.devicePixelRatio)).toBe(originalDensity);
  // Clearing CDP also clears Playwright's original viewport override; use the restored CSS extent.
  await expect.poll(() => page.evaluate(() => {
    const canvas = document.querySelector('canvas'); return canvas.width === Math.round(canvas.clientWidth * window.devicePixelRatio);
  })).toBe(true);
  expect(await page.locator('#helengine-error').textContent()).toBe('');
});
test('scene_input_ui keydown_then_blur_releases_key', async ({ page }) => {
  await running(page); await page.locator('canvas').click();
  await page.keyboard.down('a'); await page.waitForTimeout(100);
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  await page.waitForTimeout(50);
  const afterBlur = await page.evaluate(() => window.helengineDiagnostics.entities['16'].position);
  await page.waitForTimeout(150);
  expect(await page.evaluate(() => window.helengineDiagnostics.entities['16'].position)).toEqual(afterBlur);
  await page.keyboard.up('a');
});
test('scene_input_ui hidden_tab_resets_clock_without_simulation_jump', async ({ page }) => {
  // Headless Chrome keeps tabs visible; model the browser's visibility event and suspended RAF boundary.
  await page.addInitScript(() => {
    window.simulatedHidden = false;
    Object.defineProperty(document, 'hidden', { configurable: true, get: () => window.simulatedHidden });
    const original = window.requestAnimationFrame.bind(window);
    window.resumeAnimation = () => { const callback = window.suspendedAnimation; window.suspendedAnimation = null; if (callback) original(callback); };
    window.requestAnimationFrame = callback => original(timestamp => {
      if (window.simulatedHidden) window.suspendedAnimation = callback;
      else callback(timestamp);
    });
  });
  await running(page);
  const orientation = await page.evaluate(() => window.helengineDiagnostics.entities['34'].orientation);
  await page.evaluate(() => { window.simulatedHidden = true; document.dispatchEvent(new Event('visibilitychange')); });
  await expect.poll(() => page.evaluate(() => Boolean(window.suspendedAnimation))).toBe(true);
  await page.waitForTimeout(800);
  await page.evaluate(() => { window.simulatedHidden = false; document.dispatchEvent(new Event('visibilitychange')); window.resumeAnimation(); });
  await page.waitForTimeout(50);
  const resumed = await page.evaluate(() => window.helengineDiagnostics.entities['34'].orientation);
  const dot = Math.abs(orientation.reduce((sum, value, index) => sum + value * resumed[index], 0));
  expect(dot).toBeGreaterThan(0.97);
});

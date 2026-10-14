const { test, expect } = require('@playwright/test');
test.use({ deviceScaleFactor: 2 });
test.beforeEach(async ({ page }) => {
  await page.goto('http://127.0.0.1:8197'); await page.waitForFunction(() => window.moduleReady);
  await page.evaluate(() => {
    const canvas = document.getElementById('helengine-canvas');
    canvas.tabIndex = 0; canvas.style.width = '320px'; canvas.style.height = '180px';
    const header = document.createElement('header'); header.style.height = '60px'; document.body.prepend(header);
    Module._he_test_start_input(); canvas.focus();
  });
});
test('native input mouse release preserves canvas coordinates with header and DPI', async ({ page }) => {
  const rect = await page.locator('canvas').boundingBox();
  await page.mouse.move(rect.x + 40, rect.y + 50); await page.mouse.down();
  const pressed = await page.evaluate(() => [Module._he_test_mouse_x(), Module._he_test_mouse_y()]);
  expect(pressed).toEqual([80, 100]);
  await page.mouse.up();
  expect(await page.evaluate(() => [Module._he_test_mouse_x(), Module._he_test_mouse_y()])).toEqual(pressed);
});
test('native input maps left modifier and does not duplicate held key state', async ({ page }) => {
  await page.keyboard.down('ShiftLeft');
  expect(await page.evaluate(() => Module._he_test_key_down(160))).toBe(1);
  await page.keyboard.down('ShiftLeft');
  expect(await page.evaluate(() => Module._he_test_key_down(160))).toBe(1);
  await page.keyboard.up('ShiftLeft');
  expect(await page.evaluate(() => Module._he_test_key_down(160))).toBe(0);
});

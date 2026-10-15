const { test, expect } = require('@playwright/test');

test('native light selection and shadow projections preserve desktop contracts', async ({ page }) => {
  await page.goto('http://127.0.0.1:8197/shaders/lights.html');
  await expect.poll(() => page.evaluate(() => window.lightStateChecks)).toBe('passed');
});

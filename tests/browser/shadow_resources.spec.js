const { test, expect } = require('@playwright/test');

test('depth atlas and all six cube faces compare actual raster depth', async ({ page }) => {
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.goto('http://127.0.0.1:8197/shaders/shadows.html');
  await expect.poll(() => page.evaluate(() => window.shadowResourceError || window.shadowResourceFrames || 0)).toBeGreaterThan(2);
  const samples = await page.evaluate(async () => {
    const canvas = document.querySelector('canvas'); const context = canvas.getContext('webgpu'); const config = context.getConfiguration();
    context.configure({ ...config, usage: config.usage | GPUTextureUsage.COPY_SRC });
    await new Promise(requestAnimationFrame);
    const device = config.device; const buffer = device.createBuffer({ size: 2560 * 360, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
    const encoder = device.createCommandEncoder(); encoder.copyTextureToBuffer({ texture: context.getCurrentTexture() }, { buffer, bytesPerRow: 2560 }, { width: 640, height: 360 });
    device.queue.submit([encoder.finish()]); await buffer.mapAsync(GPUMapMode.READ); const bytes = new Uint8Array(buffer.getMappedRange());
    const values = Array.from({ length: 18 }, (_, index) => {
      const offset = 180 * 2560 + Math.floor((index + 0.5) * 640 / 18) * 4;
      return config.format.startsWith('bgra') ? [bytes[offset + 2], bytes[offset + 1], bytes[offset], bytes[offset + 3]] : Array.from(bytes.subarray(offset, offset + 4));
    }); buffer.unmap(); buffer.destroy(); return values;
  });
  for (let index = 0; index < 18; index++) {
    const reference = [0.1, 0.5, 0.9][index % 3]; const depth = Math.floor(index / 3) % 2 ? 0.75 : 0.25;
    expect(samples[index]).toEqual([reference <= 0.25 ? 255 : 0, reference <= depth ? 255 : 0, 0, 255]);
  }
  expect(await page.evaluate(() => window.shadowPipelineCache)).toEqual({ reusesSameTarget: true, separatesDepthFormat: true, separatesBias: true });
  expect(errors).toEqual([]);
});

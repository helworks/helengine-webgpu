/** Reads numeric GPU pixels directly; never captures or saves a screenshot. */
async function readback(page) {
  return page.evaluate(async () => {
    const canvas = document.querySelector('canvas');
    const context = canvas.getContext('webgpu');
    const configuration = context.getConfiguration();
    context.configure({ ...configuration, usage: configuration.usage | GPUTextureUsage.COPY_SRC });
    await new Promise(requestAnimationFrame);
    const device = configuration.device;
    const rowPitch = Math.ceil(canvas.width * 4 / 256) * 256;
    const buffer = device.createBuffer({ size: rowPitch * canvas.height, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
    const encoder = device.createCommandEncoder();
    encoder.copyTextureToBuffer({ texture: context.getCurrentTexture() }, { buffer, bytesPerRow: rowPitch }, { width: canvas.width, height: canvas.height });
    device.queue.submit([encoder.finish()]); await buffer.mapAsync(GPUMapMode.READ);
    const bytes = new Uint8Array(buffer.getMappedRange());
    function pixel(x, y) {
      const offset = y * rowPitch + x * 4;
      return configuration.format.startsWith('bgra') ? [bytes[offset + 2], bytes[offset + 1], bytes[offset], bytes[offset + 3]] : Array.from(bytes.subarray(offset, offset + 4));
    }
    const background = pixel(2, 2);
    const center = pixel(Math.floor(canvas.width / 2), Math.floor(canvas.height / 2));
    let uiPixels = 0;
    for (let y = Math.floor(canvas.height * 0.55); y < canvas.height; y += 3) {
      for (let x = 0; x < Math.floor(canvas.width * 0.4); x += 3) {
        if (pixel(x, y).some((value, index) => Math.abs(value - background[index]) > 10)) uiPixels++;
      }
    }
    buffer.unmap(); buffer.destroy();
    return { background, center, uiPixels, width: canvas.width, height: canvas.height };
  });
}
module.exports = { readback };

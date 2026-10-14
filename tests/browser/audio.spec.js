const { test, expect } = require('@playwright/test');
test.beforeEach(async ({ page }) => {
  await page.addInitScript(() => {
    const NativeAudioContext = window.AudioContext;
    window.audioProbe = { gains: [], buffers: [], context: null };
    window.AudioContext = class extends NativeAudioContext {
      constructor(...args) { super(...args); window.audioProbe.context = this; }
      createGain(...args) { const node = super.createGain(...args); window.audioProbe.gains.push(node); return node; }
      createBuffer(...args) { const buffer = super.createBuffer(...args); window.audioProbe.buffers.push(buffer); return buffer; }
    };
  });
  await page.goto('http://127.0.0.1:8197');
  await page.waitForFunction(() => window.moduleReady);
  await page.addScriptTag({ url: 'http://127.0.0.1:8197/game/audio.js' });
  await page.evaluate(() => { const button = document.createElement('button'); button.textContent = 'Unlock'; button.onclick = () => window.helengineAudio.unlock(); document.body.append(button); });
  await page.getByText('Unlock', { exact: true }).click();
});
test('audio native backend accepts the prepared pcm-buffered asset contract', async ({ page }) => {
  const result = await page.evaluate(() => ({ voice: Module._he_test_prepared_pcm(), error: Module.UTF8ToString(Module._he_test_audio_error()) }));
  expect(result.error).toBe(''); expect(result.voice).toBeGreaterThan(0);
  expect(await page.evaluate(id => window.helengineAudio.isPlaying(id), result.voice)).toBe(true);
});
test('audio gesture unlocks real Web Audio context and bus gain reaches its node', async ({ page }) => {
  expect(await page.evaluate(() => window.audioProbe.context.state)).toBe('running');
  const values = await page.evaluate(() => {
    window.helengineAudio.play(new Uint8Array(1600), 1, 8000, 'gain-test', 0.5, true, 0);
    window.helengineAudio.setBusGain('gain-test', 0.25);
    return window.audioProbe.gains.map(node => node.gain.value);
  });
  expect(values).toEqual([0.25, 0.5]);
});
test('audio bus identity ignores case like the engine AudioManager', async ({ page }) => {
  const values = await page.evaluate(() => {
    window.helengineAudio.play(new Uint8Array(1600), 1, 8000, 'MUSIC', 1, true, 0);
    window.helengineAudio.setBusGain('music', 0.25);
    return window.audioProbe.gains.map(node => node.gain.value);
  });
  expect(values).toEqual([0.25, 1]);
});
test('audio copies PCM before WASM memory growth detaches the input heap', async ({ page }) => {
  const samples = await page.evaluate(() => {
    const memory = new WebAssembly.Memory({ initial: 1, maximum: 2 });
    const bytes = new Uint8Array(memory.buffer, 0, 1600);
    const pcm = new DataView(memory.buffer); pcm.setInt16(0, 8192, true); pcm.setInt16(2, -16384, true);
    window.helengineAudio.play(bytes, 2, 8000, 'heap-growth', 1, true, 0);
    memory.grow(1);
    const buffer = window.audioProbe.buffers[0];
    return { oldLength: bytes.byteLength, left: buffer.getChannelData(0)[0], right: buffer.getChannelData(1)[0] };
  });
  expect(samples).toEqual({ oldLength: 0, left: 0.25, right: -0.5 });
});
test('audio PCM voice stops and reports completion', async ({ page }) => {
  const id = await page.evaluate(() => window.helengineAudio.play(new Uint8Array(1600), 1, 8000, 'effects', 1, false, 0));
  expect(await page.evaluate(id => window.helengineAudio.isPlaying(id), id)).toBe(true);
  await page.waitForTimeout(200);
  expect(await page.evaluate(id => window.helengineAudio.isPlaying(id), id)).toBe(false);
});
test('audio loop stays active until stop', async ({ page }) => {
  const id = await page.evaluate(() => window.helengineAudio.play(new Uint8Array(160), 1, 8000, 'effects', 1, true, 0));
  await page.waitForTimeout(150);
  expect(await page.evaluate(id => window.helengineAudio.isPlaying(id), id)).toBe(true);
  await page.evaluate(id => window.helengineAudio.stop(id), id);
  expect(await page.evaluate(id => window.helengineAudio.isPlaying(id), id)).toBe(false);
});
test('audio bus pause preserves voice and resume reaches completion', async ({ page }) => {
  const id = await page.evaluate(() => { const audio = window.helengineAudio; const id = audio.play(new Uint8Array(3200), 1, 8000, 'music', 0.5, false, 0); audio.setBusPaused('music', true); audio.setBusGain('music', 0.25); return id; });
  await page.waitForTimeout(250);
  expect(await page.evaluate(id => window.helengineAudio.isPlaying(id), id)).toBe(true);
  await page.evaluate(() => window.helengineAudio.setBusPaused('music', false));
  await page.waitForTimeout(300);
  expect(await page.evaluate(id => window.helengineAudio.isPlaying(id), id)).toBe(false);
});
test('audio rejects malformed PCM without manufacturing a voice', async ({ page }) => {
  expect(await page.evaluate(() => { try { window.helengineAudio.play(new Uint8Array(3), 2, 8000, 'effects', 1, false, 0); return false; } catch { return true; } })).toBe(true);
});

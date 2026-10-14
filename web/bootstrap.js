(() => {
  const canvas = document.getElementById('helengine-canvas');
  const error = document.getElementById('helengine-error');
  function fail(message) { error.textContent = message; error.hidden = false; document.getElementById('helengine-status').textContent = 'Stopped'; }
  window.addEventListener('error', event => fail(event.message));
  window.addEventListener('unhandledrejection', event => fail(String(event.reason)));
  document.getElementById('helengine-start').addEventListener('click', async event => {
    try { await window.helengineAudio.unlock(); event.target.textContent = 'Audio enabled'; event.target.disabled = true; canvas.focus(); }
    catch (reason) { fail(String(reason)); }
  });
  function resize() {
    const density = window.devicePixelRatio || 1;
    canvas.width = Math.round(canvas.clientWidth * density);
    canvas.height = Math.round(canvas.clientHeight * density);
  }
  resize(); new ResizeObserver(resize).observe(canvas);
  window.addEventListener('resize', resize);
  canvas.addEventListener('pointerdown', () => canvas.focus());
  canvas.addEventListener('contextmenu', event => event.preventDefault());
  window.addEventListener('pagehide', () => { if (window.helengine) window.helengine._he_shutdown(); window.helengineAudio.close(); });
  if (!navigator.gpu) { fail('This browser or device does not support WebGPU.'); return; }
  createHelEngine({ canvas, onAbort: reason => fail(String(reason)), printErr: message => console.error(message),
    monitorRunDependencies: remaining => { if (remaining) document.getElementById('helengine-status').textContent = 'Loading game content…'; }
  }).then(module => { window.helengine = module; }).catch(reason => fail(String(reason)));
})();

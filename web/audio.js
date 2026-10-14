(() => {
  let context;
  let nextVoice = 1;
  const voices = new Map();
  const buses = new Map();
  function audioContext() { if (!context) context = new AudioContext(); return context; }
  function bus(id) {
    if (!id) throw new Error('Audio bus id is required.');
    id = id.toLowerCase();
    if (!buses.has(id)) {
      const node = audioContext().createGain(); node.connect(context.destination);
      buses.set(id, { node, paused: false });
    }
    return buses.get(id);
  }
  function disconnect(voice) {
    if (!voice.source) return;
    voice.source.onended = null;
    voice.source.stop(); voice.source.disconnect(); voice.gain.disconnect(); voice.source = null;
  }
  function start(voice) {
    if (voice.bus.paused) return;
    if (voice.offset >= voice.buffer.duration) {
      if (!voice.loop) { voices.delete(voice.id); return; }
      voice.offset %= voice.buffer.duration;
    }
    const source = context.createBufferSource(); const gain = context.createGain();
    source.buffer = voice.buffer; source.loop = voice.loop; gain.gain.value = voice.volume;
    source.connect(gain); gain.connect(voice.bus.node);
    voice.source = source; voice.gain = gain; voice.startedAt = context.currentTime;
    source.onended = () => {
      if (voice.source !== source) return;
      source.disconnect(); gain.disconnect(); voice.source = null; voices.delete(voice.id);
    };
    source.start(0, voice.offset);
  }
  window.helengineAudio = {
    async unlock() { await audioContext().resume(); },
    play(bytes, channels, sampleRate, busId, volume, loop, offset) {
      if (!(bytes instanceof Uint8Array) || !bytes.length || channels < 1 || channels > 2 || !Number.isInteger(channels) || bytes.length % (channels * 2)) throw new Error('Audio requires complete mono/stereo PCM16 frames.');
      if (!Number.isFinite(volume) || volume < 0 || !Number.isFinite(offset) || offset < 0 || !Number.isInteger(sampleRate) || sampleRate <= 0) throw new Error('Invalid audio playback parameters.');
      const target = bus(busId); const frames = bytes.length / (channels * 2);
      const buffer = audioContext().createBuffer(channels, frames, sampleRate);
      const copy = bytes.slice(); const pcm = new DataView(copy.buffer, copy.byteOffset, copy.byteLength);
      for (let channel = 0; channel < channels; channel++) {
        const samples = buffer.getChannelData(channel);
        for (let frame = 0; frame < frames; frame++) samples[frame] = pcm.getInt16((frame * channels + channel) * 2, true) / 32768;
      }
      const voice = { id: nextVoice++, bus: target, buffer, volume, loop: Boolean(loop), offset, source: null };
      voices.set(voice.id, voice); start(voice); return voice.id;
    },
    stop(id) { const voice = voices.get(id); if (voice) { disconnect(voice); voices.delete(id); } },
    isPlaying(id) { return voices.has(id); },
    setBusGain(id, gain) {
      if (!Number.isFinite(gain) || gain < 0) throw new Error('Bus gain must be finite and nonnegative.');
      bus(id).node.gain.value = gain;
    },
    setBusPaused(id, paused) {
      const target = bus(id); if (target.paused === paused) return; target.paused = paused;
      for (const voice of voices.values()) {
        if (voice.bus !== target) continue;
        if (paused && voice.source) { voice.offset += context.currentTime - voice.startedAt; disconnect(voice); }
        else if (!paused) start(voice);
      }
    },
    close() { for (const id of [...voices.keys()]) this.stop(id); buses.clear(); if (context) { const closing = context.close(); context = null; return closing; } }
  };
})();

// Deterministic WebAudio platform fixture: no speaker, display, or wall clock.
globalThis.audioTestTimers = new Map();
let audioNextTimer = 1;
globalThis.setInterval = fn => { const id = audioNextTimer++; audioTestTimers.set(id, fn); return id; };
globalThis.clearInterval = id => audioTestTimers.delete(id);
class AudioTestParam {
  constructor() { this.value = 0; }
  setValueAtTime(value) { this.value = value; }
}
// Every buffer source the page creates, so a test can see what played.
globalThis.audioTestSources = [];
class AudioTestNode {
  constructor() { this.gain = new AudioTestParam(); this.pan = new AudioTestParam(); this.playbackRate = new AudioTestParam(); this.started = false; this.stopped = false; }
  connect() {}
  disconnect() {}
  start() { this.started = true; }
  stop() { this.stopped = true; }
}
globalThis.audioTestLive = () => audioTestSources.filter(source => source.started && !source.stopped);
class AudioTestBuffer {
  constructor(channels, frames, rate) {
    this.numberOfChannels = channels; this.length = frames; this.sampleRate = rate;
    this.duration = frames / rate;
    this.channels = Array.from({length: channels}, () => new Float32Array(frames));
  }
  getChannelData(index) { return this.channels[index]; }
}
globalThis.AudioContext = class {
  constructor() { this.sampleRate = 44100; this.currentTime = 0; this.state = 'running'; this.destination = {}; }
  createGain() { return new AudioTestNode(); }
  createStereoPanner() { return new AudioTestNode(); }
  createBufferSource() { const node = new AudioTestNode(); audioTestSources.push(node); return node; }
  createBuffer(channels, frames, rate) { return new AudioTestBuffer(channels, frames, rate); }
  decodeAudioData(_bytes, resolve) {
    const buffer = new AudioTestBuffer(1, 44100, 44100);
    // The promise form takes only the bytes; the callbacks are optional.
    if (resolve) resolve(buffer);
    return Promise.resolve(buffer);
  }
  resume() { return Promise.resolve(); }
  close() { this.state = 'closed'; globalThis.audioTestClosed = true; return Promise.resolve(); }
};
globalThis.OfflineAudioContext = globalThis.AudioContext;

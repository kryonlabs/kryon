addToLibrary({
  audio_test_checkpoint__deps: ['$FS'],
  audio_test_checkpoint: function(phase) {
    const A = globalThis.__kryonAudio;
    if (phase === 1) {
      for (const callback of Array.from(audioTestTimers.values())) callback();
      const sources = Object.values(A.sources);
      if (sources.length !== 1) return 1;
      const source = sources[0].source;
      if (source.buffer.length !== 8 || source.buffer.getChannelData(0)[0] !== 0.25) return 2;
      source.onended();
      if (Object.keys(A.sources).length) return 3;
    }
    if (phase === 2) {
      if (Object.keys(A.buffers).length || Object.keys(A.sources).length ||
          Object.keys(A.timers).length || audioTestTimers.size) return 4;
      const code = FS.readFile('/tmp/audio.h', {encoding: 'utf8'});
      if (!code.includes('wave_frame_count = 4;') || !code.includes('0x00, 0x80')) return 5;
    }
    if (phase === 3 && (A || !globalThis.audioTestClosed)) return 6;
    if (phase === 4) {
      const sources = Object.values(A.sources);
      if (sources.length !== 2) return 7;
      for (const record of sources) record.source.onended();
      if (Object.keys(A.sources).length) return 8;
    }
    return 0;
  }
});

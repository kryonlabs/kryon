addToLibrary({
  audio_test_checkpoint__deps: ['$FS'],
  audio_test_checkpoint: function(phase) {
    const A = globalThis.__kryonAudio;
    if (phase === 1) {
      for (const callback of Array.from(audioTestTimers.values())) callback();
      const stream = Object.values(A.streams)[0];
      if (stream.sources.length !== 1) return 1;
      const source = stream.sources[0].source;
      if (source.buffer.length !== 8 || source.buffer.getChannelData(0)[0] !== 0.25) return 2;
      source.onended();
      if (stream.sources.length) return 3;
    }
    if (phase === 2) {
      if (Object.keys(A.buffers).length || Object.keys(A.streams).length ||
          A.mixedProcessors.length || audioTestTimers.size) return 4;
      const code = FS.readFile('/tmp/audio.h', {encoding: 'utf8'});
      if (!code.includes('wave_frame_count = 4;') || !code.includes('0x00, 0x80')) return 5;
    }
    if (phase === 3 && (A || !globalThis.audioTestClosed)) return 6;
    if (phase === 4) {
      const stream = Object.values(A.streams)[0];
      if (stream.sources.length !== 2) return 7;
      for (const source of Array.from(stream.sources)) source.source.onended();
      if (stream.sources.length) return 8;
    }
    return 0;
  }
});

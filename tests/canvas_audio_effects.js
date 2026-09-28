addToLibrary({
  audio_test_checkpoint__deps: ['$FS'],
  audio_test_checkpoint: function(phase) {
    const live = audioTestLive();
    if (phase === 1) {
      for (const callback of Array.from(audioTestTimers.values())) callback();
      const playing = audioTestLive();
      if (playing.length !== 1) return 1;
      const source = playing[0];
      if (source.buffer.length !== 8 || source.buffer.getChannelData(0)[0] !== 0.25) return 2;
      // The browser reports the block finished; the audio code notices.
      source.onended();
    }
    if (phase === 2) {
      if (live.length || audioTestTimers.size) return 4;
      const code = FS.readFile('/tmp/audio.h', {encoding: 'utf8'});
      if (!code.includes('wave_frame_count = 4;') || !code.includes('0x00, 0x80')) return 5;
    }
    if (phase === 3 && (live.length || !globalThis.audioTestClosed)) return 6;
    if (phase === 4) {
      if (live.length !== 2) return 7;
      for (const source of live) source.onended();
    }
    return 0;
  }
});

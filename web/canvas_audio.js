// Raw WebAudio effects. Wave parsing, conversion, and public behavior live in Zi.
addToLibrary({
  js_audio_init__deps: ['malloc', 'free', '$getWasmTableEntry', '$FS'],
  js_audio_init: function() {
    var g = globalThis;
    if (g.__kryonAudio && g.__kryonAudio.version === 1) return;
    var A = g.__kryonAudio = {
        version: 1, ctx: null, master: null, masterVolume: 1.0,
        nextId: 1, buffers: {}, streams: {}, mixedProcessors: []
    };
    A.clamp01 = function (v) {
        v = Number(v);
        return isFinite(v) ? Math.max(0.0, Math.min(1.0, v)) : 1.0;
    };
    A.clampPan = function (v) {
        v = Number(v);
        return isFinite(v) ? Math.max(-1.0, Math.min(1.0, v)) : 0.0;
    };
    A.rate = function (v) {
        v = Number(v);
        return isFinite(v) && v > 0.01 ? Math.max(0.01, Math.min(16.0, v)) : 1.0;
    };
    A.ensure = function () {
        if (A.ctx) return A.ctx;
        var AC = g.AudioContext || g.webkitAudioContext;
        if (!AC) return null;
        try {
            A.ctx = new AC();
            A.master = A.ctx.createGain ? A.ctx.createGain() : null;
            if (A.master) {
                if (A.master.gain && A.master.gain.setValueAtTime)
                    A.master.gain.setValueAtTime(A.masterVolume, A.ctx.currentTime || 0);
                else if (A.master.gain)
                    A.master.gain.value = A.masterVolume;
                A.master.connect(A.ctx.destination);
            }
            return A.ctx;
        } catch (e) {
            return null;
        }
    };
    A.resume = function () {
        var ctx = A.ensure();
        if (ctx && ctx.state === 'suspended' && ctx.resume) {
            try {
                var p = ctx.resume();
                if (p && p.catch) p.catch(function () {});
            } catch (e) {}
        }
    };
    A.readFileBytes = async function (path) {
        if (typeof FS !== 'undefined' && FS.readFile) {
            try { return FS.readFile(path, {encoding: 'binary'}); } catch (e) {}
        }
        if (typeof fetch !== 'undefined') {
            var response = await fetch(path);
            if (!response.ok) throw new Error('audio fetch failed: ' + path);
            return new Uint8Array(await response.arrayBuffer());
        }
        throw new Error('no audio file source');
    };
    A.decoder = function () {
        /* decodeAudioData on an autoplay-suspended AudioContext never
         * settles in some browsers (Firefox parks the promise until a
         * gesture), and an Asyncify build blocks its whole C stack on
         * the await. Decoding on a plain OfflineAudioContext has no such
         * gate; build it at the main context's rate so buffers play
         * untransposed. Falls back to the main context when offline
         * contexts are unavailable. */
        if (A.decodeCtx) return A.decodeCtx;
        var ctx = A.ensure();
        var OC = g.OfflineAudioContext || g.webkitOfflineAudioContext;
        if (OC && ctx) {
            try {
                A.decodeCtx = new OC(1, 1, ctx.sampleRate || 44100);
                return A.decodeCtx;
            } catch (e) {}
        }
        return ctx;
    };
    A.decodeBytes = async function (bytes) {
        var ctx = A.decoder();
        if (!ctx || !ctx.decodeAudioData) return 0;
        A.resume();
        var copy = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
        var audioBuffer = await new Promise(function (resolve, reject) {
            var r = ctx.decodeAudioData(copy, resolve, reject);
            if (r && r.then) r.then(resolve, reject);
        });
        return A.storeBuffer(audioBuffer);
    };
    A.storeBuffer = function (audioBuffer) {
        if (!audioBuffer) return 0;
        var id = A.nextId++;
        A.buffers[id] = {
            id: id, ref: 1, buffer: audioBuffer, sources: [],
            volume: 1.0, pitch: 1.0, pan: 0.0, paused: false,
            pausedOffset: 0, music: null
        };
        return id;
    };
    A.setParam = function (param, value) {
        if (!param) return;
        if (param.setValueAtTime) param.setValueAtTime(value, A.ctx ? A.ctx.currentTime || 0 : 0);
        else param.value = value;
    };
    A.connectOutput = function (volume, pan) {
        var ctx = A.ensure();
        if (!ctx) return null;
        var gain = ctx.createGain ? ctx.createGain() : null;
        var tail = gain;
        if (gain) A.setParam(gain.gain, A.clamp01(volume));
        if (ctx.createStereoPanner) {
            var panner = ctx.createStereoPanner();
            A.setParam(panner.pan, A.clampPan(pan));
            if (gain) gain.connect(panner);
            tail = panner;
        }
        if (tail) tail.connect(A.master || ctx.destination);
        return {input: gain || tail || (A.master || ctx.destination),
                gain: gain, panner: tail !== gain ? tail : null};
    };
    A.stopSource = function (source) {
        if (!source) return;
        try { source.onended = null; } catch (e) {}
        try { source.stop(0); } catch (e) {}
        try { source.disconnect(); } catch (e) {}
    };
    A.disconnectOutput = function (output) {
        if (!output) return;
        [output.gain, output.panner].forEach(function (node) {
            try { if (node) node.disconnect(); } catch (e) {}
        });
    };
    A.sampleBytes = function (sampleSize) {
        return sampleSize === 8 ? 1 : sampleSize === 16 ? 2 : 4;
    };
    A.callAudioCallback = function (callback, ptr, frames) {
        if (!callback || !ptr || frames <= 0) return;
        try {
            getWasmTableEntry(callback)(ptr, frames);
        } catch (e) {}
    };
    A.processStreamBuffer = function (st, ptr, frames) {
        if (!st || !ptr || frames <= 0) return;
        (st.processors || []).forEach(function (callback) {
            A.callAudioCallback(callback, ptr, frames);
        });
        A.mixedProcessors.forEach(function (callback) {
            A.callAudioCallback(callback, ptr, frames);
        });
    };
    A.startStreamCallback = function (st) {
        if (!st || !st.callback || st.callbackTimer) return;
        var interval = Math.max(8, Math.floor((st.callbackFrames || 1024) /
                                             (st.sampleRate || 44100) * 500));
        st.callbackTimer = setInterval(function () {
            if (!st.playing || !st.callback || st.sources.length >= 2) return;
            var frames = st.callbackFrames || 1024;
            var bytes = frames * (st.channels || 2) * A.sampleBytes(st.sampleSize || 32);
            var ptr = _malloc(bytes);
            if (!ptr) return;
            try {
                HEAPU8.fill(0, ptr, ptr + bytes);
                A.callAudioCallback(st.callback, ptr, frames);
                A.streamPush(st.id, ptr, frames, st.sampleRate, st.sampleSize, st.channels);
            } finally {
                _free(ptr);
            }
        }, interval);
    };
    A.stopStreamCallback = function (st) {
        if (!st || !st.callbackTimer) return;
        clearInterval(st.callbackTimer);
        st.callbackTimer = 0;
    };
    A.prune = function (entry) {
        if (entry) entry.sources = entry.sources.filter(function (s) { return !s.done; });
    };
    A.playSound = function (id, offset) {
        var entry = A.buffers[id], ctx = A.ensure();
        if (!entry || !ctx || !ctx.createBufferSource) return 0;
        A.resume();
        var source = ctx.createBufferSource();
        source.buffer = entry.buffer;
        A.setParam(source.playbackRate, A.rate(entry.pitch));
        var output = A.connectOutput(entry.volume, entry.pan);
        source.connect(output ? output.input : (A.master || ctx.destination));
        var state = {
            source: source, started: ctx.currentTime || 0,
            offset: Math.max(0, offset || 0), done: false,
            output: output
        };
        source.onended = function () {
            state.done = true;
            A.stopSource(source);
            A.disconnectOutput(output);
            A.prune(entry);
        };
        entry.sources.push(state);
        try { source.start(0, state.offset); } catch (e) { state.done = true; return 0; }
        return 1;
    };
    A.stopSound = function (id) {
        var entry = A.buffers[id];
        if (!entry) return;
        entry.sources.forEach(function (s) {
            s.done = true;
            A.stopSource(s.source);
            A.disconnectOutput(s.output);
        });
        entry.sources = [];
    };
    A.pauseSound = function (id) {
        var entry = A.buffers[id], ctx = A.ensure();
        if (!entry || !ctx) return;
        A.prune(entry);
        entry.pausedOffset = 0;
        if (entry.sources.length) {
            var s = entry.sources[0];
            entry.pausedOffset = s.offset + Math.max(0, (ctx.currentTime || 0) - s.started) * A.rate(entry.pitch);
        }
        A.stopSound(id);
        entry.paused = true;
    };
    A.resumeSound = function (id) {
        var entry = A.buffers[id];
        if (!entry || !entry.paused) return;
        entry.paused = false;
        A.playSound(id, entry.pausedOffset || 0);
    };
    A.playMusic = function (id, looping) {
        var entry = A.buffers[id], ctx = A.ensure();
        if (!entry || !ctx || !ctx.createBufferSource) return 0;
        A.resume();
        if (entry.music && entry.music.source) {
            A.stopSource(entry.music.source);
            A.disconnectOutput(entry.music.output);
        }
        var source = ctx.createBufferSource();
        var music = entry.music || {};
        source.buffer = entry.buffer;
        source.loop = !!looping;
        A.setParam(source.playbackRate, A.rate(entry.pitch));
        var output = A.connectOutput(entry.volume, entry.pan);
        source.connect(output ? output.input : (A.master || ctx.destination));
        music.source = source;
        music.output = output;
        music.offset = Math.max(0, music.offset || 0);
        music.started = ctx.currentTime || 0;
        music.playing = true;
        music.looping = !!looping;
        source.onended = function () {
            if (entry.music === music && !music.looping) {
                music.playing = false;
                A.stopSource(source);
                A.disconnectOutput(output);
            }
        };
        entry.music = music;
        try { source.start(0, music.offset); } catch (e) { music.playing = false; return 0; }
        return 1;
    };
    A.musicOffset = function (entry) {
        var ctx = A.ensure(), m = entry ? entry.music : null;
        if (!entry || !m) return 0;
        if (m.playing && ctx)
            return m.offset + Math.max(0, (ctx.currentTime || 0) - m.started) * A.rate(entry.pitch);
        return m.offset || 0;
    };
    A.pauseMusic = function (id) {
        var entry = A.buffers[id];
        if (!entry || !entry.music) return;
        entry.music.offset = A.musicOffset(entry);
        entry.music.playing = false;
        A.stopSource(entry.music.source);
        A.disconnectOutput(entry.music.output);
        entry.music.source = null;
    };
    A.stopMusic = function (id) {
        var entry = A.buffers[id];
        if (!entry) return;
        if (entry.music) {
            A.stopSource(entry.music.source);
            A.disconnectOutput(entry.music.output);
        }
        entry.music = {offset: 0, playing: false, source: null, looping: true};
    };
    A.seekMusic = function (id, seconds) {
        var entry = A.buffers[id];
        if (!entry) return;
        var wasPlaying = entry.music && entry.music.playing;
        var looping = entry.music ? entry.music.looping : true;
        A.pauseMusic(id);
        if (!entry.music) entry.music = {};
        entry.music.offset = Math.max(0, Math.min(seconds, entry.buffer.duration || 0));
        if (wasPlaying) A.playMusic(id, looping);
    };
    A.streamCreate = function (sampleRate, sampleSize, channels) {
        var ctx = A.ensure();
        if (!ctx) return 0;
        var id = A.nextId++;
        A.streams[id] = {
            id: id, sampleRate: sampleRate || ctx.sampleRate || 44100,
            sampleSize: sampleSize || 32, channels: channels || 2,
            playing: false, nextTime: 0, volume: 1.0, pitch: 1.0,
            pan: 0.0, sources: [], callback: 0, callbackFrames: 1024,
            callbackTimer: 0, processors: []
        };
        return id;
    };
    A.streamPush = function (id, ptr, frames, sampleRate, sampleSize, channels) {
        var st = A.streams[id], ctx = A.ensure();
        if (!st || !ctx || !st.playing || frames <= 0 || st.sources.length >= 2) return 0;
        channels = channels || st.channels || 2;
        sampleSize = sampleSize || st.sampleSize || 32;
        sampleRate = sampleRate || st.sampleRate || ctx.sampleRate || 44100;
        A.processStreamBuffer(st, ptr, frames);
        var buffer = ctx.createBuffer(channels, frames, sampleRate);
        for (var ch = 0; ch < channels; ch++) {
            var dst = buffer.getChannelData(ch);
            for (var i = 0; i < frames; i++) {
                var idx = i * channels + ch;
                var v = sampleSize === 8 ? (HEAPU8[ptr + idx] - 128) / 128.0 :
                        sampleSize === 16 ? HEAP16[(ptr >> 1) + idx] / 32768.0 :
                        HEAPF32[(ptr >> 2) + idx];
                dst[i] = Math.max(-1, Math.min(1, v));
            }
        }
        var source = ctx.createBufferSource();
        source.buffer = buffer;
        A.setParam(source.playbackRate, A.rate(st.pitch));
        var output = A.connectOutput(st.volume, st.pan);
        source.connect(output ? output.input : (A.master || ctx.destination));
        st.nextTime = Math.max(st.nextTime || 0, ctx.currentTime || 0);
        try { source.start(st.nextTime); } catch (e) { return 0; }
        var scheduled = {source: source, output: output};
        st.sources.push(scheduled);
        source.onended = function () {
            A.stopSource(source);
            A.disconnectOutput(output);
            var index = st.sources.indexOf(scheduled);
            if (index >= 0) st.sources.splice(index, 1);
        };
        st.nextTime += buffer.duration / A.rate(st.pitch);
        return 1;
    };
    var unlock = function () {
        var active = globalThis.__kryonAudio;
        if (active) active.resume();
    };
    if (typeof document !== 'undefined' && !document.__kryonAudioUnlock) {
        document.__kryonAudioUnlock = 1;
        ['pointerdown', 'keydown', 'touchstart'].forEach(function (name) {
            document.addEventListener(name, unlock, {passive: true});
        });
    }
  },
  js_audio_close: function() {
    var A = globalThis.__kryonAudio;
    if (!A) return;
    Object.keys(A.buffers).forEach(function(id) {
        A.stopSound(id);
        A.stopMusic(id);
    });
    Object.keys(A.streams).forEach(function(id) {
        var stream = A.streams[id];
        A.stopStreamCallback(stream);
        stream.sources.forEach(function(source) {
            A.stopSource(source.source);
            A.disconnectOutput(source.output);
        });
    });
    if (A.master) { try { A.master.disconnect(); } catch (e) {} }
    if (A.ctx && A.ctx.close) {
        try { var close = A.ctx.close(); if (close && close.catch) close.catch(function() {}); } catch (e) {}
    }
    A.buffers = {}; A.streams = {}; A.mixedProcessors = [];
    globalThis.__kryonAudio = null;
  },
  js_audio_ready: function() {
    var A = globalThis.__kryonAudio;
    return A && A.ensure && A.ensure() ? 1 : 0;
  },
  js_audio_set_master_volume: function(volume) {
    var A = globalThis.__kryonAudio;
    if (!A) return;
    A.masterVolume = A.clamp01(volume);
    if (A.master && A.master.gain) A.setParam(A.master.gain, A.masterVolume);
  },
  js_audio_get_master_volume: function() {
    var A = globalThis.__kryonAudio;
    return A ? A.masterVolume : 1.0;
  },
  js_audio_load_file__deps: ['$UTF8ToString', '$Asyncify'],
  js_audio_load_file__async: true,
  js_audio_load_file: function(file_name) {
    return Asyncify.handleAsync(async function () {
    var A = globalThis.__kryonAudio;
    if (!A) return 0;
    try { return await A.decodeBytes(await A.readFileBytes(UTF8ToString(file_name))); }
    catch (e) { return 0; }
    });
  },
  js_audio_load_memory__deps: ['$Asyncify'],
  js_audio_load_memory__async: true,
  js_audio_load_memory: function(data, data_size) {
    return Asyncify.handleAsync(async function () {
    var A = globalThis.__kryonAudio;
    if (!A || !data || data_size <= 0) return 0;
    try { return await A.decodeBytes(HEAPU8.slice(data, data + data_size)); }
    catch (e) { return 0; }
    });
  },
  js_audio_decode_wave_memory__deps: ['malloc', '$Asyncify'],
  js_audio_decode_wave_memory__async: true,
  js_audio_decode_wave_memory: function(data, data_size, sr_out, ch_out, frames_out) {
    return Asyncify.handleAsync(async function () {
    var A = globalThis.__kryonAudio;
    if (!A || !data || data_size <= 0) return 0;
    try {
        var id = await A.decodeBytes(HEAPU8.slice(data, data + data_size));
        var entry = A.buffers[id];
        if (!entry) return 0;
        var b = entry.buffer;
        delete A.buffers[id];
        var channels = b.numberOfChannels || 1, frames = b.length || 0;
        var byteLength = frames * channels * 4;
        if (!Number.isSafeInteger(byteLength) || byteLength <= 0 || byteLength > 0x7fffffff) return 0;
        var ptr = _malloc(byteLength);
        if (!ptr) return 0;
        for (var ch = 0; ch < channels; ch++) {
            var src = b.getChannelData(ch);
            for (var i = 0; i < frames; i++)
                HEAPF32[(ptr >> 2) + i * channels + ch] = src[i];
        }
        HEAP32[sr_out >> 2] = b.sampleRate || 44100;
        HEAP32[ch_out >> 2] = channels;
        HEAP32[frames_out >> 2] = frames;
        return ptr;
    } catch (e) {
        return 0;
    }
    });
  },
  js_audio_buffer_from_pcm: function(data, frames, sample_rate, sample_size, channels) {
    var A = globalThis.__kryonAudio, ctx = A && A.ensure ? A.ensure() : null;
    if (!A || !ctx || !data || frames <= 0 || sample_rate <= 0 || channels <= 0)
        return 0;
    try {
        var buffer = ctx.createBuffer(channels, frames, sample_rate);
        for (var ch = 0; ch < channels; ch++) {
            var dst = buffer.getChannelData(ch);
            for (var i = 0; i < frames; i++) {
                var idx = i * channels + ch;
                var v = sample_size === 8 ? (HEAPU8[data + idx] - 128) / 128.0 :
                        sample_size === 16 ? HEAP16[(data >> 1) + idx] / 32768.0 :
                        HEAPF32[(data >> 2) + idx];
                dst[i] = Math.max(-1, Math.min(1, v));
            }
        }
        return A.storeBuffer(buffer);
    } catch (e) {
        return 0;
    }
  },
  js_audio_update_buffer_from_pcm: function(id, data, frames, sample_rate, sample_size, channels) {
    var A = globalThis.__kryonAudio, ctx = A && A.ensure ? A.ensure() : null;
    var entry = A && A.buffers[id];
    if (!A || !ctx || !entry || !data || frames <= 0 || sample_rate <= 0 || channels <= 0)
        return 0;
    try {
        var buffer = ctx.createBuffer(channels, frames, sample_rate);
        for (var ch = 0; ch < channels; ch++) {
            var dst = buffer.getChannelData(ch);
            for (var i = 0; i < frames; i++) {
                var idx = i * channels + ch;
                var v = sample_size === 8 ? (HEAPU8[data + idx] - 128) / 128.0 :
                        sample_size === 16 ? HEAP16[(data >> 1) + idx] / 32768.0 :
                        HEAPF32[(data >> 2) + idx];
                dst[i] = Math.max(-1, Math.min(1, v));
            }
        }
        A.stopSound(id);
        A.stopMusic(id);
        entry.buffer = buffer;
        return 1;
    } catch (e) {
        return 0;
    }
  },
  js_audio_ref: function(id) {
    var A = globalThis.__kryonAudio;
    if (A && A.buffers[id]) A.buffers[id].ref++;
  },
  js_audio_release: function(id) {
    var A = globalThis.__kryonAudio, entry = A && A.buffers[id];
    if (!entry) return;
    entry.ref--;
    if (entry.ref > 0) return;
    A.stopSound(id);
    A.stopMusic(id);
    delete A.buffers[id];
  },
  js_audio_buffer_frames: function(id) {
    var e = globalThis.__kryonAudio && globalThis.__kryonAudio.buffers[id];
    return e && e.buffer ? e.buffer.length | 0 : 0;
  },
  js_audio_buffer_rate: function(id) {
    var e = globalThis.__kryonAudio && globalThis.__kryonAudio.buffers[id];
    return e && e.buffer ? e.buffer.sampleRate | 0 : 0;
  },
  js_audio_buffer_channels: function(id) {
    var e = globalThis.__kryonAudio && globalThis.__kryonAudio.buffers[id];
    return e && e.buffer ? e.buffer.numberOfChannels | 0 : 0;
  },
  js_audio_sound_play: function(id) {
    var A = globalThis.__kryonAudio; if (A) A.playSound(id, 0);
  },
  js_audio_sound_stop: function(id) {
    var A = globalThis.__kryonAudio; if (A) A.stopSound(id);
  },
  js_audio_sound_pause: function(id) {
    var A = globalThis.__kryonAudio; if (A) A.pauseSound(id);
  },
  js_audio_sound_resume: function(id) {
    var A = globalThis.__kryonAudio; if (A) A.resumeSound(id);
  },
  js_audio_sound_playing: function(id) {
    var A = globalThis.__kryonAudio, e = A && A.buffers[id];
    if (!e) return 0;
    A.prune(e);
    return e.sources.length > 0 ? 1 : 0;
  },
  js_audio_buffer_volume: function(id, volume) {
    var A = globalThis.__kryonAudio, e = A && A.buffers[id];
    if (!e) return;
    e.volume = A.clamp01(volume);
    e.sources.forEach(function (s) {
        if (s.output && s.output.gain) A.setParam(s.output.gain.gain, e.volume);
    });
    if (e.music && e.music.output && e.music.output.gain)
        A.setParam(e.music.output.gain.gain, e.volume);
  },
  js_audio_buffer_pitch: function(id, pitch) {
    var A = globalThis.__kryonAudio, e = A && A.buffers[id];
    if (!e) return;
    e.pitch = A.rate(pitch);
    e.sources.forEach(function (s) {
        if (s.source && s.source.playbackRate) A.setParam(s.source.playbackRate, e.pitch);
    });
    if (e.music && e.music.source && e.music.source.playbackRate)
        A.setParam(e.music.source.playbackRate, e.pitch);
  },
  js_audio_buffer_pan: function(id, pan) {
    var A = globalThis.__kryonAudio, e = A && A.buffers[id];
    if (!e) return;
    e.pan = A.clampPan(pan);
    e.sources.forEach(function (s) {
        if (s.output && s.output.panner) A.setParam(s.output.panner.pan, e.pan);
    });
    if (e.music && e.music.output && e.music.output.panner)
        A.setParam(e.music.output.panner.pan, e.pan);
  },
  js_audio_music_play: function(id, looping) {
    var A = globalThis.__kryonAudio; if (A) A.playMusic(id, looping);
  },
  js_audio_music_stop: function(id) {
    var A = globalThis.__kryonAudio; if (A) A.stopMusic(id);
  },
  js_audio_music_pause: function(id) {
    var A = globalThis.__kryonAudio; if (A) A.pauseMusic(id);
  },
  js_audio_music_resume: function(id) {
    var A = globalThis.__kryonAudio, e = A && A.buffers[id];
    if (A && e) A.playMusic(id, e.music ? e.music.looping : true);
  },
  js_audio_music_seek: function(id, position) {
    var A = globalThis.__kryonAudio; if (A) A.seekMusic(id, position);
  },
  js_audio_music_playing: function(id) {
    var e = globalThis.__kryonAudio && globalThis.__kryonAudio.buffers[id];
    return e && e.music && e.music.playing ? 1 : 0;
  },
  js_audio_music_length: function(id) {
    var e = globalThis.__kryonAudio && globalThis.__kryonAudio.buffers[id];
    return e && e.buffer ? e.buffer.duration || 0 : 0;
  },
  js_audio_music_played: function(id) {
    var A = globalThis.__kryonAudio, e = A && A.buffers[id];
    return A && e ? A.musicOffset(e) : 0;
  },
  js_audio_stream_create: function(sample_rate, sample_size, channels) {
    var A = globalThis.__kryonAudio;
    return A ? A.streamCreate(sample_rate, sample_size, channels) : 0;
  },
  js_audio_stream_processed: function(id) {
    var A = globalThis.__kryonAudio, stream = A && A.streams[id];
    return stream && stream.playing && stream.sources.length < 2 ? 1 : 0;
  },
  js_audio_stream_free: function(id) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id];
    if (!st) return;
    st.sources.forEach(function (s) { A.stopSource(s.source || s); A.disconnectOutput(s.output); });
    A.stopStreamCallback(st);
    delete A.streams[id];
  },
  js_audio_stream_play: function(id) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id], ctx = A && A.ensure ? A.ensure() : null;
    if (!st || !ctx) return;
    A.resume();
    st.playing = true;
    st.nextTime = ctx.currentTime || 0;
    A.startStreamCallback(st);
  },
  js_audio_stream_stop: function(id) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id];
    if (!st) return;
    st.sources.forEach(function (s) { A.stopSource(s.source || s); A.disconnectOutput(s.output); });
    st.sources = [];
    st.playing = false;
    st.nextTime = 0;
    A.stopStreamCallback(st);
  },
  js_audio_stream_pause: function(id) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id];
    if (st) {
        st.playing = false;
        A.stopStreamCallback(st);
    }
  },
  js_audio_stream_resume: function(id) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id], ctx = A && A.ensure ? A.ensure() : null;
    if (!st || !ctx) return;
    A.resume();
    st.playing = true;
    st.nextTime = Math.max(st.nextTime || 0, ctx.currentTime || 0);
    A.startStreamCallback(st);
  },
  js_audio_stream_playing: function(id) {
    var st = globalThis.__kryonAudio && globalThis.__kryonAudio.streams[id];
    return st && st.playing ? 1 : 0;
  },
  js_audio_stream_update: function(id, data, frames, sample_rate, sample_size, channels) {
    var A = globalThis.__kryonAudio;
    return A ? A.streamPush(id, data, frames, sample_rate, sample_size, channels) : 0;
  },
  js_audio_stream_volume: function(id, volume) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id];
    if (!st) return;
    st.volume = A.clamp01(volume);
    st.sources.forEach(function (s) {
        if (s.output && s.output.gain) A.setParam(s.output.gain.gain, st.volume);
    });
  },
  js_audio_stream_pitch: function(id, pitch) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id];
    if (!st) return;
    st.pitch = A.rate(pitch);
    st.sources.forEach(function (s) {
        if (s.source && s.source.playbackRate) A.setParam(s.source.playbackRate, st.pitch);
    });
  },
  js_audio_stream_pan: function(id, pan) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id];
    if (!st) return;
    st.pan = A.clampPan(pan);
    st.sources.forEach(function (s) {
        if (s.output && s.output.panner) A.setParam(s.output.panner.pan, st.pan);
    });
  },
  js_audio_stream_callback: function(id, callback, frames) {
    var A = globalThis.__kryonAudio, st = A && A.streams[id];
    if (!st) return;
    st.callback = callback;
    st.callbackFrames = frames > 0 ? frames : st.callbackFrames || 1024;
    if (!callback) A.stopStreamCallback(st);
    else if (st.playing) A.startStreamCallback(st);
  },
  js_audio_stream_attach_processor: function(id, callback) {
    var st = globalThis.__kryonAudio && globalThis.__kryonAudio.streams[id];
    if (!st || !callback) return;
    if (st.processors.indexOf(callback) < 0) st.processors.push(callback);
  },
  js_audio_stream_detach_processor: function(id, callback) {
    var st = globalThis.__kryonAudio && globalThis.__kryonAudio.streams[id];
    if (!st || !callback) return;
    st.processors = st.processors.filter(function (p) { return p !== callback; });
  },
  js_audio_attach_mixed_processor: function(callback) {
    var A = globalThis.__kryonAudio;
    if (!A || !callback) return;
    if (A.mixedProcessors.indexOf(callback) < 0) A.mixedProcessors.push(callback);
  },
  js_audio_detach_mixed_processor: function(callback) {
    var A = globalThis.__kryonAudio;
    if (!A || !callback) return;
    A.mixedProcessors = A.mixedProcessors.filter(function (p) { return p !== callback; });
  },
});

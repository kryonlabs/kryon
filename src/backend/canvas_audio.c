/*
 * canvas_audio.c - WebAudio implementation for the Tier A Canvas2D backend.
 *
 * Browser audio is async, while the raylib-style surface is synchronous. The
 * canvas web build therefore requires Asyncify: encoded file/memory loaders
 * await decodeAudioData, then return ordinary Sound/Music handles to C.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

#include <math.h>
#include <stdint.h>

EM_JS(void, js_audio_init, (void), {
    var g = globalThis;
    if (g.__kryAudio && g.__kryAudio.version === 1) return;
    var A = g.__kryAudio = {
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
    A.decodeBytes = async function (bytes) {
        var ctx = A.ensure();
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
    };
    A.sampleBytes = function (sampleSize) {
        return sampleSize === 8 ? 1 : sampleSize === 16 ? 2 : 4;
    };
    A.callAudioCallback = function (callback, ptr, frames) {
        if (!callback || !ptr || frames <= 0) return;
        try {
            dynCall('vii', callback, [ptr, frames]);
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
            if (!st.playing || !st.callback) return;
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
        source.onended = function () { state.done = true; A.prune(entry); };
        entry.sources.push(state);
        try { source.start(0, state.offset); } catch (e) { state.done = true; return 0; }
        return 1;
    };
    A.stopSound = function (id) {
        var entry = A.buffers[id];
        if (!entry) return;
        entry.sources.forEach(function (s) { s.done = true; A.stopSource(s.source); });
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
        if (entry.music && entry.music.source) A.stopSource(entry.music.source);
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
            if (entry.music === music && !music.looping) music.playing = false;
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
        entry.music.source = null;
    };
    A.stopMusic = function (id) {
        var entry = A.buffers[id];
        if (!entry) return;
        if (entry.music) A.stopSource(entry.music.source);
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
        if (!st || !ctx || !st.playing || frames <= 0) return 0;
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
        st.sources.push({source: source, output: output});
        st.nextTime += buffer.duration / A.rate(st.pitch);
        return 1;
    };
    var unlock = function () { A.resume(); };
    if (typeof document !== 'undefined' && !document.__kryAudioUnlock) {
        document.__kryAudioUnlock = 1;
        ['pointerdown', 'keydown', 'touchstart'].forEach(function (name) {
            document.addEventListener(name, unlock, {passive: true});
        });
    }
});

EM_JS(int, js_audio_ready, (void), {
    var A = globalThis.__kryAudio;
    return A && A.ensure && A.ensure() ? 1 : 0;
});

EM_JS(void, js_audio_set_master_volume, (float volume), {
    var A = globalThis.__kryAudio;
    if (!A) return;
    A.masterVolume = A.clamp01(volume);
    if (A.master && A.master.gain) A.setParam(A.master.gain, A.masterVolume);
});

EM_JS(float, js_audio_get_master_volume, (void), {
    var A = globalThis.__kryAudio;
    return A ? A.masterVolume : 1.0;
});

EM_ASYNC_JS(int, js_audio_load_file, (const char *file_name), {
    var A = globalThis.__kryAudio;
    if (!A) return 0;
    try { return await A.decodeBytes(await A.readFileBytes(UTF8ToString(file_name))); }
    catch (e) { return 0; }
});

EM_ASYNC_JS(int, js_audio_load_memory, (const unsigned char *data, int data_size), {
    var A = globalThis.__kryAudio;
    if (!A || !data || data_size <= 0) return 0;
    try { return await A.decodeBytes(HEAPU8.slice(data, data + data_size)); }
    catch (e) { return 0; }
});

EM_ASYNC_JS(int, js_audio_decode_wave_memory,
            (const unsigned char *data, int data_size, int sr_out, int ch_out, int frames_out), {
    var A = globalThis.__kryAudio;
    if (!A || !data || data_size <= 0) return 0;
    try {
        var id = await A.decodeBytes(HEAPU8.slice(data, data + data_size));
        var entry = A.buffers[id];
        if (!entry) return 0;
        var b = entry.buffer;
        var channels = b.numberOfChannels || 1, frames = b.length || 0;
        var ptr = _malloc(frames * channels * 4);
        if (!ptr) return 0;
        for (var ch = 0; ch < channels; ch++) {
            var src = b.getChannelData(ch);
            for (var i = 0; i < frames; i++)
                HEAPF32[(ptr >> 2) + i * channels + ch] = src[i];
        }
        delete A.buffers[id];
        setValue(sr_out, b.sampleRate || 44100, 'i32');
        setValue(ch_out, channels, 'i32');
        setValue(frames_out, frames, 'i32');
        return ptr;
    } catch (e) {
        return 0;
    }
});

EM_JS(int, js_audio_buffer_from_pcm,
      (const void *data, int frames, int sample_rate, int sample_size, int channels), {
    var A = globalThis.__kryAudio, ctx = A && A.ensure ? A.ensure() : null;
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
});

EM_JS(int, js_audio_update_buffer_from_pcm,
      (int id, const void *data, int frames, int sample_rate, int sample_size, int channels), {
    var A = globalThis.__kryAudio, ctx = A && A.ensure ? A.ensure() : null;
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
});

EM_JS(void, js_audio_ref, (int id), {
    var A = globalThis.__kryAudio;
    if (A && A.buffers[id]) A.buffers[id].ref++;
});

EM_JS(void, js_audio_release, (int id), {
    var A = globalThis.__kryAudio, entry = A && A.buffers[id];
    if (!entry) return;
    entry.ref--;
    if (entry.ref > 0) return;
    A.stopSound(id);
    A.stopMusic(id);
    delete A.buffers[id];
});

EM_JS(int, js_audio_buffer_frames, (int id), {
    var e = globalThis.__kryAudio && globalThis.__kryAudio.buffers[id];
    return e && e.buffer ? e.buffer.length | 0 : 0;
});

EM_JS(int, js_audio_buffer_rate, (int id), {
    var e = globalThis.__kryAudio && globalThis.__kryAudio.buffers[id];
    return e && e.buffer ? e.buffer.sampleRate | 0 : 0;
});

EM_JS(int, js_audio_buffer_channels, (int id), {
    var e = globalThis.__kryAudio && globalThis.__kryAudio.buffers[id];
    return e && e.buffer ? e.buffer.numberOfChannels | 0 : 0;
});

EM_JS(void, js_audio_sound_play, (int id), {
    var A = globalThis.__kryAudio; if (A) A.playSound(id, 0);
});
EM_JS(void, js_audio_sound_stop, (int id), {
    var A = globalThis.__kryAudio; if (A) A.stopSound(id);
});
EM_JS(void, js_audio_sound_pause, (int id), {
    var A = globalThis.__kryAudio; if (A) A.pauseSound(id);
});
EM_JS(void, js_audio_sound_resume, (int id), {
    var A = globalThis.__kryAudio; if (A) A.resumeSound(id);
});
EM_JS(int, js_audio_sound_playing, (int id), {
    var A = globalThis.__kryAudio, e = A && A.buffers[id];
    if (!e) return 0;
    A.prune(e);
    return e.sources.length > 0 ? 1 : 0;
});
EM_JS(void, js_audio_buffer_volume, (int id, float volume), {
    var A = globalThis.__kryAudio, e = A && A.buffers[id];
    if (!e) return;
    e.volume = A.clamp01(volume);
    e.sources.forEach(function (s) {
        if (s.output && s.output.gain) A.setParam(s.output.gain.gain, e.volume);
    });
    if (e.music && e.music.output && e.music.output.gain)
        A.setParam(e.music.output.gain.gain, e.volume);
});
EM_JS(void, js_audio_buffer_pitch, (int id, float pitch), {
    var A = globalThis.__kryAudio, e = A && A.buffers[id];
    if (!e) return;
    e.pitch = A.rate(pitch);
    e.sources.forEach(function (s) {
        if (s.source && s.source.playbackRate) A.setParam(s.source.playbackRate, e.pitch);
    });
    if (e.music && e.music.source && e.music.source.playbackRate)
        A.setParam(e.music.source.playbackRate, e.pitch);
});
EM_JS(void, js_audio_buffer_pan, (int id, float pan), {
    var A = globalThis.__kryAudio, e = A && A.buffers[id];
    if (!e) return;
    e.pan = A.clampPan(pan);
    e.sources.forEach(function (s) {
        if (s.output && s.output.panner) A.setParam(s.output.panner.pan, e.pan);
    });
    if (e.music && e.music.output && e.music.output.panner)
        A.setParam(e.music.output.panner.pan, e.pan);
});

EM_JS(void, js_audio_music_play, (int id, int looping), {
    var A = globalThis.__kryAudio; if (A) A.playMusic(id, looping);
});
EM_JS(void, js_audio_music_stop, (int id), {
    var A = globalThis.__kryAudio; if (A) A.stopMusic(id);
});
EM_JS(void, js_audio_music_pause, (int id), {
    var A = globalThis.__kryAudio; if (A) A.pauseMusic(id);
});
EM_JS(void, js_audio_music_resume, (int id), {
    var A = globalThis.__kryAudio, e = A && A.buffers[id];
    if (A && e) A.playMusic(id, e.music ? e.music.looping : true);
});
EM_JS(void, js_audio_music_seek, (int id, float position), {
    var A = globalThis.__kryAudio; if (A) A.seekMusic(id, position);
});
EM_JS(int, js_audio_music_playing, (int id), {
    var e = globalThis.__kryAudio && globalThis.__kryAudio.buffers[id];
    return e && e.music && e.music.playing ? 1 : 0;
});
EM_JS(float, js_audio_music_length, (int id), {
    var e = globalThis.__kryAudio && globalThis.__kryAudio.buffers[id];
    return e && e.buffer ? e.buffer.duration || 0 : 0;
});
EM_JS(float, js_audio_music_played, (int id), {
    var A = globalThis.__kryAudio, e = A && A.buffers[id];
    return A && e ? A.musicOffset(e) : 0;
});

EM_JS(int, js_audio_stream_create, (int sample_rate, int sample_size, int channels), {
    var A = globalThis.__kryAudio;
    return A ? A.streamCreate(sample_rate, sample_size, channels) : 0;
});
EM_JS(void, js_audio_stream_free, (int id), {
    var A = globalThis.__kryAudio, st = A && A.streams[id];
    if (!st) return;
    st.sources.forEach(function (s) { A.stopSource(s.source || s); });
    A.stopStreamCallback(st);
    delete A.streams[id];
});
EM_JS(void, js_audio_stream_play, (int id), {
    var A = globalThis.__kryAudio, st = A && A.streams[id], ctx = A && A.ensure ? A.ensure() : null;
    if (!st || !ctx) return;
    A.resume();
    st.playing = true;
    st.nextTime = ctx.currentTime || 0;
    A.startStreamCallback(st);
});
EM_JS(void, js_audio_stream_stop, (int id), {
    var A = globalThis.__kryAudio, st = A && A.streams[id];
    if (!st) return;
    st.sources.forEach(function (s) { A.stopSource(s.source || s); });
    st.sources = [];
    st.playing = false;
    st.nextTime = 0;
    A.stopStreamCallback(st);
});
EM_JS(void, js_audio_stream_pause, (int id), {
    var A = globalThis.__kryAudio, st = A && A.streams[id];
    if (st) {
        st.playing = false;
        A.stopStreamCallback(st);
    }
});
EM_JS(void, js_audio_stream_resume, (int id), {
    var A = globalThis.__kryAudio, st = A && A.streams[id], ctx = A && A.ensure ? A.ensure() : null;
    if (!st || !ctx) return;
    A.resume();
    st.playing = true;
    st.nextTime = Math.max(st.nextTime || 0, ctx.currentTime || 0);
    A.startStreamCallback(st);
});
EM_JS(int, js_audio_stream_playing, (int id), {
    var st = globalThis.__kryAudio && globalThis.__kryAudio.streams[id];
    return st && st.playing ? 1 : 0;
});
EM_JS(int, js_audio_stream_update, (int id, const void *data, int frames,
                                    int sample_rate, int sample_size, int channels), {
    var A = globalThis.__kryAudio;
    return A ? A.streamPush(id, data, frames, sample_rate, sample_size, channels) : 0;
});
EM_JS(void, js_audio_stream_volume, (int id, float volume), {
    var A = globalThis.__kryAudio, st = A && A.streams[id];
    if (!st) return;
    st.volume = A.clamp01(volume);
    st.sources.forEach(function (s) {
        if (s.output && s.output.gain) A.setParam(s.output.gain.gain, st.volume);
    });
});
EM_JS(void, js_audio_stream_pitch, (int id, float pitch), {
    var A = globalThis.__kryAudio, st = A && A.streams[id];
    if (!st) return;
    st.pitch = A.rate(pitch);
    st.sources.forEach(function (s) {
        if (s.source && s.source.playbackRate) A.setParam(s.source.playbackRate, st.pitch);
    });
});
EM_JS(void, js_audio_stream_pan, (int id, float pan), {
    var A = globalThis.__kryAudio, st = A && A.streams[id];
    if (!st) return;
    st.pan = A.clampPan(pan);
    st.sources.forEach(function (s) {
        if (s.output && s.output.panner) A.setParam(s.output.panner.pan, st.pan);
    });
});
EM_JS(void, js_audio_stream_callback, (int id, int callback, int frames), {
    var A = globalThis.__kryAudio, st = A && A.streams[id];
    if (!st) return;
    st.callback = callback;
    st.callbackFrames = frames > 0 ? frames : st.callbackFrames || 1024;
    if (!callback) A.stopStreamCallback(st);
    else if (st.playing) A.startStreamCallback(st);
});
EM_JS(void, js_audio_stream_attach_processor, (int id, int callback), {
    var st = globalThis.__kryAudio && globalThis.__kryAudio.streams[id];
    if (!st || !callback) return;
    if (st.processors.indexOf(callback) < 0) st.processors.push(callback);
});
EM_JS(void, js_audio_stream_detach_processor, (int id, int callback), {
    var st = globalThis.__kryAudio && globalThis.__kryAudio.streams[id];
    if (!st || !callback) return;
    st.processors = st.processors.filter(function (p) { return p !== callback; });
});
EM_JS(void, js_audio_attach_mixed_processor, (int callback), {
    var A = globalThis.__kryAudio;
    if (!A || !callback) return;
    if (A.mixedProcessors.indexOf(callback) < 0) A.mixedProcessors.push(callback);
});
EM_JS(void, js_audio_detach_mixed_processor, (int callback), {
    var A = globalThis.__kryAudio;
    if (!A || !callback) return;
    A.mixedProcessors = A.mixedProcessors.filter(function (p) { return p !== callback; });
});

static int g_audio_initialized;
static int g_audio_stream_default_frames = 4096;

static void audio_ensure(void)
{
    if(!g_audio_initialized) {
        js_audio_init();
        g_audio_initialized = 1;
    }
}

static int audio_id_from_stream(AudioStream stream)
{
    return (int)(uintptr_t)stream.buffer;
}

static AudioStream audio_stream_from_id(int id, unsigned int sample_rate,
                                        unsigned int sample_size,
                                        unsigned int channels)
{
    AudioStream stream = {0};
    stream.buffer = (rAudioBuffer *)(uintptr_t)id;
    stream.sampleRate = sample_rate;
    stream.sampleSize = sample_size;
    stream.channels = channels;
    return stream;
}

static Sound sound_from_id(int id)
{
    Sound sound = {0};
    if(id <= 0) return sound;
    sound.stream = audio_stream_from_id(id, (unsigned int)js_audio_buffer_rate(id),
                                        32u, (unsigned int)js_audio_buffer_channels(id));
    sound.frameCount = (unsigned int)js_audio_buffer_frames(id);
    return sound;
}

static Music music_from_id(int id)
{
    Music music = {0};
    if(id <= 0) return music;
    music.stream = audio_stream_from_id(id, (unsigned int)js_audio_buffer_rate(id),
                                        32u, (unsigned int)js_audio_buffer_channels(id));
    music.frameCount = (unsigned int)js_audio_buffer_frames(id);
    music.looping = true;
    music.ctxData = (void *)(uintptr_t)id;
    return music;
}

static int sample_bytes(unsigned int sample_size)
{
    if(sample_size == 8) return 1;
    if(sample_size == 16) return 2;
    if(sample_size == 32) return 4;
    return 0;
}

static unsigned int sample_count(Wave wave)
{
    return wave.frameCount * wave.channels;
}

static uint16_t rd16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

static void wr32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static float sample_as_float(const void *data, unsigned int sample_size,
                             unsigned int index)
{
    if(data == NULL) return 0.0f;
    if(sample_size == 8) return (((const unsigned char *)data)[index] - 128) / 128.0f;
    if(sample_size == 16) return ((const int16_t *)data)[index] / 32768.0f;
    if(sample_size == 32) return ((const float *)data)[index];
    return 0.0f;
}

static void write_float_sample(void *data, unsigned int sample_size,
                               unsigned int index, float v)
{
    if(v > 1.0f) v = 1.0f;
    if(v < -1.0f) v = -1.0f;
    if(sample_size == 8)
        ((unsigned char *)data)[index] = (unsigned char)(v * 127.0f + 128.0f);
    else if(sample_size == 16)
        ((int16_t *)data)[index] = (int16_t)(v * 32767.0f);
    else if(sample_size == 32)
        ((float *)data)[index] = v;
}

static Wave load_wav_memory(const unsigned char *file_data, int data_size)
{
    Wave wave = {0};
    uint16_t audio_format = 0, channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0, pcm_size = 0;
    const unsigned char *pcm = NULL;
    int pos = 12;

    if(file_data == NULL || data_size < 44) return wave;
    if(memcmp(file_data, "RIFF", 4) != 0 || memcmp(file_data + 8, "WAVE", 4) != 0)
        return wave;

    while(pos + 8 <= data_size) {
        const unsigned char *chunk = file_data + pos;
        uint32_t chunk_size = rd32(chunk + 4);
        int next = pos + 8 + (int)chunk_size + ((chunk_size & 1u) ? 1 : 0);
        if(next < pos || next > data_size + 1) break;
        if(memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16) {
            audio_format = rd16(chunk + 8);
            channels = rd16(chunk + 10);
            sample_rate = rd32(chunk + 12);
            bits_per_sample = rd16(chunk + 22);
        } else if(memcmp(chunk, "data", 4) == 0) {
            pcm = chunk + 8;
            pcm_size = chunk_size;
        }
        pos = next;
    }

    if(pcm == NULL || pcm_size == 0 || channels == 0 || sample_rate == 0)
        return wave;
    if(audio_format != 1 && audio_format != 3) return wave;
    if(audio_format == 3) bits_per_sample = 32;

    if(audio_format == 1 && bits_per_sample == 24) {
        unsigned int frames = pcm_size / ((unsigned int)channels * 3u);
        float *samples = (float *)calloc((size_t)frames * channels, sizeof(float));
        if(samples == NULL || frames == 0) {
            free(samples);
            return wave;
        }
        for(unsigned int i = 0; i < frames * channels; i++) {
            int32_t s = (int32_t)pcm[i * 3u] |
                        ((int32_t)pcm[i * 3u + 1u] << 8) |
                        ((int32_t)pcm[i * 3u + 2u] << 16);
            if(s & 0x00800000) s |= (int32_t)0xff000000;
            samples[i] = (float)s / 8388608.0f;
        }
        wave.frameCount = frames;
        wave.sampleRate = sample_rate;
        wave.sampleSize = 32;
        wave.channels = channels;
        wave.data = samples;
        return wave;
    }

    if(audio_format == 1 && bits_per_sample == 32) {
        unsigned int frames = pcm_size / ((unsigned int)channels * 4u);
        float *samples = (float *)calloc((size_t)frames * channels, sizeof(float));
        if(samples == NULL || frames == 0) {
            free(samples);
            return wave;
        }
        for(unsigned int i = 0; i < frames * channels; i++) {
            int32_t s = (int32_t)rd32(pcm + i * 4u);
            samples[i] = (float)((double)s / 2147483648.0);
        }
        wave.frameCount = frames;
        wave.sampleRate = sample_rate;
        wave.sampleSize = 32;
        wave.channels = channels;
        wave.data = samples;
        return wave;
    }

    if(bits_per_sample == 8 || bits_per_sample == 16 || bits_per_sample == 32) {
        int bytes = sample_bytes(bits_per_sample);
        unsigned int frames = pcm_size / ((unsigned int)channels * (unsigned int)bytes);
        size_t bytes_to_copy = (size_t)frames * channels * (unsigned int)bytes;
        void *copy;
        if(frames == 0) return wave;
        copy = malloc(bytes_to_copy);
        if(copy == NULL) return wave;
        memcpy(copy, pcm, bytes_to_copy);
        wave.frameCount = frames;
        wave.sampleRate = sample_rate;
        wave.sampleSize = bits_per_sample;
        wave.channels = channels;
        wave.data = copy;
    }
    return wave;
}

void InitAudioDevice(void)
{
    audio_ensure();
    (void)js_audio_ready();
}

void CloseAudioDevice(void)
{
    g_audio_initialized = 0;
}

bool IsAudioDeviceReady(void)
{
    audio_ensure();
    return js_audio_ready() != 0;
}

void SetMasterVolume(float volume)
{
    audio_ensure();
    js_audio_set_master_volume(volume);
}

float GetMasterVolume(void)
{
    audio_ensure();
    return js_audio_get_master_volume();
}

Wave LoadWave(const char *fileName)
{
    int data_size = 0;
    unsigned char *data = LoadFileData(fileName, &data_size);
    Wave wave = LoadWaveFromMemory(GetFileExtension(fileName), data, data_size);
    UnloadFileData(data);
    return wave;
}

Wave LoadWaveFromMemory(const char *fileType, const unsigned char *fileData,
                        int dataSize)
{
    Wave wave = {0};
    int sample_rate = 0, channels = 0, frames = 0, ptr = 0;

    if(fileData == NULL || dataSize <= 0) return wave;
    if(fileType == NULL || strcmp(fileType, ".wav") == 0 || strcmp(fileType, "wav") == 0)
        wave = load_wav_memory(fileData, dataSize);
    if(wave.data != NULL) return wave;

    audio_ensure();
    ptr = js_audio_decode_wave_memory(fileData, dataSize,
                                      (int)(uintptr_t)&sample_rate,
                                      (int)(uintptr_t)&channels,
                                      (int)(uintptr_t)&frames);
    if(ptr <= 0 || sample_rate <= 0 || channels <= 0 || frames <= 0)
        return (Wave){0};
    wave.frameCount = (unsigned int)frames;
    wave.sampleRate = (unsigned int)sample_rate;
    wave.sampleSize = 32;
    wave.channels = (unsigned int)channels;
    wave.data = (void *)(uintptr_t)ptr;
    return wave;
}

bool IsWaveValid(Wave wave)
{
    return wave.data != NULL && wave.frameCount > 0 && wave.sampleRate > 0 &&
           wave.channels > 0 && sample_bytes(wave.sampleSize) > 0;
}

void UnloadWave(Wave wave)
{
    free(wave.data);
}

Wave WaveCopy(Wave wave)
{
    Wave copy = {0};
    int bytes = sample_bytes(wave.sampleSize);
    size_t total;
    if(!IsWaveValid(wave)) return copy;
    total = (size_t)sample_count(wave) * (size_t)bytes;
    copy = wave;
    copy.data = malloc(total);
    if(copy.data == NULL) return (Wave){0};
    memcpy(copy.data, wave.data, total);
    return copy;
}

void WaveCrop(Wave *wave, int initFrame, int finalFrame)
{
    int bytes;
    unsigned int new_frames;
    void *new_data;
    if(wave == NULL || !IsWaveValid(*wave)) return;
    if(initFrame < 0) initFrame = 0;
    if(finalFrame < initFrame) finalFrame = initFrame;
    if((unsigned int)finalFrame > wave->frameCount) finalFrame = (int)wave->frameCount;
    bytes = sample_bytes(wave->sampleSize);
    new_frames = (unsigned int)(finalFrame - initFrame);
    new_data = calloc((size_t)new_frames * wave->channels, (size_t)bytes);
    if(new_data == NULL) return;
    memcpy(new_data,
           (const unsigned char *)wave->data +
               (size_t)initFrame * wave->channels * (unsigned int)bytes,
           (size_t)new_frames * wave->channels * (unsigned int)bytes);
    free(wave->data);
    wave->data = new_data;
    wave->frameCount = new_frames;
}

void WaveFormat(Wave *wave, int sampleRate, int sampleSize, int channels)
{
    unsigned int old_frames, old_channels, new_frames;
    void *new_data;
    int bytes;
    if(wave == NULL || !IsWaveValid(*wave) || sampleRate <= 0 || channels <= 0)
        return;
    if(sampleSize != 8 && sampleSize != 16 && sampleSize != 32)
        sampleSize = 32;
    bytes = sample_bytes((unsigned int)sampleSize);
    old_frames = wave->frameCount;
    old_channels = wave->channels;
    new_frames = (unsigned int)ceil((double)old_frames * (double)sampleRate /
                                    (double)wave->sampleRate);
    if(new_frames == 0) new_frames = 1;
    new_data = calloc((size_t)new_frames * (unsigned int)channels, (size_t)bytes);
    if(new_data == NULL) return;
    for(unsigned int i = 0; i < new_frames; i++) {
        double src_pos = (double)i * (double)(old_frames - 1) /
                         (double)(new_frames > 1 ? new_frames - 1 : 1);
        unsigned int src_i = (unsigned int)src_pos;
        unsigned int src_j = src_i + 1 < old_frames ? src_i + 1 : src_i;
        float frac = (float)(src_pos - src_i);
        for(int ch = 0; ch < channels; ch++) {
            unsigned int src_ch = (unsigned int)ch < old_channels ? (unsigned int)ch : 0;
            float a = sample_as_float(wave->data, wave->sampleSize,
                                      src_i * old_channels + src_ch);
            float b = sample_as_float(wave->data, wave->sampleSize,
                                      src_j * old_channels + src_ch);
            write_float_sample(new_data, (unsigned int)sampleSize,
                               i * (unsigned int)channels + (unsigned int)ch,
                               a + (b - a) * frac);
        }
    }
    free(wave->data);
    wave->data = new_data;
    wave->frameCount = new_frames;
    wave->sampleRate = (unsigned int)sampleRate;
    wave->sampleSize = (unsigned int)sampleSize;
    wave->channels = (unsigned int)channels;
}

float *LoadWaveSamples(Wave wave)
{
    unsigned int count = sample_count(wave);
    float *samples;
    if(!IsWaveValid(wave)) return NULL;
    samples = (float *)malloc((size_t)count * sizeof(float));
    if(samples == NULL) return NULL;
    for(unsigned int i = 0; i < count; i++)
        samples[i] = sample_as_float(wave.data, wave.sampleSize, i);
    return samples;
}

void UnloadWaveSamples(float *samples)
{
    free(samples);
}

bool ExportWave(Wave wave, const char *fileName)
{
    int bytes = sample_bytes(wave.sampleSize);
    unsigned int block_align, data_bytes;
    unsigned char *out;
    bool ok;

    if(fileName == NULL || !IsWaveValid(wave) || bytes == 0) return false;
    block_align = wave.channels * (unsigned int)bytes;
    data_bytes = wave.frameCount * block_align;
    out = (unsigned char *)malloc((size_t)data_bytes + 44u);
    if(out == NULL) return false;

    memcpy(out + 0, "RIFF", 4);
    wr32(out + 4, 36u + data_bytes);
    memcpy(out + 8, "WAVEfmt ", 8);
    wr32(out + 16, 16);
    wr16(out + 20, wave.sampleSize == 32 ? 3u : 1u);
    wr16(out + 22, wave.channels);
    wr32(out + 24, wave.sampleRate);
    wr32(out + 28, wave.sampleRate * block_align);
    wr16(out + 32, block_align);
    wr16(out + 34, wave.sampleSize);
    memcpy(out + 36, "data", 4);
    wr32(out + 40, data_bytes);
    memcpy(out + 44, wave.data, data_bytes);

    ok = SaveFileData(fileName, out, (int)data_bytes + 44);
    free(out);
    return ok;
}

bool ExportWaveAsCode(Wave wave, const char *fileName)
{
    int bytes = sample_bytes(wave.sampleSize);
    const unsigned char *raw = (const unsigned char *)wave.data;
    unsigned int data_bytes;
    size_t cap, len = 0;
    char *text;
    bool ok;

    if(fileName == NULL || !IsWaveValid(wave) || bytes == 0) return false;
    data_bytes = wave.frameCount * wave.channels * (unsigned int)bytes;
    cap = 512u + (size_t)data_bytes * 7u;
    text = (char *)malloc(cap);
    if(text == NULL) return false;

    len += (size_t)snprintf(text + len, cap - len,
                           "/* Generated by Kryon ExportWaveAsCode */\n"
                           "static const unsigned int wave_frame_count = %u;\n"
                           "static const unsigned int wave_sample_rate = %u;\n"
                           "static const unsigned int wave_sample_size = %u;\n"
                           "static const unsigned int wave_channels = %u;\n"
                           "static const unsigned char wave_data[%u] = {\n",
                           wave.frameCount, wave.sampleRate, wave.sampleSize,
                           wave.channels, data_bytes);
    for(unsigned int i = 0; i < data_bytes && len + 8u < cap; i++) {
        len += (size_t)snprintf(text + len, cap - len, "0x%02x%s",
                               raw[i], i + 1u < data_bytes ? "," : "");
        if((i % 12u) == 11u || i + 1u == data_bytes)
            len += (size_t)snprintf(text + len, cap - len, "\n");
        else
            len += (size_t)snprintf(text + len, cap - len, " ");
    }
    snprintf(text + len, cap - len, "};\n");
    ok = SaveFileText(fileName, text);
    free(text);
    return ok;
}

Sound LoadSound(const char *fileName)
{
    audio_ensure();
    return sound_from_id(js_audio_load_file(fileName));
}

Sound LoadSoundFromWave(Wave wave)
{
    audio_ensure();
    if(!IsWaveValid(wave)) return (Sound){0};
    return sound_from_id(js_audio_buffer_from_pcm(wave.data, (int)wave.frameCount,
                                                  (int)wave.sampleRate,
                                                  (int)wave.sampleSize,
                                                  (int)wave.channels));
}

Sound LoadSoundAlias(Sound source)
{
    int id = audio_id_from_stream(source.stream);
    audio_ensure();
    if(id <= 0) return (Sound){0};
    js_audio_ref(id);
    return source;
}

bool IsSoundValid(Sound sound)
{
    return audio_id_from_stream(sound.stream) > 0 && sound.frameCount > 0;
}

void UpdateSound(Sound sound, const void *data, int frameCount)
{
    int id = audio_id_from_stream(sound.stream);
    audio_ensure();
    if(id <= 0 || data == NULL || frameCount <= 0) return;
    int sample_rate = sound.stream.sampleRate ? (int)sound.stream.sampleRate : 44100;
    int sample_size = sound.stream.sampleSize ? (int)sound.stream.sampleSize : 32;
    int channels = sound.stream.channels ? (int)sound.stream.channels : 2;
    (void)js_audio_update_buffer_from_pcm(id, data, frameCount, sample_rate, sample_size, channels);
}

void UnloadSound(Sound sound)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_release(id);
}

void UnloadSoundAlias(Sound alias)
{
    UnloadSound(alias);
}

void PlaySound(Sound sound)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_sound_play(id);
}

void StopSound(Sound sound)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_sound_stop(id);
}

void PauseSound(Sound sound)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_sound_pause(id);
}

void ResumeSound(Sound sound)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_sound_resume(id);
}

bool IsSoundPlaying(Sound sound)
{
    int id = audio_id_from_stream(sound.stream);
    return id > 0 && js_audio_sound_playing(id) != 0;
}

void SetSoundVolume(Sound sound, float volume)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_buffer_volume(id, volume);
}

void SetSoundPitch(Sound sound, float pitch)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_buffer_pitch(id, pitch);
}

void SetSoundPan(Sound sound, float pan)
{
    int id = audio_id_from_stream(sound.stream);
    if(id > 0) js_audio_buffer_pan(id, pan);
}

Music LoadMusicStream(const char *fileName)
{
    audio_ensure();
    return music_from_id(js_audio_load_file(fileName));
}

Music LoadMusicStreamFromMemory(const char *fileType, const unsigned char *data,
                                int dataSize)
{
    (void)fileType;
    audio_ensure();
    return music_from_id(js_audio_load_memory(data, dataSize));
}

bool IsMusicValid(Music music)
{
    return audio_id_from_stream(music.stream) > 0 && music.frameCount > 0;
}

void UnloadMusicStream(Music music)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_release(id);
}

void PlayMusicStream(Music music)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_music_play(id, music.looping ? 1 : 0);
}

bool IsMusicStreamPlaying(Music music)
{
    int id = audio_id_from_stream(music.stream);
    return id > 0 && js_audio_music_playing(id) != 0;
}

void UpdateMusicStream(Music music)
{
    (void)music;
}

void StopMusicStream(Music music)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_music_stop(id);
}

void PauseMusicStream(Music music)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_music_pause(id);
}

void ResumeMusicStream(Music music)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_music_resume(id);
}

void SeekMusicStream(Music music, float position)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_music_seek(id, position);
}

void SetMusicVolume(Music music, float volume)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_buffer_volume(id, volume);
}

void SetMusicPitch(Music music, float pitch)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_buffer_pitch(id, pitch);
}

void SetMusicPan(Music music, float pan)
{
    int id = audio_id_from_stream(music.stream);
    if(id > 0) js_audio_buffer_pan(id, pan);
}

float GetMusicTimeLength(Music music)
{
    int id = audio_id_from_stream(music.stream);
    return id > 0 ? js_audio_music_length(id) : 0.0f;
}

float GetMusicTimePlayed(Music music)
{
    int id = audio_id_from_stream(music.stream);
    return id > 0 ? js_audio_music_played(id) : 0.0f;
}

AudioStream LoadAudioStream(unsigned int sampleRate, unsigned int sampleSize,
                            unsigned int channels)
{
    int id;
    audio_ensure();
    id = js_audio_stream_create((int)sampleRate, (int)sampleSize, (int)channels);
    return audio_stream_from_id(id, sampleRate, sampleSize, channels);
}

bool IsAudioStreamValid(AudioStream stream)
{
    return audio_id_from_stream(stream) > 0;
}

void UnloadAudioStream(AudioStream stream)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_free(id);
}

void UpdateAudioStream(AudioStream stream, const void *data, int frameCount)
{
    int id = audio_id_from_stream(stream);
    if(id <= 0 || data == NULL || frameCount <= 0) return;
    (void)js_audio_stream_update(id, data, frameCount, (int)stream.sampleRate,
                                 (int)stream.sampleSize, (int)stream.channels);
}

bool IsAudioStreamProcessed(AudioStream stream)
{
    (void)stream;
    return true;
}

void PlayAudioStream(AudioStream stream)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_play(id);
}

void PauseAudioStream(AudioStream stream)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_pause(id);
}

void ResumeAudioStream(AudioStream stream)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_resume(id);
}

bool IsAudioStreamPlaying(AudioStream stream)
{
    int id = audio_id_from_stream(stream);
    return id > 0 && js_audio_stream_playing(id) != 0;
}

void StopAudioStream(AudioStream stream)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_stop(id);
}

void SetAudioStreamVolume(AudioStream stream, float volume)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_volume(id, volume);
}

void SetAudioStreamPitch(AudioStream stream, float pitch)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_pitch(id, pitch);
}

void SetAudioStreamPan(AudioStream stream, float pan)
{
    int id = audio_id_from_stream(stream);
    if(id > 0) js_audio_stream_pan(id, pan);
}

void SetAudioStreamBufferSizeDefault(int size)
{
    if(size > 0) g_audio_stream_default_frames = size;
}

void SetAudioStreamCallback(AudioStream stream, AudioCallback callback)
{
    int id = audio_id_from_stream(stream);
    if(id > 0)
        js_audio_stream_callback(id, (int)(uintptr_t)callback,
                                 g_audio_stream_default_frames);
}

void AttachAudioStreamProcessor(AudioStream stream, AudioCallback processor)
{
    int id = audio_id_from_stream(stream);
    if(id > 0 && processor != NULL)
        js_audio_stream_attach_processor(id, (int)(uintptr_t)processor);
}

void DetachAudioStreamProcessor(AudioStream stream, AudioCallback processor)
{
    int id = audio_id_from_stream(stream);
    if(id > 0 && processor != NULL)
        js_audio_stream_detach_processor(id, (int)(uintptr_t)processor);
}

void AttachAudioMixedProcessor(AudioCallback processor)
{
    if(processor != NULL)
        js_audio_attach_mixed_processor((int)(uintptr_t)processor);
}

void DetachAudioMixedProcessor(AudioCallback processor)
{
    if(processor != NULL)
        js_audio_detach_mixed_processor((int)(uintptr_t)processor);
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */

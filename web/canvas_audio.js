// Raw WebAudio effects. Buffers, voices, positions, and streams are tracked in
// canvas_audio_engine.zi; this file only creates and connects nodes for the
// integer handles that module chooses.
addToLibrary({
  $KryonAudio__deps: ['$getWasmTableEntry', '$FS'],
  $KryonAudio: function () {
    var g = globalThis;
    if (g.__kryonAudio) return g.__kryonAudio;
    var A = g.__kryonAudio = {
        ctx: null, master: null, masterVolume: 1.0, decodeCtx: null,
        buffers: {}, sources: {}, timers: {}
    };
    A.setParam = function (param, value) {
        if (!param) return;
        if (param.setValueAtTime)
            param.setValueAtTime(value, A.ctx ? A.ctx.currentTime || 0 : 0);
        else param.value = value;
    };
    A.ensure = function () {
        if (A.ctx) return A.ctx;
        var AC = g.AudioContext || g.webkitAudioContext;
        if (!AC) return null;
        try {
            A.ctx = new AC();
            A.master = A.ctx.createGain ? A.ctx.createGain() : null;
            if (A.master) {
                A.setParam(A.master.gain, A.masterVolume);
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
    A.decoder = function () {
        /* Decoding on an autoplay-suspended AudioContext never settles in
         * some browsers, so decode on an offline context at the main rate. */
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
    A.decode = async function (bytes) {
        var ctx = A.decoder();
        if (!ctx || !ctx.decodeAudioData) return null;
        A.resume();
        var copy = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
        return await new Promise(function (resolve, reject) {
            var r = ctx.decodeAudioData(copy, resolve, reject);
            if (r && r.then) r.then(resolve, reject);
        });
    };
    A.readFile = async function (path) {
        if (typeof FS !== 'undefined' && FS.readFile) {
            try { return new Uint8Array(FS.readFile(path)); } catch (e) {}
        }
        if (typeof fetch === 'function') {
            var response = await fetch(path);
            if (response && response.ok)
                return new Uint8Array(await response.arrayBuffer());
        }
        throw new Error('no audio file source');
    };
    A.pcmBuffer = function (ptr, frames, rate, size, channels) {
        var ctx = A.ensure();
        if (!ctx || !ptr || frames <= 0 || rate <= 0 || channels <= 0) return null;
        var buffer = ctx.createBuffer(channels, frames, rate);
        for (var ch = 0; ch < channels; ch++) {
            var dst = buffer.getChannelData(ch);
            for (var i = 0; i < frames; i++) {
                var idx = i * channels + ch;
                var v = size === 8 ? (HEAPU8[ptr + idx] - 128) / 128.0 :
                        size === 16 ? HEAP16[(ptr >> 1) + idx] / 32768.0 :
                        HEAPF32[(ptr >> 2) + idx];
                dst[i] = Math.max(-1, Math.min(1, v));
            }
        }
        return buffer;
    };
    A.disconnect = function (node) {
        try { if (node) node.disconnect(); } catch (e) {}
    };
    A.release = function (record) {
        if (!record) return;
        try { record.source.onended = null; } catch (e) {}
        try { record.source.stop(0); } catch (e) {}
        A.disconnect(record.source);
        A.disconnect(record.gain);
        A.disconnect(record.panner);
    };
    A.play = function (handle, buffer, loop, rate, volume, pan, start) {
        var ctx = A.ensure();
        if (!buffer || !ctx || !ctx.createBufferSource) return 0;
        var record = {source: ctx.createBufferSource(), gain: null, panner: null};
        record.source.buffer = buffer;
        record.source.loop = !!loop;
        A.setParam(record.source.playbackRate, rate);
        var tail = null;
        if (ctx.createGain) {
            record.gain = ctx.createGain();
            A.setParam(record.gain.gain, volume);
            tail = record.gain;
        }
        if (ctx.createStereoPanner) {
            record.panner = ctx.createStereoPanner();
            A.setParam(record.panner.pan, pan);
            if (tail) tail.connect(record.panner);
            tail = record.panner;
        }
        if (tail) tail.connect(A.master || ctx.destination);
        record.source.connect(record.gain || record.panner || A.master || ctx.destination);
        record.source.onended = function () {
            A.release(record);
            if (A.sources[handle] === record) delete A.sources[handle];
        };
        A.sources[handle] = record;
        try { start(record.source); } catch (e) {
            A.release(record);
            delete A.sources[handle];
            return 0;
        }
        return 1;
    };
    var unlock = function () {
        var active = g.__kryonAudio;
        if (active) active.resume();
    };
    if (typeof document !== 'undefined' && !document.__kryonAudioUnlock) {
        document.__kryonAudioUnlock = 1;
        ['pointerdown', 'keydown', 'touchstart'].forEach(function (name) {
            document.addEventListener(name, unlock, {passive: true});
        });
    }
    return A;
  },

  js_wa_open__deps: ['$KryonAudio'],
  js_wa_open: function() {
    return KryonAudio().ensure() ? 1 : 0;
  },
  js_wa_close__deps: ['$KryonAudio'],
  js_wa_close: function() {
    var A = globalThis.__kryonAudio;
    if (!A) return;
    Object.keys(A.timers).forEach(function (handle) { clearInterval(A.timers[handle]); });
    Object.keys(A.sources).forEach(function (handle) { A.release(A.sources[handle]); });
    A.disconnect(A.master);
    if (A.ctx && A.ctx.close) {
        try { var closed = A.ctx.close(); if (closed && closed.catch) closed.catch(function () {}); } catch (e) {}
    }
    globalThis.__kryonAudio = null;
  },
  js_wa_resume__deps: ['$KryonAudio'],
  js_wa_resume: function() { KryonAudio().resume(); },
  js_wa_time__deps: ['$KryonAudio'],
  js_wa_time: function() {
    var A = KryonAudio();
    return A.ctx ? A.ctx.currentTime || 0 : 0;
  },
  js_wa_rate__deps: ['$KryonAudio'],
  js_wa_rate: function() {
    var A = KryonAudio();
    return A.ctx ? A.ctx.sampleRate || 0 : 0;
  },
  js_wa_master__deps: ['$KryonAudio'],
  js_wa_master: function(volume) {
    var A = KryonAudio();
    A.masterVolume = volume;
    if (A.master && A.master.gain) A.setParam(A.master.gain, volume);
  },

  js_wa_buffer_pcm__deps: ['$KryonAudio'],
  js_wa_buffer_pcm: function(handle, data, frames, rate, size, channels) {
    try {
        var buffer = KryonAudio().pcmBuffer(data, frames, rate, size, channels);
        if (!buffer) return 0;
        globalThis.__kryonAudio.buffers[handle] = buffer;
        return 1;
    } catch (e) { return 0; }
  },
  js_wa_buffer_file__deps: ['$KryonAudio', '$UTF8ToString', '$Asyncify'],
  js_wa_buffer_file__async: true,
  js_wa_buffer_file: function(handle, path) {
    return Asyncify.handleAsync(async function () {
        var A = KryonAudio();
        try {
            var buffer = await A.decode(await A.readFile(UTF8ToString(path)));
            if (!buffer || !globalThis.__kryonAudio) return 0;
            A.buffers[handle] = buffer;
            return 1;
        } catch (e) { return 0; }
    });
  },
  js_wa_buffer_memory__deps: ['$KryonAudio', '$Asyncify'],
  js_wa_buffer_memory__async: true,
  js_wa_buffer_memory: function(handle, data, size) {
    return Asyncify.handleAsync(async function () {
        var A = KryonAudio();
        try {
            var buffer = await A.decode(HEAPU8.slice(data, data + size));
            if (!buffer || !globalThis.__kryonAudio) return 0;
            A.buffers[handle] = buffer;
            return 1;
        } catch (e) { return 0; }
    });
  },
  js_wa_buffer_read__deps: ['$KryonAudio'],
  js_wa_buffer_read: function(handle, field) {
    var buffer = KryonAudio().buffers[handle];
    if (!buffer) return 0;
    return field === 0 ? buffer.length : field === 1 ? buffer.sampleRate :
           field === 2 ? buffer.numberOfChannels : buffer.duration || 0;
  },
  js_wa_buffer_export__deps: ['$KryonAudio', 'malloc'],
  js_wa_buffer_export: function(handle, rateOut, channelsOut, framesOut) {
    var b = KryonAudio().buffers[handle];
    if (!b) return 0;
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
    HEAP32[rateOut >> 2] = b.sampleRate || 44100;
    HEAP32[channelsOut >> 2] = channels;
    HEAP32[framesOut >> 2] = frames;
    return ptr;
  },
  js_wa_buffer_drop__deps: ['$KryonAudio'],
  js_wa_buffer_drop: function(handle) { delete KryonAudio().buffers[handle]; },

  js_wa_source_start__deps: ['$KryonAudio'],
  js_wa_source_start: function(handle, buffer, offset, loop, rate, volume, pan) {
    var A = KryonAudio();
    return A.play(handle, A.buffers[buffer], loop, rate, volume, pan,
        function (source) { source.start(0, offset); });
  },
  js_wa_source_pcm__deps: ['$KryonAudio'],
  js_wa_source_pcm: function(handle, data, frames, sampleRate, sampleSize, channels,
                             when, rate, volume, pan) {
    var A = KryonAudio();
    try {
        var buffer = A.pcmBuffer(data, frames, sampleRate, sampleSize, channels);
        return A.play(handle, buffer, 0, rate, volume, pan,
            function (source) { source.start(when); });
    } catch (e) { return 0; }
  },
  js_wa_source_stop__deps: ['$KryonAudio'],
  js_wa_source_stop: function(handle) {
    var A = KryonAudio();
    A.release(A.sources[handle]);
    delete A.sources[handle];
  },
  js_wa_source_set__deps: ['$KryonAudio'],
  js_wa_source_set: function(handle, volume, pan, rate) {
    var A = KryonAudio(), record = A.sources[handle];
    if (!record) return;
    if (record.gain) A.setParam(record.gain.gain, volume);
    if (record.panner) A.setParam(record.panner.pan, pan);
    if (record.source.playbackRate) A.setParam(record.source.playbackRate, rate);
  },
  js_wa_source_done__deps: ['$KryonAudio'],
  js_wa_source_done: function(handle) {
    return KryonAudio().sources[handle] ? 0 : 1;
  },

  js_wa_timer__deps: ['$KryonAudio', '$getWasmTableEntry'],
  js_wa_timer: function(handle, interval, tick) {
    var A = KryonAudio();
    if (A.timers[handle]) clearInterval(A.timers[handle]);
    A.timers[handle] = setInterval(function () {
        try { getWasmTableEntry(tick)(handle); } catch (e) {}
    }, interval);
  },
  js_wa_timer_stop__deps: ['$KryonAudio'],
  js_wa_timer_stop: function(handle) {
    var A = KryonAudio();
    if (A.timers[handle]) clearInterval(A.timers[handle]);
    delete A.timers[handle];
  },
});

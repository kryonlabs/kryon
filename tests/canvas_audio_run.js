Module['onExit'] = function (status) {
    if (status && typeof process !== 'undefined') process.exitCode = status;
};

class FakeAudioParam {
    constructor(value) { this.value = value; }
    setValueAtTime(value) { this.value = value; }
}

class FakeNode {
    connect(node) { return node || this; }
    disconnect() {}
}

class FakeBuffer {
    constructor(channels, frames, sampleRate) {
        this.numberOfChannels = channels;
        this.length = frames;
        this.sampleRate = sampleRate;
        this.duration = frames / sampleRate;
        this.data = Array.from({length: channels}, () => new Float32Array(frames));
    }
    getChannelData(channel) { return this.data[channel]; }
}

class FakeSource extends FakeNode {
    constructor() {
        super();
        this.playbackRate = new FakeAudioParam(1);
        this.loop = false;
        this.onended = null;
    }
    start() { this.started = true; }
    stop() {
        this.stopped = true;
        if (this.onended) this.onended();
    }
}

class FakeAudioContext {
    constructor() {
        this.currentTime = 1;
        this.sampleRate = 44100;
        this.state = 'running';
        this.destination = new FakeNode();
    }
    createGain() {
        const node = new FakeNode();
        node.gain = new FakeAudioParam(1);
        return node;
    }
    createStereoPanner() {
        const node = new FakeNode();
        node.pan = new FakeAudioParam(0);
        return node;
    }
    createBuffer(channels, frames, sampleRate) {
        return new FakeBuffer(channels, frames, sampleRate);
    }
    createBufferSource() { return new FakeSource(); }
    decodeAudioData(arrayBuffer, ok, fail) {
        const buffer = new FakeBuffer(2, 128, 44100);
        buffer.getChannelData(0).fill(0.25);
        buffer.getChannelData(1).fill(-0.25);
        if (ok) setTimeout(() => ok(buffer), 0);
        return Promise.resolve(buffer).catch(fail);
    }
    resume() {
        this.state = 'running';
        return Promise.resolve();
    }
}

globalThis.AudioContext = FakeAudioContext;
globalThis.document = {
    addEventListener() {}
};

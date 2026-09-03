// Node harness: recording Canvas2D shim + module run + assertions.
const calls = [];
const rec = (name) => function (...a) { calls.push(name); };
const grad = { addColorStop: rec('addColorStop') };
const ctxState = {
    canvas: {width: 320, height: 240},
    font: '', textBaseline: '', fillStyle: '', strokeStyle: '',
    lineWidth: 1, lineCap: '', globalAlpha: 1, imageSmoothingEnabled: true,
    fillRect: rec('fillRect'), strokeRect: rec('strokeRect'),
    clearRect: rec('clearRect'), beginPath: rec('beginPath'),
    arc: rec('arc'), fill: rec('fill'), stroke: rec('stroke'),
    moveTo: rec('moveTo'), lineTo: rec('lineTo'), closePath: rec('closePath'),
    drawImage: function (img, sx, sy, sw, sh) {
        calls.push('drawImage');
        // 9-arg form: reject out-of-bounds source rects — the browser
        // clips them to nothing, which silently blanks the frame.
        if (arguments.length >= 9 && img && img.width !== undefined) {
            if (sx < 0 || sy < 0 || sx + sw > img.width || sy + sh > img.height)
                throw new Error('drawImage source rect out of bounds: ' +
                    [sx, sy, sw, sh].join(',') + ' vs ' + img.width + 'x' + img.height);
        }
    },
    save: rec('save'), restore: rec('restore'),
    clip: rec('clip'), rect: rec('rect'),
    translate: rec('translate'), rotate: rec('rotate'), scale: rec('scale'),
    setTransform: rec('setTransform'),
    createLinearGradient: () => grad,
    createRadialGradient: () => grad,
    fillText: rec('fillText'),
    measureText: (t) => ({
        width: t.length * 8,
        actualBoundingBoxLeft: 0, actualBoundingBoxRight: t.length * 8,
        actualBoundingBoxAscent: 12, actualBoundingBoxDescent: 2
    }),
    getImageData: (x, y, w, h) => ({data: new Uint8ClampedArray(w * h * 4)}),
    putImageData: rec('putImageData'),
    createImageData: (w, h) => ({data: new Uint8ClampedArray(w * h * 4)})
};
const ctx = new Proxy(ctxState, {
    get: (t, k) => k in t ? t[k] : rec(String(k)),
    set: (t, k, v) => {
        if (k === 'imageSmoothingEnabled')
            calls.push('imageSmoothingEnabled=' + v);
        t[k] = v;
        return true;
    }
});
globalThis.OffscreenCanvas = class {
    constructor(w, h) { this.width = w; this.height = h; }
    getContext() { return ctx; }
};
globalThis.__kryTestCanvas = {
    width: 320, height: 240, style: {},
    getContext: () => ctx,
    getBoundingClientRect: () => ({left: 0, top: 0})
};
globalThis.location = { pathname: '/', hash: '' };
globalThis.history = {
    pushState(_state, _title, value) {
        applyLocation(value);
    },
    replaceState(_state, _title, value) {
        applyLocation(value);
    }
};

function applyLocation(value) {
    const text = String(value || '');
    const hashIndex = text.indexOf('#');
    if (hashIndex >= 0) {
        globalThis.location.pathname = text.slice(0, hashIndex) || '/';
        globalThis.location.hash = text.slice(hashIndex);
    } else {
        globalThis.location.pathname = text || '/';
        globalThis.location.hash = '';
    }
}

const Module = require('./canvas_smoke.js');

setTimeout(() => {
    const kinds = {};
    for (const c of calls) kinds[c] = (kinds[c] || 0) + 1;
    console.log('recorded calls:', calls.length, JSON.stringify(kinds));
    const fail = [];
    if (Module && Module.EXITSTATUS)
        fail.push('program exit status ' + Module.EXITSTATUS);
    if (!kinds.fillRect || kinds.fillRect < 6) fail.push('fillRect (background/rect per frame)');
    if ((kinds.strokeRect || 0) < 6) fail.push('strokeRect (rectangle outlines)');
    if (!kinds.arc) fail.push('arc (DrawCircle)');
    if (!kinds.moveTo) fail.push('moveTo (DrawLine)');
    if (!kinds.drawImage) fail.push('drawImage (DrawText atlas blits)');
    if (!kinds.roundRect) fail.push('roundRect (DrawRectangleRounded)');
    if (!kinds.addColorStop) fail.push('addColorStop (gradient with both colors)');
    if ((kinds.arc || 0) < 6) fail.push('arc (circle + annulus ring)');
    if (!kinds['imageSmoothingEnabled=true'])
        fail.push('imageSmoothingEnabled=true (bilinear texture scaling)');
    if (!kinds['imageSmoothingEnabled=false'])
        fail.push('imageSmoothingEnabled=false (nearest texture scaling)');
    if (globalThis.__kryTestCanvas.style.cursor !== 'pointer')
        fail.push('canvas style cursor pointer');
    if (fail.length) { console.error('SMOKE FAIL:', fail.join(', ')); process.exit(1); }
    console.log('canvas backend smoke ok');
    process.exit(0);
}, 1500);

// Node harness: recording Canvas2D shim + module run + assertions.
const calls = [];
const rec = (name) => function (...a) { calls.push(name); };
const grad = { addColorStop: rec('addColorStop') };
const ctx = new Proxy({
    canvas: {width: 320, height: 240},
    font: '', textBaseline: '', fillStyle: '', strokeStyle: '',
    lineWidth: 1, lineCap: '', globalAlpha: 1,
    fillRect: rec('fillRect'), strokeRect: rec('strokeRect'),
    clearRect: rec('clearRect'), beginPath: rec('beginPath'),
    arc: rec('arc'), fill: rec('fill'), stroke: rec('stroke'),
    moveTo: rec('moveTo'), lineTo: rec('lineTo'), closePath: rec('closePath'),
    drawImage: rec('drawImage'), save: rec('save'), restore: rec('restore'),
    clip: rec('clip'), rect: rec('rect'),
    translate: rec('translate'), rotate: rec('rotate'), scale: rec('scale'),
    setTransform: rec('setTransform'),
    createLinearGradient: () => grad,
    fillText: rec('fillText'),
    measureText: (t) => ({
        width: t.length * 8,
        actualBoundingBoxLeft: 0, actualBoundingBoxRight: t.length * 8,
        actualBoundingBoxAscent: 12, actualBoundingBoxDescent: 2
    }),
    getImageData: (x, y, w, h) => ({data: new Uint8ClampedArray(w * h * 4)}),
    putImageData: rec('putImageData'),
    createImageData: (w, h) => ({data: new Uint8ClampedArray(w * h * 4)})
}, {get: (t, k) => k in t ? t[k] : rec(String(k))});
globalThis.OffscreenCanvas = class {
    constructor(w, h) { this.width = w; this.height = h; }
    getContext() { return ctx; }
};
globalThis.__kryTestCanvas = {
    width: 320, height: 240,
    getContext: () => ctx,
    getBoundingClientRect: () => ({left: 0, top: 0})
};

require('./canvas_smoke.js');

setTimeout(() => {
    const kinds = {};
    for (const c of calls) kinds[c] = (kinds[c] || 0) + 1;
    console.log('recorded calls:', calls.length, JSON.stringify(kinds));
    const fail = [];
    if (!kinds.fillRect || kinds.fillRect < 6) fail.push('fillRect (background/rect per frame)');
    if (!kinds.strokeRect) fail.push('strokeRect (DrawRectangleLines)');
    if (!kinds.arc) fail.push('arc (DrawCircle)');
    if (!kinds.moveTo) fail.push('moveTo (DrawLine)');
    if (!kinds.drawImage) fail.push('drawImage (DrawText atlas blits)');
    if (fail.length) { console.error('SMOKE FAIL:', fail.join(', ')); process.exit(1); }
    console.log('canvas backend smoke ok');
    process.exit(0);
}, 1500);

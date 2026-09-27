// Browser font loading and glyph raster effects; atlas/layout policy stays in Zi.
addToLibrary({
  js_font_face_load__deps: ['$Asyncify'],
  js_font_face_load__async: true,
  js_font_face_load: (ptr, len) => Asyncify.handleAsync(async () => {
    var g = globalThis;
    if (typeof FontFace === 'undefined') return 0;
    try {
        var buf = new Uint8Array(HEAPU8.subarray(ptr, ptr + len));
        var face = new FontFace('kry-face-' + (g.__kryFaceCount || 0), buf);
        await face.load();
        if (g.document && g.document.fonts) g.document.fonts.add(face);
        g.__kryFaceCount = (g.__kryFaceCount || 0) + 1;
        if (!g.__kryFaces) g.__kryFaces = new Map();
        g.__kryFaces.set(g.__kryFaceCount, face);
        return g.__kryFaceCount;      /* 1-based face id */
    } catch (e) {
        return 0;
    }
  }),

  js_glyph_metrics: (face_id, cp, size, adv, w, h, offx, offy, ptr) => {
    var g = globalThis;
    if (!g.__kryGlyphCv) {
        if (typeof document !== 'undefined')
            g.__kryGlyphCv = document.createElement('canvas');
        else if (g.OffscreenCanvas)
            g.__kryGlyphCv = new OffscreenCanvas(64, 64);
        else return 0;
    }
    var cv = g.__kryGlyphCv;
    var ctx = cv.getContext('2d', {willReadFrequently: true});
    var spec = size + 'px ' + (face_id > 0
        ? 'kry-face-' + (face_id - 1) : 'monospace');
    ctx.font = spec;
    ctx.textBaseline = 'alphabetic';
    var ch = String.fromCodePoint(cp);
    var m = ctx.measureText(ch);
    var advX = Math.ceil(m.width);
    var glyphAsc = Math.ceil(m.actualBoundingBoxAscent !== undefined
                             ? m.actualBoundingBoxAscent : size * 0.8);
    var glyphDesc = Math.ceil(m.actualBoundingBoxDescent !== undefined
                              ? m.actualBoundingBoxDescent : 2);
    var fontAsc = Math.ceil(m.fontBoundingBoxAscent !== undefined
                            ? m.fontBoundingBoxAscent : size * 0.8);
    var fontDesc = Math.ceil(m.fontBoundingBoxDescent !== undefined
                             ? m.fontBoundingBoxDescent : Math.max(2, size * 0.2));
    var baseline = Math.max(fontAsc, glyphAsc) + 1;
    var left = Math.ceil(m.actualBoundingBoxLeft !== undefined
                         ? m.actualBoundingBoxLeft : 0);
    var right = Math.ceil(m.actualBoundingBoxRight !== undefined
                          ? m.actualBoundingBoxRight : m.width);
    var gw = Math.max(right + left + 2, 1);
    var gh = Math.max(baseline + Math.max(fontDesc, glyphDesc) + 1, 1);
    if (gw > 256 || gh > 256) return 0;
    if (cv.width < gw || cv.height < gh) {
        cv.width = Math.max(cv.width, gw);
        cv.height = Math.max(cv.height, gh);
        ctx = cv.getContext('2d', {willReadFrequently: true});
        ctx.font = spec;
        ctx.textBaseline = 'alphabetic';
    }
    ctx.clearRect(0, 0, gw, gh);
    ctx.fillStyle = '#fff';
    ctx.fillText(ch, left + 1, baseline);
    var d;
    try {
        d = ctx.getImageData(0, 0, gw, gh).data;
    } catch (e) {
        return 0;
    }
    HEAPU8.set(d.subarray(0, gw * gh * 4), ptr);
    HEAP32[adv >> 2] = advX;
    HEAP32[w >> 2] = gw;
    HEAP32[h >> 2] = gh;
    HEAP32[offx >> 2] = -(left + 1);
    HEAP32[offy >> 2] = 0;
    return 1;
  },

  js_text_transform: (begin, x, y, ox, oy, rotation) => {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    if (begin) {
        ctx.save();
        ctx.translate(x, y);
        if (rotation !== 0.0) ctx.rotate(rotation * Math.PI / 180.0);
        ctx.translate(-ox, -oy);
    } else {
        ctx.restore();
    }
  },

  js_font_face_release: id => {
    const g = globalThis;
    const face = g.__kryFaces?.get(id);
    if (!face) return;
    g.document?.fonts?.delete(face);
    g.__kryFaces.delete(id);
  }
});

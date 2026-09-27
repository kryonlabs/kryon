// Raw browser effects; state and exported host policy live in canvas_texture.zi.
addToLibrary({
  js_draw_texture_pro__sig: "vidddddddddddiiiii",
  js_draw_texture_pro: function(id, sx, sy, sw, sh, dx, dy, dw, dh, ox, oy, rot, r, gg, bb, aa, filter) {
    var K = globalThis.__kryonCanvas;
    if (!K || K.closed) return;
    var ctx = K.ctxNow();
    var tex = K.textures[id];
    if (!ctx || !tex) return;
    var flipX = false, flipY = false;
    /* Raylib re-anchors a negative source extent at the far corner and
     * mirrors it. Canvas textures are stored upright, so sprites mirror
     * via the flip transform below. Render targets are the exception:
     * raylib stores them bottom-up and the {0,0,w,-h} idiom exists to
     * draw them upright — on this backend that is a plain in-bounds copy
     * of |sh| rows starting at sy, with no mirror. */
    var isTarget = !!(K.targets && K.targets[id]);
    if (sw < 0) { sx += sw; sw = -sw; flipX = true; }
    if (sh < 0) {
        sh = -sh;
        if (!isTarget) { sy -= sh; flipY = true; }
    }
    if (dw < 0) { dw = -dw; flipX = !flipX; }
    if (dh < 0) { dh = -dh; flipY = !flipY; }
    var white = (r === 255 && gg === 255 && bb === 255 && aa === 255);
    if (!white) {
        /* tinted copies are cached per (texture, tint): multiply the RGB
         * channels, then restore the source alpha with destination-in */
        var key = id + ':' + r + ',' + gg + ',' + bb;
        if (!K.tints) K.tints = {};
        if (!K.tints[key]) {
            var tintKeys = Object.keys(K.tints);
            if (tintKeys.length >= 32) delete K.tints[tintKeys[0]];
            var cv = K.makeCanvas(tex.width, tex.height);
            var c2 = cv.getContext('2d');
            c2.imageSmoothingEnabled = false;
            c2.drawImage(tex, 0, 0);
            c2.globalCompositeOperation = 'multiply';
            c2.fillStyle = 'rgb(' + r + ',' + gg + ',' + bb + ')';
            c2.fillRect(0, 0, cv.width, cv.height);
            c2.globalCompositeOperation = 'destination-in';
            c2.drawImage(tex, 0, 0);
            K.tints[key] = cv;
        }
        tex = K.tints[key];
        // Bound retained tint pixels as well as entry count. Large images may
        // still be drawn once without pinning another copy in the cache.
        var retained=Object.keys(K.tints), bytes=retained.reduce(function(total,name) {
            var entry=K.tints[name]; return total+entry.width*entry.height*4;
        },0);
        while(bytes>16777216 && retained.length) {
            var oldest=retained.shift(), entry=K.tints[oldest];
            bytes-=entry.width*entry.height*4; delete K.tints[oldest];
        }
    }
    ctx.save();
    ctx.imageSmoothingEnabled = filter !== 0;
    if (aa < 255) ctx.globalAlpha = aa / 255.0;
    ctx.translate(dx + ox, dy + oy);
    if (rot !== 0.0) ctx.rotate(rot * Math.PI / 180.0);
    if (flipX || flipY) ctx.scale(flipX ? -1 : 1, flipY ? -1 : 1);
    ctx.drawImage(tex, sx, sy, sw, sh,
                  flipX ? ox - dw : -ox,
                  flipY ? oy - dh : -oy,
                  dw, dh);
    ctx.restore();
  },
  js_texture_from_rgba__sig: "iiii",
  js_texture_from_rgba: function(ptr, w, h) {
    var K = globalThis.__kryonCanvas;
    var cv = K.makeCanvas(w, h);
    if (!cv) return 0;
    var c2 = cv.getContext('2d');
    var img = c2.createImageData(w, h);
    img.data.set(HEAPU8.subarray(ptr, ptr + w * h * 4));
    c2.putImageData(img, 0, 0);
    var id = K.nextTex++;
    K.textures[id] = cv;
    return id;
  },
  js_texture_free__sig: "vi",
  js_texture_free: function(id) {
    var K = globalThis.__kryonCanvas;
    delete K.textures[id];
    if (K.tints) Object.keys(K.tints).forEach(function(key) { if(key.indexOf(id + ':') === 0) delete K.tints[key]; });
    if (K.filters) delete K.filters[id];
    if (K.targets) delete K.targets[id];
  },
  js_texture_update_rgba__sig: "viiiiii",
  js_texture_update_rgba: function(id, ptr, x, y, w, h) {
    var K = globalThis.__kryonCanvas;
    var cv = K && K.textures ? K.textures[id] : null;
    if (!cv || !cv.getContext || !ptr || w <= 0 || h <= 0) return;
    var c2 = cv.getContext('2d');
    var img = c2.createImageData(w, h);
    img.data.set(HEAPU8.subarray(ptr, ptr + w * h * 4));
    c2.putImageData(img, x, y);
    /* Tint copies contain pixels from the previous texture contents. */
    if (K.tints) K.tints = {};
  },
  js_texture_filter__sig: "vii",
  js_texture_filter: function(id, filter) {
    var K = globalThis.__kryonCanvas;
    if (!K || !K.textures[id]) return;
    if (!K.filters) K.filters = {};
    K.filters[id] = filter | 0;
  },
  js_texture_filter_for__sig: "ii",
  js_texture_filter_for: function(id) {
    var K = globalThis.__kryonCanvas;
    if (!K || !K.filters || K.filters[id] === undefined) return 0;
    return K.filters[id] | 0;
  },
  js_render_target__sig: "iii",
  js_render_target: function(w, h) {
    var K = globalThis.__kryonCanvas;
    var cv = K.makeCanvas(w, h);
    if (!cv) return 0;
    var id = K.nextTex++;
    K.textures[id] = cv;
    if (!K.targets) K.targets = {};
    K.targets[id] = 1;
    return id;
  },
  js_target_select__sig: "vi",
  js_target_select: function(id) {
    var K = globalThis.__kryonCanvas;
    var cv = K.textures[id];
    if (cv) {
        K.target.push({canvas:cv,ctx:cv.getContext('2d'),mode2D:K.mode2D,saved:K.saved,rasterClips:K.rasterClips||0});
        K.mode2D=null; K.saved=0; K.rasterClips=0;
        // Cached tint pixels become stale as soon as a render target is written.
        if(K.tints) Object.keys(K.tints).forEach(function(key) { if(key.indexOf(id + ':') === 0) delete K.tints[key]; });
    }
  },
  js_target_deselect__sig: "v",
  js_target_deselect: function() {
    var K=globalThis.__kryonCanvas;
    if(!K || !K.target.length) return;
    var state=K.target.pop();
    K.mode2D=state.mode2D; K.saved=state.saved; K.rasterClips=state.rasterClips;
  },
  js_texture_read__sig: "iiii",
  js_texture_read: function(id, ptr, capacity) {
    var K = globalThis.__kryonCanvas;
    var cv = id === 0 ? K.canvas : K.textures[id];
    if (!cv || !cv.getContext) return 0;
    var d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data;
    if (d.length > capacity || ptr < 0 || ptr + d.length > HEAPU8.length) return 0;
    HEAPU8.set(d, ptr);
    return 1;
  },
  js_image_decode__async: true,
  js_image_decode__deps: ['$Asyncify','malloc'],
  js_image_decode__sig: 'iiiii',
  js_image_decode: function(data, size, width, height) {
    var encoded=HEAPU8.slice(data,data+size);
    return Asyncify.handleAsync(async function() {
      var bitmap=null;
      try {
        bitmap=await createImageBitmap(new Blob([encoded]));
        var w=bitmap.width, h=bitmap.height;
        if(w<=0 || h<=0 || w>16384 || h>16384 || w*h*4>67108864) return 0;
        var K=globalThis.__kryonCanvas;
        var canvas=K && K.makeCanvas ? K.makeCanvas(w,h) : new OffscreenCanvas(w,h);
        var ctx=canvas.getContext('2d'); ctx.drawImage(bitmap,0,0);
        var pixels=ctx.getImageData(0,0,w,h).data, ptr=_malloc(pixels.length);
        if(!ptr) return 0;
        HEAPU8.set(pixels,ptr); HEAP32[width>>2]=w; HEAP32[height>>2]=h;
        return ptr;
      } catch(error) { return 0; }
      finally { if(bitmap && bitmap.close) bitmap.close(); }
    });
  },
  js_image_export__async: true,
  js_image_export__deps: ['$FS','$UTF8ToString','$Asyncify'],
  js_image_export__sig: 'iiiii',
  js_image_export: function(path, data, width, height) {
    var name=UTF8ToString(path), pixels=HEAPU8.slice(data,data+width*height*4);
    return Asyncify.handleAsync(async function() {
      try {
        var K=globalThis.__kryonCanvas, canvas=K.makeCanvas(width,height), ctx=canvas.getContext('2d');
        var image=ctx.createImageData(width,height); image.data.set(pixels); ctx.putImageData(image,0,0);
        var blob=canvas.convertToBlob ? await canvas.convertToBlob({type:'image/png'}) : await new Promise(function(resolve) { canvas.toBlob(resolve,'image/png'); });
        if(!blob) return 0;
        FS.writeFile(name,new Uint8Array(await blob.arrayBuffer())); return 1;
      } catch(error) { return 0; }
    });
  },
});

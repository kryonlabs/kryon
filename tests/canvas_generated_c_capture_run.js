var Module = typeof Module !== "undefined" ? Module : {};

(function() {
  var nodeFs = typeof require === "function" ? require("fs") : null;
  var stats = {};

  function bump(name) {
    stats[name] = (stats[name] || 0) + 1;
  }

  if (typeof process !== "undefined" && process.on) {
    process.on("unhandledRejection", function(reason) {
      console.error("canvas-capture: unhandled rejection:", reason && reason.stack ? reason.stack : reason);
      process.exit(1);
    });
  }

  function clamp(v, lo, hi) {
    v = Math.floor(Number(v) || 0);
    return v < lo ? lo : v > hi ? hi : v;
  }

  function parseColor(value) {
    if (value && value.__kryColor) {
      return value.__kryColor;
    }
    if (typeof value !== "string") {
      return [0, 0, 0, 255];
    }
    var m = value.match(/rgba?\(([^)]+)\)/);
    if (!m) {
      return [0, 0, 0, 255];
    }
    var p = m[1].split(",").map(function(x) { return x.trim(); });
    return [
      clamp(p[0], 0, 255),
      clamp(p[1], 0, 255),
      clamp(p[2], 0, 255),
      p.length > 3 ? clamp(parseFloat(p[3]) * 255, 0, 255) : 255
    ];
  }

  function makeGradient() {
    return {
      __kryColor: [0, 0, 0, 255],
      addColorStop: function(_offset, color) {
        this.__kryColor = parseColor(color);
      }
    };
  }

  /* Affine matrix helpers: m = [a, b, c, d, e, f] mapping
   * x' = a*x + c*y + e ; y' = b*x + d*y + f (canvas order). */
  function matIdentity() {
    return [1, 0, 0, 1, 0, 0];
  }

  function matMul(m, n) {
    /* Return m * n (apply n first, then m). */
    return [
      m[0] * n[0] + m[2] * n[1],
      m[1] * n[0] + m[3] * n[1],
      m[0] * n[2] + m[2] * n[3],
      m[1] * n[2] + m[3] * n[3],
      m[0] * n[4] + m[2] * n[5] + m[4],
      m[1] * n[4] + m[3] * n[5] + m[5]
    ];
  }

  function matApply(m, x, y) {
    return [m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]];
  }

  function matInvert(m) {
    var det = m[0] * m[3] - m[1] * m[2];
    if (!det || !isFinite(det)) {
      return null;
    }
    var invDet = 1 / det;
    return [
      m[3] * invDet,
      -m[1] * invDet,
      -m[2] * invDet,
      m[0] * invDet,
      (m[2] * m[5] - m[3] * m[4]) * invDet,
      (m[1] * m[4] - m[0] * m[5]) * invDet
    ];
  }

  function TestCanvas(w, h) {
    this.style = {};
    this.parentElement = {style: {}};
    this._context = null;
    this._width = 0;
    this._height = 0;
    this._pixels = new Uint8ClampedArray(0);
    this._resize(w || 1, h || 1);
  }

  TestCanvas.prototype._resize = function(w, h) {
    this._width = Math.max(1, w | 0);
    this._height = Math.max(1, h | 0);
    this._pixels = new Uint8ClampedArray(this._width * this._height * 4);
  };

  Object.defineProperty(TestCanvas.prototype, "width", {
    get: function() { return this._width; },
    set: function(v) { this._resize(v, this._height); }
  });

  Object.defineProperty(TestCanvas.prototype, "height", {
    get: function() { return this._height; },
    set: function(v) { this._resize(this._width, v); }
  });

  TestCanvas.prototype.getContext = function() {
    if (!this._context) {
      this._context = new TestContext(this);
    }
    return this._context;
  };

  TestCanvas.prototype.getBoundingClientRect = function() {
    return {left: 0, top: 0, width: this.width, height: this.height};
  };

  TestCanvas.prototype.addEventListener = function() {};

  function TestContext(canvas) {
    this.canvas = canvas;
    this.fillStyle = "rgba(0,0,0,1)";
    this.strokeStyle = "rgba(0,0,0,1)";
    this.globalAlpha = 1;
    this.lineWidth = 1;
    this.lineCap = "butt";
    this.font = "";
    this.textBaseline = "";
    this.imageSmoothingEnabled = true;
    this._path = null;
    this._stack = [];
    this._m = matIdentity();
    this._clip = null;
  }

  TestContext.prototype._color = function(style) {
    var c = parseColor(style);
    c = c.slice();
    c[3] = clamp(c[3] * this.globalAlpha, 0, 255);
    return c;
  };

  TestContext.prototype._setPixel = function(dx, dy, c) {
    /* Write one device-space pixel honoring the active clip. Composites
     * source-over like a real canvas instead of overwriting, so translucent
     * surfaces and antialiased glyphs stack correctly. */
    if (dx < 0 || dy < 0 || dx >= this.canvas.width || dy >= this.canvas.height) {
      return;
    }
    var clip = this._clip;
    if (clip && (dx < clip[0] || dy < clip[1] || dx >= clip[2] || dy >= clip[3])) {
      return;
    }
    var i = (dy * this.canvas.width + dx) * 4;
    var pixels = this.canvas._pixels;
    var sa = c[3];
    if (sa >= 255) {
      pixels[i + 0] = c[0];
      pixels[i + 1] = c[1];
      pixels[i + 2] = c[2];
      pixels[i + 3] = 255;
      return;
    }
    if (sa <= 0) {
      return;
    }
    var da = pixels[i + 3];
    var outA = sa + da * (255 - sa) / 255;
    if (outA <= 0) {
      return;
    }
    pixels[i + 0] = (c[0] * sa + pixels[i + 0] * da * (255 - sa) / 255) / outA;
    pixels[i + 1] = (c[1] * sa + pixels[i + 1] * da * (255 - sa) / 255) / outA;
    pixels[i + 2] = (c[2] * sa + pixels[i + 2] * da * (255 - sa) / 255) / outA;
    pixels[i + 3] = outA;
  };

  TestContext.prototype._put = function(x, y, c) {
    /* User-space put: apply the current transform to the point. */
    var p = matApply(this._m, x + 0.5, y + 0.5);
    this._setPixel(Math.floor(p[0]), Math.floor(p[1]), c);
  };

  /* Iterate device pixels covered by the transformed user-space rectangle
   * [x, y, w, h]. For each device pixel, invoke cb(userX, userY) with the
   * inverse-mapped user-space coordinates of the pixel center. */
  TestContext.prototype._forRectPixels = function(x, y, w, h, cb) {
    var m = this._m;
    var corners = [
      matApply(m, x, y),
      matApply(m, x + w, y),
      matApply(m, x, y + h),
      matApply(m, x + w, y + h)
    ];
    var minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    for (var i = 0; i < 4; i++) {
      if (corners[i][0] < minX) minX = corners[i][0];
      if (corners[i][0] > maxX) maxX = corners[i][0];
      if (corners[i][1] < minY) minY = corners[i][1];
      if (corners[i][1] > maxY) maxY = corners[i][1];
    }
    var clip = this._clip;
    if (clip) {
      minX = Math.max(minX, clip[0]);
      minY = Math.max(minY, clip[1]);
      maxX = Math.min(maxX, clip[2]);
      maxY = Math.min(maxY, clip[3]);
    }
    var inv = matInvert(m);
    if (!inv) {
      return;
    }
    var x0 = Math.max(0, Math.floor(minX));
    var y0 = Math.max(0, Math.floor(minY));
    var x1 = Math.min(this.canvas.width, Math.ceil(maxX));
    var y1 = Math.min(this.canvas.height, Math.ceil(maxY));
    for (var dy = y0; dy < y1; dy++) {
      for (var dx = x0; dx < x1; dx++) {
        var u = matApply(inv, dx + 0.5, dy + 0.5);
        if (u[0] < x || u[0] > x + w || u[1] < y || u[1] > y + h) {
          continue;
        }
        cb(dx, dy, u[0] - x, u[1] - y);
      }
    }
  };

  TestContext.prototype.fillRect = function(x, y, w, h) {
    bump("fillRect");
    var c = this._color(this.fillStyle);
    this._forRectPixels(x, y, w, h, function(dx, dy) {
      this._setPixel(dx, dy, c);
    }.bind(this));
  };

  TestContext.prototype.clearRect = function(x, y, w, h) {
    var old = this.fillStyle;
    this.fillStyle = "rgba(0,0,0,0)";
    this.fillRect(x, y, w, h);
    this.fillStyle = old;
  };

  TestContext.prototype.strokeRect = function(x, y, w, h) {
    bump("strokeRect");
    var lw = Math.max(1, Math.round(this.lineWidth || 1));
    var c = this.strokeStyle;
    var old = this.fillStyle;
    this.fillStyle = c;
    this.fillRect(x, y, w, lw);
    this.fillRect(x, y + h - lw, w, lw);
    this.fillRect(x, y, lw, h);
    this.fillRect(x + w - lw, y, lw, h);
    this.fillStyle = old;
  };

  TestContext.prototype.beginPath = function() {
    this._path = [];
  };

  TestContext.prototype.rect = function(x, y, w, h) {
    this._path = [{type: "rect", x: x, y: y, w: w, h: h}];
  };

  TestContext.prototype.roundRect = function(x, y, w, h) {
    this.rect(x, y, w, h);
  };

  TestContext.prototype.arc = function(x, y, r) {
    this._path = [{type: "circle", x: x, y: y, r: r}];
  };

  TestContext.prototype.moveTo = function(x, y) {
    this._path = [{type: "line", x0: x, y0: y, x1: x, y1: y}];
  };

  TestContext.prototype.lineTo = function(x, y) {
    if (!this._path || !this._path.length) {
      this.moveTo(x, y);
      return;
    }
    var p = this._path[this._path.length - 1];
    if (p.type === "line") {
      p.x1 = x;
      p.y1 = y;
    }
  };

  TestContext.prototype.closePath = function() {};

  TestContext.prototype.fill = function() {
    bump("fill");
    if (!this._path || !this._path.length) {
      return;
    }
    var p = this._path[0];
    if (p.type === "rect") {
      this.fillRect(p.x, p.y, p.w, p.h);
    } else if (p.type === "circle") {
      var c = this._color(this.fillStyle);
      var r = Math.max(0, p.r | 0);
      for (var y = -r; y <= r; y++) {
        for (var x = -r; x <= r; x++) {
          if (x * x + y * y <= r * r) {
            this._put(p.x + x, p.y + y, c);
          }
        }
      }
    }
  };

  TestContext.prototype.stroke = function() {
    bump("stroke");
    if (!this._path || !this._path.length) {
      return;
    }
    var p = this._path[0];
    if (p.type === "line") {
      this._line(p.x0, p.y0, p.x1, p.y1, this._color(this.strokeStyle));
    } else if (p.type === "rect") {
      this.strokeRect(p.x, p.y, p.w, p.h);
    } else if (p.type === "circle") {
      this.strokeRect(p.x - p.r, p.y - p.r, p.r * 2, p.r * 2);
    }
  };

  TestContext.prototype._line = function(x0, y0, x1, y1, c) {
    x0 = Math.round(x0); y0 = Math.round(y0);
    x1 = Math.round(x1); y1 = Math.round(y1);
    var dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    var dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    var err = dx + dy;
    for (;;) {
      this._put(x0, y0, c);
      if (x0 === x1 && y0 === y1) break;
      var e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  };

  TestContext.prototype.drawImage = function(src) {
    bump("drawImage");
    var sx = 0, sy = 0, sw = src.width, sh = src.height;
    var dx = 0, dy = 0, dw = sw, dh = sh;
    if (arguments.length === 3) {
      dx = arguments[1]; dy = arguments[2];
    } else if (arguments.length === 5) {
      dx = arguments[1]; dy = arguments[2]; dw = arguments[3]; dh = arguments[4];
    } else if (arguments.length >= 9) {
      sx = arguments[1]; sy = arguments[2]; sw = arguments[3]; sh = arguments[4];
      dx = arguments[5]; dy = arguments[6]; dw = arguments[7]; dh = arguments[8];
    }
    if (!src || !src._pixels) {
      return;
    }
    if (dw <= 0 || dh <= 0) {
      return;
    }
    var alpha = this.globalAlpha;
    var self = this;
    this._forRectPixels(dx, dy, dw, dh, function(ddx, ddy, u, v) {
      var tx = clamp(sx + Math.floor(u * sw / Math.max(1, dw)), 0, src.width - 1);
      var ty = clamp(sy + Math.floor(v * sh / Math.max(1, dh)), 0, src.height - 1);
      var si = (ty * src.width + tx) * 4;
      self._setPixel(ddx, ddy, [
        src._pixels[si + 0],
        src._pixels[si + 1],
        src._pixels[si + 2],
        clamp(src._pixels[si + 3] * alpha / 255, 0, 255)
      ]);
    });
  };

  TestContext.prototype.createImageData = function(w, h) {
    return {data: new Uint8ClampedArray(w * h * 4), width: w, height: h};
  };

  TestContext.prototype.getImageData = function(x, y, w, h) {
    bump("getImageData");
    var out = this.createImageData(w, h);
    for (var yy = 0; yy < h; yy++) {
      for (var xx = 0; xx < w; xx++) {
        var sx = x + xx, sy = y + yy;
        if (sx < 0 || sy < 0 || sx >= this.canvas.width || sy >= this.canvas.height) continue;
        var si = (sy * this.canvas.width + sx) * 4;
        var di = (yy * w + xx) * 4;
        out.data[di + 0] = this.canvas._pixels[si + 0];
        out.data[di + 1] = this.canvas._pixels[si + 1];
        out.data[di + 2] = this.canvas._pixels[si + 2];
        out.data[di + 3] = this.canvas._pixels[si + 3];
      }
    }
    return out;
  };

  TestContext.prototype.putImageData = function(img, x, y) {
    bump("putImageData");
    /* ImageData transfer ignores the transform and clip per the canvas spec. */
    var w = img.width || Math.sqrt(img.data.length / 4) | 0;
    var h = img.height || (img.data.length / 4 / Math.max(1, w)) | 0;
    for (var yy = 0; yy < h; yy++) {
      for (var xx = 0; xx < w; xx++) {
        var dx = x + xx, dy = y + yy;
        if (dx < 0 || dy < 0 || dx >= this.canvas.width || dy >= this.canvas.height) continue;
        var si = (yy * w + xx) * 4;
        var di = (dy * this.canvas.width + dx) * 4;
        this.canvas._pixels[di + 0] = img.data[si + 0];
        this.canvas._pixels[di + 1] = img.data[si + 1];
        this.canvas._pixels[di + 2] = img.data[si + 2];
        this.canvas._pixels[di + 3] = img.data[si + 3];
      }
    }
  };

  TestContext.prototype.save = function() {
    this._stack.push({
      fillStyle: this.fillStyle,
      strokeStyle: this.strokeStyle,
      globalAlpha: this.globalAlpha,
      lineWidth: this.lineWidth,
      m: this._m.slice(),
      clip: this._clip ? this._clip.slice() : null
    });
  };

  TestContext.prototype.restore = function() {
    var s = this._stack.pop();
    if (!s) return;
    this.fillStyle = s.fillStyle;
    this.strokeStyle = s.strokeStyle;
    this.globalAlpha = s.globalAlpha;
    this.lineWidth = s.lineWidth;
    this._m = s.m;
    this._clip = s.clip;
  };

  TestContext.prototype.translate = function(x, y) {
    this._m = matMul(this._m, [1, 0, 0, 1, x, y]);
  };

  TestContext.prototype.scale = function(x, y) {
    this._m = matMul(this._m, [x, 0, 0, y, 0, 0]);
  };

  TestContext.prototype.rotate = function(angle) {
    var cos = Math.cos(angle), sin = Math.sin(angle);
    this._m = matMul(this._m, [cos, sin, -sin, cos, 0, 0]);
  };

  TestContext.prototype.transform = function(a, b, c, d, e, f) {
    this._m = matMul(this._m, [a, b, c, d, e, f]);
  };

  TestContext.prototype.setTransform = function(a, b, c, d, e, f) {
    if (arguments.length >= 6) {
      this._m = [a, b, c, d, e, f];
    } else {
      this._m = matIdentity();
    }
  };

  TestContext.prototype.resetTransform = function() {
    this._m = matIdentity();
  };

  TestContext.prototype.clip = function() {
    /* Approximate the current path with its transformed device-space
     * bounding box, intersected with the active clip. */
    if (!this._path || !this._path.length) {
      return;
    }
    var p = this._path[0];
    var x = p.x, y = p.y, w = p.w, h = p.h;
    if (p.type === "circle") {
      x = p.x - p.r; y = p.y - p.r; w = p.r * 2; h = p.r * 2;
    } else if (p.type === "line") {
      x = Math.min(p.x0, p.x1); y = Math.min(p.y0, p.y1);
      w = Math.abs(p.x1 - p.x0); h = Math.abs(p.y1 - p.y0);
    }
    var c0 = matApply(this._m, x, y);
    var c1 = matApply(this._m, x + w, y + h);
    var minX = Math.floor(Math.min(c0[0], c1[0]));
    var minY = Math.floor(Math.min(c0[1], c1[1]));
    var maxX = Math.ceil(Math.max(c0[0], c1[0]));
    var maxY = Math.ceil(Math.max(c0[1], c1[1]));
    if (this._clip) {
      minX = Math.max(minX, this._clip[0]);
      minY = Math.max(minY, this._clip[1]);
      maxX = Math.min(maxX, this._clip[2]);
      maxY = Math.min(maxY, this._clip[3]);
    }
    this._clip = [minX, minY, maxX, maxY];
  };

  TestContext.prototype._fontSize = function() {
    var m = /(\d+(?:\.\d+)?)px/.exec(this.font || "");
    return m ? Math.max(1, parseFloat(m[1])) : 12;
  };

  TestContext.prototype.createLinearGradient = makeGradient;
  TestContext.prototype.measureText = function(text) {
    /* Font-size aware approximation so glyph boxes track the requested
     * pixel size instead of a fixed 8x14 cell. */
    var size = this._fontSize();
    var width = String(text || "").length * size * 0.6;
    return {
      width: width,
      actualBoundingBoxLeft: 0,
      actualBoundingBoxRight: width,
      actualBoundingBoxAscent: size * 0.8,
      actualBoundingBoxDescent: size * 0.2
    };
  };
  TestContext.prototype.fillText = function(text, x, y) {
    var size = this._fontSize();
    this.fillRect(x, y - size * 0.8, String(text || "").length * size * 0.6, size);
  };

  globalThis.OffscreenCanvas = TestCanvas;
  globalThis.__kryTestCanvas = new TestCanvas(1, 1);

  /* The wasm side probes matchMedia('(prefers-color-scheme: dark)') for the
   * system theme. Node has no matchMedia; default to dark so captures match
   * the dark GTK environment the raylib reference captures run under.
   * Set KRYON_CAPTURE_PREFERS_DARK=0 to capture the light palette. */
  if (typeof globalThis.matchMedia !== "function") {
    globalThis.matchMedia = function(query) {
      var dark = /\(prefers-color-scheme:\s*dark\)/.test(String(query || ""));
      var enabled = process.env.KRYON_CAPTURE_PREFERS_DARK !== "0";
      return {
        matches: !!(dark && enabled),
        media: query,
        onchange: null,
        addListener: function() {},
        removeListener: function() {},
        addEventListener: function() {},
        removeEventListener: function() {},
        dispatchEvent: function() { return false; }
      };
    };
  }

  function exportCapture() {
    if (!nodeFs) {
      return;
    }
    var out = process.env.KRYON_CANVAS_CAPTURE_OUT;
    if (!out) {
      return;
    }
    var fsApi = typeof FS !== "undefined" ? FS : Module.FS;
    try {
      if (process.env.KRYON_CANVAS_CAPTURE_DEBUG && globalThis.__kryCanvas) {
        var cv = globalThis.__kryCanvas.canvas;
        var pixels = cv && cv._pixels ? cv._pixels : new Uint8ClampedArray(0);
        var first = pixels.length >= 4 ? Array.prototype.slice.call(pixels, 0, 4).join(",") : "none";
        var varied = 0;
        for (var i = 4; i < pixels.length; i += 4) {
          if (pixels[i] !== pixels[0] || pixels[i + 1] !== pixels[1] ||
              pixels[i + 2] !== pixels[2] || pixels[i + 3] !== pixels[3]) {
            varied = 1;
            break;
          }
        }
        console.error("canvas-capture-debug:", JSON.stringify(stats), "first=" + first, "varied=" + varied);
      }
      var data = fsApi.readFile("/canvas-capture.png");
      nodeFs.writeFileSync(out, Buffer.from(data));
    } catch (e) {
      console.error("canvas-capture: cannot export /canvas-capture.png:", e && e.message ? e.message : e);
      process.exit(1);
    }
    process.exit(0);
  }

  Module.postRun = Module.postRun || [];
  Module.postRun.push(function() {
    setTimeout(exportCapture, 1500);
  });
})();

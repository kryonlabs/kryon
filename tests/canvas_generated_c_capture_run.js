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
  }

  TestContext.prototype._color = function(style) {
    var c = parseColor(style);
    c = c.slice();
    c[3] = clamp(c[3] * this.globalAlpha, 0, 255);
    return c;
  };

  TestContext.prototype._put = function(x, y, c) {
    x = x | 0;
    y = y | 0;
    if (x < 0 || y < 0 || x >= this.canvas.width || y >= this.canvas.height) {
      return;
    }
    var i = (y * this.canvas.width + x) * 4;
    this.canvas._pixels[i + 0] = c[0];
    this.canvas._pixels[i + 1] = c[1];
    this.canvas._pixels[i + 2] = c[2];
    this.canvas._pixels[i + 3] = c[3];
  };

  TestContext.prototype.fillRect = function(x, y, w, h) {
    bump("fillRect");
    var c = this._color(this.fillStyle);
    var x0 = clamp(Math.floor(x), 0, this.canvas.width);
    var y0 = clamp(Math.floor(y), 0, this.canvas.height);
    var x1 = clamp(Math.ceil(x + w), 0, this.canvas.width);
    var y1 = clamp(Math.ceil(y + h), 0, this.canvas.height);
    for (var yy = y0; yy < y1; yy++) {
      for (var xx = x0; xx < x1; xx++) {
        this._put(xx, yy, c);
      }
    }
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
    var alpha = this.globalAlpha;
    for (var y = 0; y < dh; y++) {
      for (var x = 0; x < dw; x++) {
        var tx = clamp(sx + Math.floor(x * sw / Math.max(1, dw)), 0, src.width - 1);
        var ty = clamp(sy + Math.floor(y * sh / Math.max(1, dh)), 0, src.height - 1);
        var si = (ty * src.width + tx) * 4;
        this._put(dx + x, dy + y, [
          src._pixels[si + 0],
          src._pixels[si + 1],
          src._pixels[si + 2],
          clamp(src._pixels[si + 3] * alpha / 255, 0, 255)
        ]);
      }
    }
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
    var w = img.width || Math.sqrt(img.data.length / 4) | 0;
    var h = img.height || (img.data.length / 4 / Math.max(1, w)) | 0;
    for (var yy = 0; yy < h; yy++) {
      for (var xx = 0; xx < w; xx++) {
        var si = (yy * w + xx) * 4;
        this._put(x + xx, y + yy, [
          img.data[si + 0], img.data[si + 1],
          img.data[si + 2], img.data[si + 3]
        ]);
      }
    }
  };

  TestContext.prototype.save = function() {
    this._stack.push({
      fillStyle: this.fillStyle,
      strokeStyle: this.strokeStyle,
      globalAlpha: this.globalAlpha,
      lineWidth: this.lineWidth
    });
  };

  TestContext.prototype.restore = function() {
    var s = this._stack.pop();
    if (!s) return;
    this.fillStyle = s.fillStyle;
    this.strokeStyle = s.strokeStyle;
    this.globalAlpha = s.globalAlpha;
    this.lineWidth = s.lineWidth;
  };

  TestContext.prototype.clip = function() {};
  TestContext.prototype.translate = function() {};
  TestContext.prototype.rotate = function() {};
  TestContext.prototype.scale = function() {};
  TestContext.prototype.setTransform = function() {};
  TestContext.prototype.createLinearGradient = makeGradient;
  TestContext.prototype.measureText = function(text) {
    var width = String(text || "").length * 8;
    return {
      width: width,
      actualBoundingBoxLeft: 0,
      actualBoundingBoxRight: width,
      actualBoundingBoxAscent: 12,
      actualBoundingBoxDescent: 2
    };
  };
  TestContext.prototype.fillText = function(text, x, y) {
    this.fillRect(x, y - 12, String(text || "").length * 8, 14);
  };

  globalThis.OffscreenCanvas = TestCanvas;
  globalThis.__kryTestCanvas = new TestCanvas(1, 1);

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

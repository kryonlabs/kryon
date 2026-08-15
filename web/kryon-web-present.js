/*
 * Kryon web present shim — WebKitGTK + software-GL survival mode.
 *
 * WebKitGTK (GNOME Web and other WebKit ports using its DMABUF compositor)
 * leaks several megabytes of compositor buffers per presented frame when the
 * WebGL canvas is composited on the software-GL path (llvmpipe/softpipe):
 * measured ~50 MB/s on a bare clear() loop, ~150 MB/s with a full app,
 * crashing the web process within minutes. The per-frame leak equals one
 * full-canvas RGBA buffer per composited canvas update; plain 2D-canvas
 * putImageData presentation allocates its surface once and stays flat
 * (measured: 0 MB growth at 58 fps).
 *
 * When (and only when) a WebKit-family browser AND a software GL renderer
 * are detected, this shim:
 *   - creates the WebGL context itself on a display:none canvas (a canvas
 *     that is never composited cannot leak through the compositor), with
 *     preserveDrawingBuffer so frames remain readable after present;
 *   - hands that context to Emscripten via Module.preinitializedWebGLContext
 *     and points Module.canvas at the visible canvas, so input, resize, and
 *     focus keep working through the normal raylib path;
 *   - after each app frame (rAF registered after Emscripten's own loop),
 *     reads the GL framebuffer into a reused buffer and blits it onto the
 *     visible canvas' 2D context — zero per-frame allocations.
 *
 * Chromium, Firefox, and hardware-GL WebKit never activate the shim and run
 * the normal WebGL present. Apps opt in with one script tag before their
 * Emscripten bootstrap:
 *
 *   <script src="kryon-web-present.js"></script>
 *
 * The visible canvas needs a 2D context on this path, so apps must not grab
 * another context type on it first.
 */
(function () {
  "use strict";

  var state = { active: false, reason: "" };
  window.__kryonWebPresent = state;

  function webkitFamily() {
    var ua = navigator.userAgent || "";
    if (/Epiphany\//.test(ua) || /WebKitGTK/.test(ua))
      return true;
    /* AppleWebKit without a Chromium engine marker = Safari-family */
    return /AppleWebKit/.test(ua) && !/Chrome|Chromium|Edg\//.test(ua);
  }

  function softwareRenderer() {
    var probe = document.createElement("canvas");
    var gl = probe.getContext("webgl") ||
             probe.getContext("experimental-webgl");
    if (!gl)
      return false;
    var ext = gl.getExtension("WEBGL_debug_renderer_info");
    if (!ext)
      return false;
    var renderer = "";
    try {
      renderer = String(gl.getParameter(ext.UNMASKED_RENDERER_WEBGL) || "");
    } catch (e) {
      return false;
    }
    /* lose the probe context eagerly */
    var lose = gl.getExtension("WEBGL_lose_context");
    if (lose)
      lose.loseContext();
    return /llvmpipe|softpipe|swiftshader|software/i.test(renderer);
  }

  function visibleCanvas() {
    var c = document.querySelector("canvas.emscripten") ||
            document.getElementById("canvas");
    return c instanceof HTMLCanvasElement ? c : null;
  }

  function activate(glCanvasSizeFrom) {
    var visible = visibleCanvas();
    if (!visible) {
      state.reason = "no canvas";
      return false;
    }

    var hidden = document.createElement("canvas");
    hidden.id = "kryon-gl-canvas";
    hidden.style.display = "none";
    hidden.width = visible.width > 0 ? visible.width : glCanvasSizeFrom;
    hidden.height = visible.height > 0 ? visible.height : 560;
    document.body.appendChild(hidden);

    var attrs = {
      alpha: false,
      depth: true,
      stencil: true,
      antialias: false,
      premultipliedAlpha: false,
      preserveDrawingBuffer: true,
      powerPreference: "default",
      failIfMajorPerformanceCaveat: false
    };
    var gl = hidden.getContext("webgl", attrs) ||
             hidden.getContext("experimental-webgl", attrs);
    if (!gl) {
      state.reason = "no webgl on hidden canvas";
      return false;
    }

    var ctx2d;
    try {
      ctx2d = visible.getContext("2d");
    } catch (e) {
      ctx2d = null;
    }
    if (!ctx2d) {
      state.reason = "visible canvas has no 2d context";
      return false;
    }

    var mod = window.Module || (window.Module = {});
    mod.canvas = visible;
    mod.preinitializedWebGLContext = gl;

    /* UNMASKED readback flip: GL is bottom-up, ImageData is top-down. */
    try {
      gl.pixelStorei(0x9240 /* PACK_FLIP_Y_WEBGL */, true);
    } catch (e) {
      /* without the flip the image presents upside down; still readable */
    }

    var px = null, img = null, w = 0, h = 0;

    function syncSize() {
      var cw = visible.width | 0, ch = visible.height | 0;
      if (cw > 0 && ch > 0 && (cw !== w || ch !== h)) {
        w = cw;
        h = ch;
        hidden.width = w;
        hidden.height = h;
        px = new Uint8Array(w * h * 4);
        img = new ImageData(new Uint8ClampedArray(px.buffer), w, h);
      }
    }

    function present() {
      requestAnimationFrame(present);
      syncSize();
      if (document.hidden || w === 0)
        return;
      try {
        gl.readPixels(0, 0, w, h, gl.RGBA, gl.UNSIGNED_BYTE, px);
        ctx2d.putImageData(img, 0, 0);
      } catch (e) {
        /* context lost or mismatched size: keep the loop, syncSize retries */
      }
    }

    /* Register AFTER Emscripten's main loop so each rAF tick presents the
     * frame that was just rendered, not the previous one. postRun fires
     * after main() has started the loop. */
    var started = false;
    function start() {
      if (started)
        return;
      started = true;
      syncSize();
      requestAnimationFrame(present);
    }
    var prevPostRun = mod.postRun;
    mod.postRun = function () {
      if (typeof prevPostRun === "function")
        prevPostRun();
      start();
    };
    /* fallback if the app overrides postRun after us or never fires it */
    setTimeout(start, 2000);

    state.active = true;
    state.reason = "presenting via 2d canvas";
    return true;
  }

  function boot() {
    try {
      if (!webkitFamily()) {
        state.reason = "not webkit";
        return;
      }
      if (!softwareRenderer()) {
        state.reason = "hardware gl";
        return;
      }
      activate(320);
    } catch (e) {
      state.reason = "error: " + (e && e.message ? e.message : e);
    }
  }

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", boot, { once: true });
  else
    boot();
})();

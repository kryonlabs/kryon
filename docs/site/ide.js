(function() {
  "use strict";

  var source = document.querySelector("[data-source]");
  var status = document.querySelector("[data-status]");
  var previewStatus = document.querySelector("[data-preview-status]");
  var canvas = document.querySelector("[data-preview]");
  var artifact = document.querySelector("[data-artifact]");
  var tabButtons = Array.prototype.slice.call(document.querySelectorAll("[data-tab]"));
  var compileButton = document.querySelector("[data-action='compile']");
  var sampleButton = document.querySelector("[data-action='load-sample']");
  var menuToggle = document.querySelector(".menu-toggle");
  var headerNav = document.getElementById("header-nav");
  var k2irMod = null;
  var k2bMod = null;
  var activeTab = "kir";
  var last = { kir: "", krb: "", c: "", bytes: null };
  var compileTimer = 0;

  var sample = [
    "#import \"kryon.h\"",
    "",
    "app {",
    "    title: \"Kryon Web IDE\"",
    "    width: 800",
    "    height: 520",
    "    frame: App",
    "}",
    "",
    "App :: () {",
    "    Background((Color){249, 246, 235, 255})",
    "    Rect(48, 42, 704, 390, (Color){255, 254, 249, 255})",
    "    Text(\"Hello from .kry\", 76, 86, UI_TEXT_24, (Color){31, 83, 102, 255})",
    "    Text(\"This page runs k2ir and k2b as WebAssembly, then renders the KRB cartridge.\", 76, 134, UI_TEXT_16, (Color){42, 59, 64, 255})",
    "    Rect(76, 188, 292, 80, (Color){35, 101, 125, 255})",
    "    Text(\"Portable preview\", 104, 219, UI_TEXT_20, WHITE)",
    "    Button((ButtonProps){.bounds = (Rectangle){76, 308, 184, 44}, .label = \"Get started\"})",
    "}"
  ].join("\n");
  source.value = sample;

  if (menuToggle && headerNav) {
    menuToggle.addEventListener("click", function() {
      var open = headerNav.classList.toggle("is-open");
      menuToggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
  }

  function setStatus(text) {
    if (status) status.textContent = text;
  }

  function setPreviewStatus(text) {
    if (previewStatus) previewStatus.textContent = text;
  }

  function ensureDir(mod, path) {
    var parts = path.split("/");
    var cur = "";
    for (var i = 0; i < parts.length; i++) {
      if (!parts[i]) continue;
      cur += "/" + parts[i];
      try {
        mod.FS.mkdir(cur);
      } catch (e) {
        if (!String(e).match(/File exists|ErrnoError/)) throw e;
      }
    }
  }

  function resetFs(mod) {
    ["/work", "/work/src", "/work/out"].forEach(function(path) {
      try { mod.FS.rmdir(path); } catch (e) {}
    });
    ensureDir(mod, "/work/src");
    ensureDir(mod, "/work/out");
  }

  function readMaybe(mod, path, binary) {
    try {
      return mod.FS.readFile(path, binary ? { encoding: "binary" } : { encoding: "utf8" });
    } catch (e) {
      return binary ? new Uint8Array(0) : "";
    }
  }

  function readFirst(mod, paths, binary) {
    for (var i = 0; i < paths.length; i++) {
      var value = readMaybe(mod, paths[i], binary);
      if (binary ? value.length > 0 : value !== "") return value;
    }
    return binary ? new Uint8Array(0) : "";
  }

  function hex(bytes) {
    var out = [];
    for (var i = 0; i < bytes.length; i += 16) {
      var line = ("00000000" + i.toString(16)).slice(-8) + "  ";
      for (var j = 0; j < 16; j++) {
        line += i + j < bytes.length ? ("0" + bytes[i + j].toString(16)).slice(-2) + " " : "   ";
      }
      out.push(line);
    }
    return out.join("\n");
  }

  function readU16(dv, off) {
    return dv.getUint16(off, true);
  }

  function readI16(dv, off) {
    return dv.getInt16(off, true);
  }

  function readU32(dv, off) {
    return dv.getUint32(off, true);
  }

  function cstr(bytes, off, max) {
    var end = off;
    while (end < max && bytes[end] !== 0) end++;
    return new TextDecoder().decode(bytes.slice(off, end));
  }

  function rgba(u32) {
    if ((u32 & 0x80000000) !== 0) {
      var slot = u32 & 0xff;
      if (slot === 1) return "rgb(42,59,64)";
      if (slot === 4) return "rgb(13,63,85)";
      return "rgb(247,244,236)";
    }
    return "rgba(" + (u32 & 255) + "," + ((u32 >> 8) & 255) + "," + ((u32 >> 16) & 255) + "," + (((u32 >> 24) & 255) / 255).toFixed(3) + ")";
  }

  function decodeKrb(bytes) {
    if (!bytes || bytes.length < 32) throw new Error("KRB is empty");
    var dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    if (readU32(dv, 0) !== 0x0042524b) throw new Error("KRB magic mismatch");
    var nodeCount = readU32(dv, 8);
    var stringBytes = readU32(dv, 12);
    var nodeOff = 32;
    var strOff = nodeOff + nodeCount * 28;
    var nodes = [];
    for (var i = 0; i < nodeCount; i++) {
      var off = nodeOff + i * 28;
      nodes.push({
        id: readU16(dv, off),
        parent: readI16(dv, off + 2),
        name: cstr(bytes, strOff + readU16(dv, off + 4), strOff + stringBytes),
        type: bytes[off + 6],
        flags: bytes[off + 7],
        bind: readU16(dv, off + 8),
        x: readI16(dv, off + 10),
        y: readI16(dv, off + 12),
        w: readI16(dv, off + 14),
        h: readI16(dv, off + 16),
        color: readU32(dv, off + 18),
        text: cstr(bytes, strOff + readU16(dv, off + 22), strOff + stringBytes),
        font: readU16(dv, off + 24) || 16,
        style: bytes[off + 26]
      });
    }
    return nodes;
  }

  function coord(v, full, scaled) {
    return scaled ? Math.round(full * v / 10000) : v;
  }

  function drawRound(ctx, x, y, w, h, r) {
    r = Math.max(0, Math.min(r, w / 2, h / 2));
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.lineTo(x + w - r, y);
    ctx.quadraticCurveTo(x + w, y, x + w, y + r);
    ctx.lineTo(x + w, y + h - r);
    ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
    ctx.lineTo(x + r, y + h);
    ctx.quadraticCurveTo(x, y + h, x, y + h - r);
    ctx.lineTo(x, y + r);
    ctx.quadraticCurveTo(x, y, x + r, y);
    ctx.closePath();
  }

  function render(bytes) {
    var ctx = canvas.getContext("2d");
    var nodes = decodeKrb(bytes);
    var w = canvas.width;
    var h = canvas.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = "#f3f1ea";
    ctx.fillRect(0, 0, w, h);
    nodes.forEach(function(n) {
      var x = coord(n.x, w, n.flags & 4);
      var y = coord(n.y, h, n.flags & 8);
      var nw = coord(n.w, w, n.flags & 16);
      var nh = coord(n.h, h, n.flags & 32);
      if (n.type === 1) {
        ctx.fillStyle = rgba(n.color);
        ctx.fillRect(0, 0, w, h);
      } else if (n.type === 2) {
        ctx.fillStyle = rgba(n.color);
        ctx.font = "700 " + n.font + "px ui-monospace, Menlo, Consolas, monospace";
        ctx.fillText(n.text || n.name, x, y + n.font);
      } else if (n.type === 3) {
        ctx.fillStyle = rgba(n.color);
        ctx.fillRect(x, y, nw || 1, nh || 1);
      } else if (n.type === 4) {
        nw = nw || 160;
        nh = nh || 40;
        drawRound(ctx, x, y, nw, nh, 6);
        ctx.fillStyle = rgba(n.color);
        ctx.fill();
        ctx.strokeStyle = "#062536";
        ctx.stroke();
        ctx.fillStyle = "#fff";
        ctx.font = "700 " + n.font + "px ui-monospace, Menlo, Consolas, monospace";
        ctx.fillText(n.text || n.name, x + 16, y + Math.round(nh / 2 + n.font / 3));
      } else if (n.type === 7 || n.type === 8 || n.type === 9) {
        ctx.strokeStyle = rgba(n.color);
        ctx.strokeRect(x, y, nw || 22, nh || 22);
        ctx.fillStyle = "#2a3b40";
        ctx.font = "700 " + n.font + "px ui-monospace, Menlo, Consolas, monospace";
        ctx.fillText(n.text || n.name, x + (nw || 28) + 8, y + n.font);
      }
    });
    setPreviewStatus(nodes.length + " nodes");
  }

  function showArtifact() {
    artifact.textContent = last[activeTab] || "";
  }

  function compile() {
    if (!k2irMod || !k2bMod) return;
    try {
      setStatus("compiling...");
      resetFs(k2irMod);
      k2irMod.FS.writeFile("/work/src/app.kry", source.value);
      k2irMod.callMain(["--root", "/work", "-o", "/work/out", "/work/src/app.kry"]);
      last.kir = readFirst(k2irMod, ["/work/out/app.kir", "/work/out/src/app.kir"], false);

      resetFs(k2bMod);
      k2bMod.FS.writeFile("/work/src/app.kry", source.value);
      k2bMod.callMain(["--no-main", "--root", "/work", "-o", "/work/out", "/work/src/app.kry"]);
      last.bytes = readFirst(k2bMod, ["/work/out/app.krb", "/work/out/src/app.krb"], true);
      last.krb = "KRB bytes: " + last.bytes.length + "\n\n" + hex(last.bytes);
      last.c = readFirst(k2bMod, ["/work/out/app.krb.c", "/work/out/src/app.krb.c"], false);
      showArtifact();
      render(last.bytes);
      setStatus("compiled");
    } catch (e) {
      setStatus("error");
      setPreviewStatus("error");
      artifact.textContent = String(e && e.stack ? e.stack : e);
    }
  }

  function scheduleCompile() {
    window.clearTimeout(compileTimer);
    compileTimer = window.setTimeout(compile, 260);
  }

  function loadInitialSource() {
    var params = new URLSearchParams(window.location.search);
    var src = params.get("src");
    if (!src) {
      source.value = sample;
      return Promise.resolve();
    }
    setStatus("fetching source...");
    return fetch(src).then(function(res) {
      if (!res.ok) throw new Error("Could not fetch " + src + ": " + res.status);
      return res.text();
    }).then(function(text) {
      source.value = text;
    }).catch(function(err) {
      source.value = sample;
      artifact.textContent = String(err);
    });
  }

  function boot() {
    if (typeof createK2irModule !== "function" || typeof createK2bModule !== "function") {
      setStatus("compiler unavailable");
      artifact.textContent = "The web compiler assets were not built.";
      return;
    }
    Promise.all([
      createK2irModule({ noInitialRun: true }),
      createK2bModule({ noInitialRun: true })
    ]).then(function(mods) {
      k2irMod = mods[0];
      k2bMod = mods[1];
      return loadInitialSource();
    }).then(function() {
      setStatus("ready");
      compile();
    }).catch(function(err) {
      setStatus("error");
      artifact.textContent = String(err && err.stack ? err.stack : err);
    });
  }

  tabButtons.forEach(function(btn) {
    btn.addEventListener("click", function() {
      activeTab = btn.getAttribute("data-tab");
      tabButtons.forEach(function(b) { b.classList.toggle("is-active", b === btn); });
      showArtifact();
    });
  });
  source.addEventListener("input", scheduleCompile);
  compileButton.addEventListener("click", compile);
  sampleButton.addEventListener("click", function() {
    source.value = sample;
    compile();
  });
  boot();
})();

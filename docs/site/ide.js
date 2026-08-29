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
  var k2cMod = null;
  var k2gMod = null;
  var k2jsMod = null;
  var krbPlayerMod = null;
  var krbPlayerInputReady = false;
  var activeTab = "kry";
  var last = { kry: "", kir: "", krb: "", c: "", go: "", js: "", bytes: null };
  var compileTimer = 0;

  var sample = [
    "#import \"kryon.h\"",
    "",
    "app \"Kryon Web IDE\" {",
    "    size 800 520",
    "}",
    "",
    "App :: () #ui {",
    "    Screen root: {",
    "        Background((Color){249, 246, 235, 255})",
    "        Rect(48, 42, 704, 390, (Color){255, 254, 249, 255})",
    "        Text(\"Hello from .kry\", 76, 86, Text24, (Color){31, 83, 102, 255})",
    "        Text(\"This page runs k2ir and k2b as WebAssembly, then renders the KRB cartridge.\", 76, 134, Text16, (Color){42, 59, 64, 255})",
    "        Rect(76, 188, 292, 80, (Color){35, 101, 125, 255})",
    "        Text(\"Portable preview\", 104, 219, Text20, WHITE)",
    "        Button((ButtonProps){.bounds = {76, 308, 184, 44}, .label = \"Get started\"})",
    "    }",
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

  function runTool(mod, args, reader) {
    var rc;

    if (!mod) return { ok: false, text: "Compiler module unavailable." };
    try {
      resetFs(mod);
      mod.FS.writeFile("/work/src/app.kry", source.value);
      rc = mod.callMain(args);
      if (rc && rc !== 0) throw new Error("compiler exited with " + rc);
      return reader(mod);
    } catch (e) {
      return { ok: false, text: String(e && e.stack ? e.stack : e) };
    }
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

  function attachKrbPlayerInput() {
    if (krbPlayerInputReady)
      return;
    krbPlayerInputReady = true;
    function pos(e) {
      var r = canvas.getBoundingClientRect();
      return [
        Math.round((e.clientX - r.left) * canvas.width / r.width),
        Math.round((e.clientY - r.top) * canvas.height / r.height)
      ];
    }
    window.addEventListener("keydown", function(e) {
      if (e.key === "Backspace" && krbPlayerMod && krbPlayerMod._krb_web_text) {
        e.preventDefault();
        krbPlayerMod._krb_web_text(8);
      }
    });
    window.addEventListener("keypress", function(e) {
      if (krbPlayerMod && krbPlayerMod._krb_web_text && e.charCode > 31)
        krbPlayerMod._krb_web_text(e.charCode);
    });
    canvas.addEventListener("wheel", function(e) {
      e.preventDefault();
      if (krbPlayerMod && krbPlayerMod._krb_web_wheel)
        krbPlayerMod._krb_web_wheel(Math.round(-e.deltaY / 2));
    }, { passive: false });
    canvas.addEventListener("mousemove", function(e) {
      var p;
      if (krbPlayerMod && krbPlayerMod._krb_web_mouse) {
        p = pos(e);
        krbPlayerMod._krb_web_mouse(p[0], p[1]);
      }
    });
    canvas.addEventListener("mousedown", function(e) {
      var p;
      if (krbPlayerMod && krbPlayerMod._krb_web_button) {
        p = pos(e);
        krbPlayerMod._krb_web_mouse(p[0], p[1]);
        krbPlayerMod._krb_web_button(0, 1);
      }
    });
    canvas.addEventListener("mouseup", function(e) {
      var p;
      if (krbPlayerMod && krbPlayerMod._krb_web_button) {
        p = pos(e);
        krbPlayerMod._krb_web_mouse(p[0], p[1]);
        krbPlayerMod._krb_web_button(0, 0);
      }
    });
  }

  function render(bytes) {
    var rc;

    if (!krbPlayerMod)
      throw new Error("KRB web player unavailable");
    canvas.width = 800;
    canvas.height = 600;
    try { krbPlayerMod.FS.unlink("/app.krb"); } catch (e) {}
    krbPlayerMod.FS.writeFile("/app.krb", bytes);
    rc = krbPlayerMod._krb_web_start();
    if (rc && rc !== 0)
      throw new Error("krb-web exited with " + rc);
    attachKrbPlayerInput();
    setPreviewStatus("wasm canvas");
  }

  function showArtifact() {
    artifact.textContent = last[activeTab] || "";
  }

  function compile() {
    var passed = 0;
    var failed = 0;
    var result;

    if (!k2irMod || !k2bMod) return;
    setStatus("compiling...");
    last = { kry: source.value, kir: "", krb: "", c: "", go: "", js: "", bytes: null };

    result = runTool(k2irMod, ["--root", "/work", "-o", "/work/out", "/work/src/app.kry"], function(mod) {
      return { ok: true, text: readFirst(mod, ["/work/out/app.kir", "/work/out/src/app.kir"], false) };
    });
    if (result.ok && result.text) { last.kir = result.text; passed++; } else { last.kir = result.text || "KIR output unavailable."; failed++; }

    result = runTool(k2cMod, ["--no-main", "--root", "/work", "-o", "/work/out", "/work/src/app.kry"], function(mod) {
      var files = [];
      var appC = readFirst(mod, ["/work/out/src/app.c", "/work/out/app.c"], false);
      var appH = readFirst(mod, ["/work/out/src/app.h", "/work/out/app.h"], false);
      var projectC = readMaybe(mod, "/work/out/kryon_project.c", false);
      if (appC) files.push("/* app.c */\n" + appC);
      if (appH) files.push("/* app.h */\n" + appH);
      if (projectC) files.push("/* kryon_project.c */\n" + projectC);
      return { ok: files.length > 0, text: files.join("\n\n") || "C output unavailable." };
    });
    if (result.ok) { last.c = result.text; passed++; } else { last.c = result.text; failed++; }

    result = runTool(k2gMod, ["--no-main", "--pkg", "kryexample", "--root", "/work", "-o", "/work/out", "/work/src/app.kry"], function(mod) {
      return { ok: true, text: readFirst(mod, ["/work/out/app.go", "/work/out/src/app.go"], false) };
    });
    if (result.ok && result.text) { last.go = result.text; passed++; } else { last.go = result.text || "Go output unavailable."; failed++; }

    result = runTool(k2jsMod, ["--no-main", "--root", "/work", "-o", "/work/out", "/work/src/app.kry"], function(mod) {
      return { ok: true, text: readFirst(mod, ["/work/out/app.js", "/work/out/src/app.js"], false) };
    });
    if (result.ok && result.text) { last.js = result.text; passed++; } else { last.js = result.text || "JS output unavailable."; failed++; }

    result = runTool(k2bMod, ["--no-main", "--root", "/work", "-o", "/work/out", "/work/src/app.kry"], function(mod) {
      var bytes = readFirst(mod, ["/work/out/app.krb", "/work/out/src/app.krb"], true);
      return { ok: bytes.length > 0, bytes: bytes, text: "KRB bytes: " + bytes.length + "\n\n" + hex(bytes) };
    });
    if (result.ok) {
      last.bytes = result.bytes;
      last.krb = result.text;
      passed++;
      try {
        render(last.bytes);
      } catch (e) {
        setPreviewStatus(String(e && e.message ? e.message : e).slice(0, 80));
        failed++;
      }
    } else {
      last.krb = result.text || "KRB output unavailable.";
      setPreviewStatus("no KRB preview");
      failed++;
    }
    showArtifact();
    setStatus(failed ? "compiled " + passed + "/5" : "compiled");
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
    if (typeof createK2irModule !== "function" || typeof createK2bModule !== "function" ||
        typeof createK2cModule !== "function" || typeof createK2gModule !== "function" ||
        typeof createK2jsModule !== "function" ||
        typeof createKrbWebModule !== "function") {
      setStatus("compiler unavailable");
      artifact.textContent = "The web compiler assets were not built.";
      return;
    }
    Promise.all([
      createK2irModule({ noInitialRun: true }),
      createK2bModule({ noInitialRun: true }),
      createK2cModule({ noInitialRun: true }),
      createK2gModule({ noInitialRun: true }),
      createK2jsModule({ noInitialRun: true }),
      createKrbWebModule({ noInitialRun: true, canvas: canvas })
    ]).then(function(mods) {
      k2irMod = mods[0];
      k2bMod = mods[1];
      k2cMod = mods[2];
      k2gMod = mods[3];
      k2jsMod = mods[4];
      krbPlayerMod = mods[5];
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

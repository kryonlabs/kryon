(function() {
  "use strict";

  var source = document.querySelector("[data-source]");
  var sourceTitle = document.querySelector("[data-source-title]");
  var status = document.querySelector("[data-status]");
  var previewStatus = document.querySelector("[data-preview-status]");
  var canvas = document.querySelector("[data-preview]");
  var artifact = document.querySelector("[data-artifact]");
  var list = document.querySelector("[data-example-list]");
  var count = document.querySelector("[data-count]");
  var tabButtons = Array.prototype.slice.call(document.querySelectorAll("[data-tab]"));
  var compileButton = document.querySelector("[data-action='compile']");
  var reloadButton = document.querySelector("[data-action='reload']");
  var menuToggle = document.querySelector(".menu-toggle");
  var headerNav = document.getElementById("header-nav");
  var activeTab = new URLSearchParams(window.location.search).get("tab") || "kry";
  var modules = {};
  var playerMod = null;
  var playerInputReady = false;
  var items = [];
  var current = null;
  var compileTimer = 0;
  var last = { kry: "", kir: "", c: "", go: "", krb: "", bytes: null };

  var fallback = {
    group: "Examples",
    title: "Buttons",
    name: "02_buttons.kry",
    path: "examples/02_buttons.kry",
    url: "examples-src/examples/02_buttons.kry"
  };

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

  function rmTree(mod, path) {
    var entries;
    try {
      entries = mod.FS.readdir(path);
    } catch (e) {
      return;
    }
    entries.forEach(function(name) {
      var child;
      if (name === "." || name === "..") return;
      child = path + "/" + name;
      try {
        if (mod.FS.isDir(mod.FS.stat(child).mode)) {
          rmTree(mod, child);
          mod.FS.rmdir(child);
        } else {
          mod.FS.unlink(child);
        }
      } catch (e) {}
    });
  }

  function resetFs(mod) {
    rmTree(mod, "/work");
    try { mod.FS.rmdir("/work"); } catch (e) {}
    ensureDir(mod, "/work");
    ensureDir(mod, "/work/out");
  }

  function writeSource(mod, path, text) {
    var full = "/work/" + path;
    var slash = full.lastIndexOf("/");

    if (slash > 0) ensureDir(mod, full.slice(0, slash));
    mod.FS.writeFile(full, text);
    return full;
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

  function stem(path) {
    var name = path.split("/").pop();
    return name.replace(/\.[^.]+$/, "");
  }

  function dir(path) {
    var slash = path.lastIndexOf("/");
    return slash > 0 ? path.slice(0, slash) : "";
  }

  function outPath(path, ext) {
    var d = dir(path);
    return "/work/out/" + (d ? d + "/" : "") + stem(path) + ext;
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

  function readU16(dv, off) { return dv.getUint16(off, true); }
  function readI16(dv, off) { return dv.getInt16(off, true); }
  function readU32(dv, off) { return dv.getUint32(off, true); }

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
    var dv;
    var nodeCount;
    var stringBytes;
    var nodeOff = 32;
    var nodes = [];

    if (!bytes || bytes.length < 32) throw new Error("KRB is empty");
    dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    if (readU32(dv, 0) !== 0x0042524b) throw new Error("KRB magic mismatch");
    nodeCount = readU32(dv, 8);
    stringBytes = readU32(dv, 12);
    for (var i = 0; i < nodeCount; i++) {
      var off = nodeOff + i * 28;
      var strOff = nodeOff + nodeCount * 28;
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

  function sizeFromSource(text) {
    var m = text.match(/\bsize\s+(\d+)\s+(\d+)/);
    if (!m) return { w: 800, h: 600 };
    return {
      w: Math.max(240, Math.min(1600, parseInt(m[1], 10))),
      h: Math.max(180, Math.min(1200, parseInt(m[2], 10)))
    };
  }

  function attachPlayerInput() {
    if (playerInputReady)
      return;
    playerInputReady = true;
    function pos(e) {
      var r = canvas.getBoundingClientRect();
      return [
        Math.round((e.clientX - r.left) * canvas.width / r.width),
        Math.round((e.clientY - r.top) * canvas.height / r.height)
      ];
    }
    window.addEventListener("keydown", function(e) {
      if (e.key === "Backspace" && playerMod && playerMod._krb_web_text) {
        e.preventDefault();
        playerMod._krb_web_text(8);
      }
    });
    window.addEventListener("keypress", function(e) {
      if (playerMod && playerMod._krb_web_text && e.charCode > 31)
        playerMod._krb_web_text(e.charCode);
    });
    canvas.addEventListener("wheel", function(e) {
      e.preventDefault();
      if (playerMod && playerMod._krb_web_wheel)
        playerMod._krb_web_wheel(Math.round(-e.deltaY / 2));
    }, { passive: false });
    canvas.addEventListener("mousemove", function(e) {
      var p;
      if (playerMod && playerMod._krb_web_mouse) {
        p = pos(e);
        playerMod._krb_web_mouse(p[0], p[1]);
      }
    });
    canvas.addEventListener("mousedown", function(e) {
      var p;
      if (playerMod && playerMod._krb_web_button) {
        p = pos(e);
        playerMod._krb_web_mouse(p[0], p[1]);
        playerMod._krb_web_button(0, 1);
      }
    });
    canvas.addEventListener("mouseup", function(e) {
      var p;
      if (playerMod && playerMod._krb_web_button) {
        p = pos(e);
        playerMod._krb_web_mouse(p[0], p[1]);
        playerMod._krb_web_button(0, 0);
      }
    });
  }

  function render(bytes) {
    var rc;

    if (!playerMod)
      throw new Error("KRB web player unavailable");
    canvas.width = 800;
    canvas.height = 600;
    try { playerMod.FS.unlink("/app.krb"); } catch (e) {}
    playerMod.FS.writeFile("/app.krb", bytes);
    rc = playerMod._krb_web_start();
    if (rc && rc !== 0)
      throw new Error("krb-web exited with " + rc);
    attachPlayerInput();
    setPreviewStatus("wasm canvas");
  }

  function runCompiler(key, args, readers) {
    var mod = modules[key];
    var full;
    var rc;

    if (!mod) return { ok: false, text: "Compiler module unavailable." };
    resetFs(mod);
    full = writeSource(mod, current.path, source.value);
    args = args.concat([full]);
    try {
      rc = mod.callMain(args);
      if (rc && rc !== 0) throw new Error(key + " exited with " + rc);
      return readers(mod);
    } catch (e) {
      return { ok: false, text: String(e && e.stack ? e.stack : e) };
    }
  }

  function showArtifact() {
    artifact.textContent = last[activeTab] || "";
  }

  function compile() {
    var passed = 0;
    var failed = 0;
    var result;

    if (!current) return;
    window.clearTimeout(compileTimer);
    last = { kry: source.value, kir: "", c: "", go: "", krb: "", bytes: null };
    setStatus("compiling...");
    setPreviewStatus("rendering");

    result = runCompiler("kir", ["--root", "/work", "-o", "/work/out"], function(mod) {
      return { ok: true, text: readFirst(mod, [outPath(current.path, ".kir"), "/work/out/" + stem(current.path) + ".kir"], false) };
    });
    if (result.ok && result.text) { last.kir = result.text; passed++; } else { last.kir = result.text || "KIR output unavailable."; failed++; }

    result = runCompiler("c", ["--no-main", "--root", "/work", "-o", "/work/out"], function(mod) {
      var files = [];
      var moduleC = readFirst(mod, [outPath(current.path, ".c"), "/work/out/" + stem(current.path) + ".c"], false);
      var header = readFirst(mod, [outPath(current.path, ".h"), "/work/out/" + stem(current.path) + ".h"], false);
      var projectC = readMaybe(mod, "/work/out/kryon_project.c", false);
      if (moduleC) files.push("/* " + current.path.replace(/\.kry$/, ".c") + " */\n" + moduleC);
      if (header) files.push("/* " + current.path.replace(/\.kry$/, ".h") + " */\n" + header);
      if (projectC) files.push("/* kryon_project.c */\n" + projectC);
      return { ok: files.length > 0, text: files.join("\n\n") || "C output unavailable." };
    });
    if (result.ok) { last.c = result.text; passed++; } else { last.c = result.text; failed++; }

    result = runCompiler("go", ["--no-main", "--pkg", "kryexample", "--root", "/work", "-o", "/work/out"], function(mod) {
      return { ok: true, text: readFirst(mod, ["/work/out/" + stem(current.path) + ".go", outPath(current.path, ".go")], false) };
    });
    if (result.ok && result.text) { last.go = result.text; passed++; } else { last.go = result.text || "Go output unavailable."; failed++; }

    result = runCompiler("krb", ["--no-main", "--root", "/work", "-o", "/work/out"], function(mod) {
      var bytes = readFirst(mod, [outPath(current.path, ".krb"), "/work/out/" + stem(current.path) + ".krb"], true);
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
    setStatus(failed ? "compiled " + passed + "/4" : "compiled");
  }

  function scheduleCompile() {
    window.clearTimeout(compileTimer);
    compileTimer = window.setTimeout(compile, 260);
  }

  function selectItem(item) {
    current = item;
    if (sourceTitle) sourceTitle.textContent = item.path;
    Array.prototype.slice.call(list.querySelectorAll("button")).forEach(function(btn) {
      btn.classList.toggle("is-active", btn.getAttribute("data-path") === item.path);
    });
    setStatus("fetching source...");
    return fetch(item.url).then(function(res) {
      if (!res.ok) throw new Error("Could not fetch " + item.url + ": " + res.status);
      return res.text();
    }).then(function(text) {
      source.value = text;
      compile();
    }).catch(function(err) {
      source.value = "#import \"kryon.h\"\n\n/* " + String(err) + " */\n";
      last.kry = source.value;
      showArtifact();
      setStatus("source unavailable");
    });
  }

  function renderList() {
    var currentGroup = "";
    list.innerHTML = "";
    items.forEach(function(item) {
      var heading;
      var btn;
      if (item.group !== currentGroup) {
        currentGroup = item.group;
        heading = document.createElement("div");
        heading.className = "example-group";
        heading.textContent = currentGroup;
        list.appendChild(heading);
      }
      btn = document.createElement("button");
      btn.type = "button";
      btn.textContent = item.title;
      btn.setAttribute("data-path", item.path);
      btn.addEventListener("click", function() { selectItem(item); });
      list.appendChild(btn);
    });
    if (count) count.textContent = String(items.length);
  }

  function loadManifest() {
    return fetch("examples-manifest.json").then(function(res) {
      if (!res.ok) throw new Error("No manifest");
      return res.json();
    }).then(function(data) {
      items = data.items || [];
      if (!items.length) items = [fallback];
    }).catch(function() {
      items = [fallback];
    });
  }

  function boot() {
    if (typeof createK2irModule !== "function" || typeof createK2bModule !== "function" ||
        typeof createK2cModule !== "function" || typeof createK2gModule !== "function" ||
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
      createKrbWebModule({ noInitialRun: true, canvas: canvas }),
      loadManifest()
    ]).then(function(values) {
      var query = new URLSearchParams(window.location.search);
      var wanted = query.get("example");
      modules.kir = values[0];
      modules.krb = values[1];
      modules.c = values[2];
      modules.go = values[3];
      playerMod = values[4];
      renderList();
      return selectItem(items.filter(function(item) {
        return item.path === wanted || item.name === wanted;
      })[0] || items[0]);
    }).catch(function(err) {
      setStatus("error");
      artifact.textContent = String(err && err.stack ? err.stack : err);
    });
  }

  tabButtons.forEach(function(btn) {
    btn.classList.toggle("is-active", btn.getAttribute("data-tab") === activeTab);
    btn.addEventListener("click", function() {
      activeTab = btn.getAttribute("data-tab");
      tabButtons.forEach(function(b) { b.classList.toggle("is-active", b === btn); });
      showArtifact();
    });
  });
  source.addEventListener("input", scheduleCompile);
  compileButton.addEventListener("click", compile);
  reloadButton.addEventListener("click", function() {
    if (current) selectItem(current);
  });
  boot();
})();

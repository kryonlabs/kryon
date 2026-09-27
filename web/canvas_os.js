// Clipboard, console and MEMFS metadata effects; file and path policy stays in Zi.
addToLibrary({
  js_dropped_count: () => {
    var K = globalThis.__kryonCanvas;
    if (K && K.droppedPending > 0) return 0;
    return K && K.dropped ? K.dropped.length : 0;
  },

  js_dropped_path__deps: ['malloc', '$lengthBytesUTF8', '$stringToUTF8'],
  js_dropped_path: (index) => {
    var K = globalThis.__kryonCanvas;
    var path = K && K.dropped && index >= 0 && index < K.dropped.length ?
        K.dropped[index] : "";
    var len = lengthBytesUTF8(path) + 1;
    var ptr = _malloc(len);
    stringToUTF8(path, ptr, len);
    return ptr;
  },

  js_clipboard_pull__deps: ['$stringToUTF8'],
  js_clipboard_pull: (dst, cap) => {
    var K = globalThis.__kryonCanvas;
    if (!K || !dst || cap <= 0) return;
    stringToUTF8(K.clipboard || "", dst, cap);
  },

  js_clipboard_push__deps: ['$UTF8ToString'],
  js_clipboard_push: (text) => {
    var value = text ? UTF8ToString(text) : "";
    var K = globalThis.__kryonCanvas;
    if (K) K.clipboard = value;
    var tryExecCommandCopy = function () {
        if (typeof document === 'undefined' || !document.execCommand)
            return false;
        var input = document.createElement('textarea');
        input.value = value;
        input.setAttribute('readonly', 'readonly');
        input.style.position = 'fixed';
        input.style.left = '-10000px';
        input.style.top = '-10000px';
        document.body.appendChild(input);
        input.focus();
        input.select();
        var ok = false;
        try { ok = !!document.execCommand('copy'); } catch (_) {}
        input.remove();
        if (K && K.canvas && K.canvas.focus) {
            try { K.canvas.focus({preventScroll: true}); } catch (_) {
                try { K.canvas.focus(); } catch (_) {}
            }
        }
        return ok;
    };
    var nowMs = (typeof performance !== 'undefined' && performance.now) ?
        performance.now() : Date.now();
    var gestureActive = K && nowMs <= (K.clipboardGestureUntil || 0);
    if (gestureActive)
        tryExecCommandCopy();
    if (globalThis.navigator && globalThis.navigator.clipboard &&
        globalThis.navigator.clipboard.writeText)
        globalThis.navigator.clipboard.writeText(value).catch(function () {
            if (!gestureActive)
                tryExecCommandCopy();
        });
  },

  $CanvasDirectories: { next: 1, entries: null },
  js_file_kind__deps: ['$FS', '$UTF8ToString'],
  js_file_kind: path => {
    if (!path) return 0;
    try { return FS.isDir(FS.stat(UTF8ToString(path)).mode) ? 2 : 1; }
    catch (_) { return 0; }
  },
  js_file_length__deps: ['$FS', '$UTF8ToString'],
  js_file_length: path => {
    if (!path) return 0;
    try {
      const stat = FS.stat(UTF8ToString(path));
      return !FS.isDir(stat.mode) && stat.size <= 2147483647 ? stat.size : 0;
    } catch (_) { return 0; }
  },
  js_file_modtime__deps: ['$FS', '$UTF8ToString'],
  js_file_modtime: path => {
    if (!path) return 0;
    try { return Math.floor(+FS.stat(UTF8ToString(path)).mtime / 1000); }
    catch (_) { return 0; }
  },
  js_directory_open__deps: ['$FS', '$UTF8ToString', '$CanvasDirectories'],
  js_directory_open: path => {
    try {
      const table = CanvasDirectories;
      if (!table.entries) table.entries = new Map();
      if (table.entries.size >= 8) return 0;
      const names = FS.readdir(path ? UTF8ToString(path) : '.');
      if (names.length > 65536) return 0;
      if (table.next >= 2147483647) table.next = 1;
      while (table.entries.has(table.next)) table.next++;
      const handle = table.next++;
      table.entries.set(handle, names);
      return handle;
    } catch (_) { return 0; }
  },
  js_directory_count__deps: ['$CanvasDirectories'],
  js_directory_count: handle => CanvasDirectories.entries?.get(handle)?.length || 0,
  js_directory_entry__deps: ['$CanvasDirectories', '$lengthBytesUTF8', '$stringToUTF8'],
  js_directory_entry: (handle, index, output, capacity) => {
    const name = CanvasDirectories.entries?.get(handle)?.[index];
    if (typeof name !== 'string' || !output || capacity < 1) return -1;
    const length = lengthBytesUTF8(name);
    if (length >= capacity) return -1;
    stringToUTF8(name, output, capacity);
    return length;
  },
  js_directory_close__deps: ['$CanvasDirectories'],
  js_directory_close: handle => { CanvasDirectories.entries?.delete(handle); },
  TextFormat__deps: ['canvas_format_text'],
  TextFormat__sig: 'iii',
  TextFormat: (format, args) => _canvas_format_text(format, args),
  TraceLog__deps: ['canvas_format_log', 'canvas_trace_callback', '$UTF8ToString'],
  TraceLog__sig: 'viii',
  TraceLog: (level, format, args) => {
    if (_canvas_trace_callback(level, format, args)) return;
    const message = UTF8ToString(_canvas_format_log(format, args));
    if (Module.printErr) Module.printErr(message);
    else console.error(message);
  }

});

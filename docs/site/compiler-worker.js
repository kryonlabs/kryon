// Compiler ABI transport only. Revision policy and diagnostics UI belong to
// the compiled editor. The shared compiler/FS handles one request at a time.
let runtime;
let pending = Promise.resolve();
let diagnostics = '';
self.onmessage = function ({data}) {
  pending = pending.then(() => compile(data), () => compile(data));
};
async function compile(data) {
  diagnostics = '';
  let sourcePath;
  let originalSource;
  let wroteSource = false;
  let compiler;
  try {
    if (!runtime) {
      importScripts('runtime/playground-runtime.js' + self.location.search);
      runtime = createPlayground({
        locateFile: name => new URL('runtime/' + name + self.location.search, self.location.href).href,
        print: function () {},
        printErr: line => { if (diagnostics.length < 32768) diagnostics += String(line) + '\n'; }
      });
    }
    compiler = await runtime;
    const authored = data.sourcePath;
    if (typeof authored !== 'string' || !authored.endsWith('.tsx') ||
        authored.includes('\\') || authored.split('/').some(part => !part || part === '.' || part === '..')) {
      throw new Error('Choose an authored .tsx source file to compile.');
    }
    sourcePath = '/' + authored;
    compiler.FS.mkdirTree(sourcePath.slice(0, sourcePath.lastIndexOf('/')) || '/');
    if (compiler.FS.analyzePath(sourcePath).exists) {
      originalSource = compiler.FS.readFile(sourcePath);
    }
    wroteSource = true;
    compiler.FS.writeFile(sourcePath, data.source);
    const binding = 'preview_entry:PreviewFrame=' + authored.slice(0, -4) + ':Frame';
    const status = compiler.ccall('BuildBundle', 'number', Array(7).fill('string'),
      ['/', data.search, '/preview_entry.zi\n' + sourcePath, 'preview_entry', 'Tick', binding, '/preview.zx']);
    if (status) { self.postMessage({id:data.id, status, diagnostics}); return; }
    const bytes = compiler.FS.readFile('/preview.zx');
    self.postMessage({id:data.id, status:0, diagnostics:'', bytes:bytes.buffer}, [bytes.buffer]);
  } catch (error) {
    self.postMessage({id:data.id, status:1, diagnostics:diagnostics || String(error)});
    runtime = undefined;
  } finally {
    if (compiler) {
      if (wroteSource) {
        try {
          if (originalSource !== undefined) compiler.FS.writeFile(sourcePath, originalSource);
          else if (compiler.FS.analyzePath(sourcePath).exists) compiler.FS.unlink(sourcePath);
        } catch (error) {
          runtime = undefined;
        }
      }
      try {
        if (compiler.FS.analyzePath('/preview.zx').exists) compiler.FS.unlink('/preview.zx');
      } catch (error) {
        runtime = undefined;
      }
    }
  }
}

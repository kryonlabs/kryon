// Compiler ABI transport only. Scheduling, revision policy and diagnostics UI
// belong to the compiled Ziran editor.
let runtime;
let diagnostics = '';
self.onmessage = async function ({data}) {
  diagnostics = '';
  try {
    if (!runtime) {
      importScripts('runtime/playground-runtime.js' + self.location.search);
      runtime = await createPlayground({
        locateFile: name => new URL('runtime/' + name + self.location.search, self.location.href).href,
        print: function () {},
        printErr: line => { if (diagnostics.length < 32768) diagnostics += String(line) + '\n'; }
      });
    }
    runtime.FS.writeFile('/app.zi', data.source);
    const status = runtime.ccall('BuildBundle', 'number', Array(6).fill('string'),
      ['/', data.search, '/preview_entry.zi', 'preview_entry', 'Tick', '/preview.zx']);
    if (status) { self.postMessage({id:data.id, status, diagnostics}); return; }
    const bytes = runtime.FS.readFile('/preview.zx');
    self.postMessage({id:data.id, status:0, diagnostics:'', bytes:bytes.buffer}, [bytes.buffer]);
  } catch (error) {
    self.postMessage({id:data.id, status:1, diagnostics:diagnostics || String(error)});
  }
};

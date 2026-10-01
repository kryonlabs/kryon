/* The real Ziran compiler runs off the editor's event loop. Its filesystem
 * contains ordinary Kryon source modules and a small Ziran frame entry. */
let runtime;
let diagnostics = [];
let diagnosticSize = 0;

self.onmessage = async function ({data}) {
  const {source, revision} = data;
  diagnostics = [];
  diagnosticSize = 0;
  try {
    if (typeof source !== 'string' || new TextEncoder().encode(source).length > 65536) throw new Error('Keep the source under 64 KB.');
    if (!runtime) {
      self.postMessage({type: 'stage', stage: 'Loading compiler…', revision});
      importScripts('assets/playground-runtime.js');
      runtime = await createPlayground({
        locateFile: name => new URL('assets/' + name, self.location.href).href,
        print: () => {},
        printErr: line => {
          diagnosticSize += line.length;
          if (diagnosticSize <= 32768) diagnostics.push(line);
        }
      });
    }
    self.postMessage({type: 'stage', stage: 'Compiling…', revision});
    runtime.FS.writeFile('/app.zi', source);
    const status = runtime.ccall('BuildBundle', 'number', Array(6).fill('string'),
      ['/', '/std\n/kryon/ui\nkryon=/kryon/ui', '/preview_entry.zi', 'preview_entry', 'Tick', '/preview.zib']);
    if (status !== 0) throw new Error(diagnostics.join('\n') || 'Unable to compile this source.');
    const bytes = runtime.FS.readFile('/preview.zib');
    self.postMessage({type: 'result', ok: true, revision, bundle: bytes.buffer}, [bytes.buffer]);
  } catch (error) {
    self.postMessage({type: 'result', ok: false, revision, diagnostics: diagnostics.join('\n') || error.message || String(error)});
    runtime = undefined;
  }
};

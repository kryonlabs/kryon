/* The real Ziran compiler runs off the editor's event loop. Its filesystem
 * contains ordinary Kryon source modules and a small Ziran frame entry. */
let runtime;
let diagnostics = [];
let diagnosticSize = 0;
const bundles = new Map();
let bundleBytes = 0;
const cacheLimit = 32 * 1024 * 1024;

function remember(key, bytes) {
  if (bytes.byteLength > cacheLimit) return;
  while (bundles.size && (bundleBytes + bytes.byteLength > cacheLimit || bundles.size >= 12)) {
    const oldest = bundles.keys().next().value;
    bundleBytes -= bundles.get(oldest).byteLength;
    bundles.delete(oldest);
  }
  const saved = bytes.slice();
  bundles.set(key, saved);
  bundleBytes += saved.byteLength;
}

self.onmessage = async function ({data}) {
  const {source, revision} = data;
  const search = data.search || "/std\n/kryon/ui\nkryon=/kryon/ui";
  diagnostics = [];
  diagnosticSize = 0;
  try {
    if (typeof source !== 'string' || new TextEncoder().encode(source).length > 65536) throw new Error('Keep the source under 64 KB.');
    const key = search + '\0' + source;
    if (bundles.has(key)) {
      const saved = bundles.get(key);
      bundles.delete(key); bundles.set(key, saved);
      const bytes = saved.slice();
      self.postMessage({type: 'result', ok: true, revision, bundle: bytes.buffer}, [bytes.buffer]);
      return;
    }
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
      ['/', search, '/preview_entry.zi', 'preview_entry', 'Tick', '/preview.zib']);
    if (status !== 0) throw new Error(diagnostics.join('\n') || 'Unable to compile this source.');
    const bytes = runtime.FS.readFile('/preview.zib');
    remember(key, bytes);
    self.postMessage({type: 'result', ok: true, revision, bundle: bytes.buffer}, [bytes.buffer]);
  } catch (error) {
    self.postMessage({type: 'result', ok: false, revision, diagnostics: diagnostics.join('\n') || error.message || String(error)});
    runtime = undefined;
  }
};

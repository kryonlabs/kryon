// Runtime packaging only. All interface state, layout and interactions are Ziran.
var Module = {
  onExit: function (code) { if (code) document.getElementById('result').textContent = 'Application stopped (' + code + ').'; },
  onAbort: function () { document.getElementById('result').textContent = 'Could not start the interactive examples.'; }
};
(async function () {
  var probe = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,123,3,2,1,0,10,10,1,8,0,65,0,253,15,253,98,11]);
  var name = WebAssembly.validate(probe) ? 'simd128' : 'baseline';
  Module.kryonBuild = name;
  var manifest = await (await fetch('runtime/assets.json', {cache:'no-store'})).json();
  var files = await Promise.all(manifest.map(async function (entry) {
    var response = await fetch(entry[1], {cache:'no-store'});
    if (!response.ok) throw new Error('Missing runtime asset');
    return [entry[0], new Uint8Array(await response.arrayBuffer())];
  }));
  Module.preRun = [function () {
    files.forEach(function (file) {
      var directory = file[0].replace(/\/[^/]+$/, '');
      if (directory && directory !== '/') Module.FS.createPath('/', directory.replace(/^\//, ''), true, true);
      Module.FS.writeFile(file[0], file[1]);
    });
  }];
  var response = await fetch('runtime/' + name + '.js', {cache:'no-store'});
  if (!response.ok) throw new Error('Missing compiled application');
  var script = document.createElement('script');
  script.id = 'mainScript'; script.text = await response.text();
  document.body.appendChild(script);
})().catch(function () { Module.onAbort(); });

// Asynchronous compiler/FS transport. The Ziran editor owns all compile state.
let compilerWorker;
let compilerRequestId;
Module.compileSource = function (request) {
  compilerRequestId = request.id;
  if (!compilerWorker) {
    compilerWorker = new Worker('compiler-worker.js?v=' + document.querySelector('script[src*="current.js"]').src.split('?v=')[1]);
    compilerWorker.onmessage = function ({data}) {
      if (data.id !== compilerRequestId) return;
      if (data.bytes) Module.FS.writeFile('/compiled.zx', new Uint8Array(data.bytes));
      delete data.bytes;
      Module.compilerResult = data;
    };
    compilerWorker.onerror = function (event) {
      Module.compilerResult = {id:compilerRequestId, status:1, diagnostics:event.message};
    };
  }
  compilerWorker.postMessage(request);
};

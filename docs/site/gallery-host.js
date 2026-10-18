// Only host observations cross this boundary; widget behavior stays in Ziran.
(() => {
  Module.galleryScene = 0;
  Module.galleryDark = 0;
  Module.galleryDisabled = 0;
  Module.previewPaused = 0;
  addEventListener('message', event => {
    if (event.source !== parent || event.origin !== location.origin) return;
    if (event.data?.type === 'preview:pause') { Module.previewPaused = event.data.paused ? 1 : 0; return; }
    if (event.data?.type !== 'gallery:select') return;
    const {scene, dark, disabled} = event.data;
    if (!Number.isInteger(scene) || scene < 0 || scene >= Module.galleryCount) return;
    Module.galleryScene = scene;
    Module.galleryDark = dark ? 1 : 0;
    Module.galleryDisabled = disabled ? 1 : 0;
  });
  const started = performance.now();
  function ready() {
    if (Module.galleryReady === 1) parent.postMessage({type: 'gallery:ready'}, location.origin);
    else if (performance.now() - started > 30000) parent.postMessage({type: 'gallery:error'}, location.origin);
    else requestAnimationFrame(ready);
  }
  ready();
})();

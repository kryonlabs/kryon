(async () => {
  const {createEditor} = await import('./live-editor.js');
  createEditor({sourceURL: 'demo/src/app.zi', staticURL: 'assets/widgets.html', staticPreview: document.getElementById('demo-preview')});
})().catch(error => {
  document.getElementById('status').textContent = 'The preview could not load. Reload to try again.';
  document.getElementById('preview-loading').hidden = true;
  console.error(error);
});

// Theme observations for the prebuilt Canvas host. Ziran owns the palette.
(() => {
  const query = new URLSearchParams(location.search);
  Module.previewTheme = query.get('color') === 'dark' || query.get('theme') === 'dark' ? 2 : query.get('theme') === 'waozi' ? 1 : 0;
  addEventListener('message', ({source, data}) => {
    if (source === parent && data?.type === 'preview:theme' && [0, 1, 2].includes(data.theme)) Module.previewTheme = data.theme;
  });
})();

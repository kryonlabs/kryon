(() => {
  const entries = window.galleryScenes;
  const list = document.getElementById('widget-list');
  const frame = document.getElementById('widget-preview');
  const stage = document.getElementById('widget-stage');
  const status = document.getElementById('preview-status');
  const disabled = document.getElementById('disabled-state');
  let selected = 0, ready = false;
  const buttons = entries.map(entry => {
    const button = document.createElement('button');
    button.type = 'button'; button.textContent = entry.name;
    button.dataset.scene = entry.index;
    button.addEventListener('click', () => select(entry.index, true));
    list.append(button); return button;
  });
  document.getElementById('inventory').textContent = `${entries.length} entries · ${entries.filter(entry => entry.available).length} live previews`;
  function send() {
    if (!ready) return;
    frame.contentWindow.postMessage({type: 'gallery:select', scene: selected, dark: document.documentElement.dataset.theme === 'dark', disabled: disabled.checked}, location.origin);
  }
  function select(index, updateHash = false) {
    if (!Number.isInteger(index) || !entries[index]) index = 0;
    selected = index;
    const entry = entries[index];
    buttons.forEach((button, i) => button.setAttribute('aria-current', String(i === index)));
    document.getElementById('preview-heading').textContent = entry.name;
    document.getElementById('widget-package').textContent = index < 54 ? 'Kryon / Core' : index < 60 ? 'Plot / Charts' : 'DataViews';
    document.getElementById('widget-source').href = entry.source;
    frame.title = `Interactive ${entry.name} preview`;
    document.getElementById('widget-note').textContent = !entry.available ? 'TableView exports layout and selection APIs, but this revision has no maintained table renderer.' : index === 61 ? 'Drag a card to another column. The board keeps the new position.' : index === 43 ? 'Drag a handle to reorder the rows.' : index === 44 ? 'Drag the divider to resize the panes.' : 'Click a control to try it. Tab moves keyboard focus.';
    if (updateHash) history.replaceState(null, '', `#${index}`);
    send();
  }
  function resize() {
    const width = stage.clientWidth;
    const scale = Math.min(width / 480, 1);
    frame.style.transform = `scale(${scale})`;
    frame.style.marginLeft = `${Math.max(0, (width - 480 * scale) / 2)}px`;
    stage.style.height = `${320 * scale + 2}px`;
  }
  addEventListener('message', event => {
    if (event.source !== frame.contentWindow || event.origin !== location.origin) return;
    if (event.data?.type === 'gallery:ready') { ready = true; status.textContent = 'Live · compiled from Ziran'; send(); }
    if (event.data?.type === 'gallery:error') { status.textContent = 'The preview could not start. Reload to try again.'; }
  });
  addEventListener('themechange', send);
  addEventListener('hashchange', () => select(Number(location.hash.slice(1))));
  disabled.addEventListener('change', send);
  new ResizeObserver(resize).observe(stage);
  select(Number(location.hash.slice(1)));
  resize();
})();

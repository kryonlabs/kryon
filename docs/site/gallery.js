import {createEditor} from './live-editor.js';

const entries = window.galleryScenes;
const list = document.getElementById('widget-list');
const frame = document.getElementById('widget-preview');
const liveFrame = document.getElementById('live-preview');
const stage = document.getElementById('widget-stage');
const status = document.getElementById('preview-status');
const disabled = document.getElementById('disabled-state');
const search = window.galleryCompilerPaths.join('\n');
let selected = 0, ready = false;
function fromHash() {
  const id = decodeURIComponent(location.hash.slice(1));
  return entries.find(entry => entry.id === id)?.index ?? (entries[Number(id)] ? Number(id) : 0);
}
selected = fromHash();
const editor = createEditor({sourceURL: entries[selected].code, search, staticURL: 'assets/widget-gallery.html', staticPreview: frame, autoCompile: false});
const buttons = new Map();
for (const module of window.galleryModules) {
  const moduleEntries = entries.filter(entry => entry.moduleId === module.id);
  if (!moduleEntries.length) continue;
  const section = document.createElement('section'); section.className = 'widget-module'; section.dataset.module = module.id;
  const heading = document.createElement('h2'); heading.textContent = module.label; section.append(heading);
  for (const category of new Set(moduleEntries.map(entry => entry.category))) {
    const group = document.createElement('section'); group.className = 'widget-category'; group.dataset.category = category;
    const title = document.createElement('h3'); title.textContent = category; group.append(title);
    for (const entry of moduleEntries.filter(entry => entry.category === category)) {
      const button = document.createElement('button'); button.type = 'button'; button.textContent = entry.name; button.dataset.scene = entry.index;
      button.addEventListener('click', () => select(entry.index, true)); group.append(button); buttons.set(entry.index, button);
    }
    section.append(group);
  }
  list.append(section);
}
function send() {
  if (ready) frame.contentWindow.postMessage({type: 'gallery:select', scene: selected, dark: document.documentElement.dataset.theme === 'dark', disabled: disabled.checked}, location.origin);
  editor.setOptions({disabled: disabled.checked});
}
function select(index, updateHash = false, updateEditor = true) {
  if (!Number.isInteger(index) || !entries[index]) index = 0;
  selected = index;
  const entry = entries[index];
  for (const [number, button] of buttons) button.setAttribute('aria-current', String(number === index));
  document.getElementById('preview-heading').textContent = entry.name;
  document.getElementById('widget-package').textContent = entry.module + ' / ' + entry.category;
  document.getElementById('widget-source').href = entry.source;
  frame.title = 'Interactive ' + entry.name + ' preview';
  liveFrame.title = 'Your edited ' + entry.name + ' preview';
  document.getElementById('widget-note').textContent = entry.note;
  if (updateHash) history.replaceState(null, '', '#' + entry.id);
  if (updateEditor) editor.setExample({sourceURL: entry.code, search});
  send();
}
function resize() {
  if (!stage.clientWidth || !stage.clientHeight) return;
  const scale = Math.min(stage.clientWidth / 480, stage.clientHeight / 320, 1);
  for (const item of [frame, liveFrame]) {
    item.style.transform = `scale(${scale})`;
    item.style.marginLeft = `${Math.max(0, (stage.clientWidth - 480 * scale) / 2)}px`;
    item.style.top = `${Math.max(0, (stage.clientHeight - 320 * scale) / 2)}px`;
  }
}
addEventListener('message', event => {
  if (event.source !== frame.contentWindow || event.origin !== location.origin) return;
  if (event.data?.type === 'gallery:ready') { ready = true; status.textContent = 'Live preview'; send(); }
  if (event.data?.type === 'gallery:error') status.textContent = 'The preview could not start. Reload to try again.';
});
addEventListener('themechange', send);
addEventListener('hashchange', () => select(fromHash()));
disabled.addEventListener('change', send);
new ResizeObserver(resize).observe(stage);
select(selected, false, false);
resize();

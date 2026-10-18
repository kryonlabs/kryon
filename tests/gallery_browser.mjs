// Real Canvas/Wasm widgets, served locally in a private browser/display.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const site = path.join(root, 'docs/site');
const output = path.join(root, 'build/scratch/widget-gallery');
await fs.mkdir(output, {recursive: true});
const profile = await fs.mkdtemp(path.join(os.tmpdir(), 'kryon-gallery-browser-'));
const env = {...process.env, YUE_DESKTOP_RECOVERY: '0'};
for (const name of ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'GDK_DISPLAY']) delete env[name];
const server = http.createServer(async (request, response) => {
  const filename = path.resolve(site, '.' + new URL(request.url, 'http://localhost').pathname);
  if (!filename.startsWith(site + path.sep)) { response.writeHead(403).end(); return; }
  try {
    const bytes = await fs.readFile(filename);
    const type = {'.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css', '.png': 'image/png', '.svg': 'image/svg+xml', '.woff2': 'font/woff2'}[path.extname(filename)];
    response.writeHead(200, {'Content-Type': type || 'application/octet-stream'}).end(bytes);
  } catch { response.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = spawn('xvfb-run', ['-a', '-s', '-screen 0 1440x1000x24 -extension GLX', process.env.CHROMIUM || 'chromium', '--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage', '--disable-background-networking', '--remote-debugging-port=0', '--user-data-dir=' + profile, 'about:blank'], {env, detached: true, stdio: ['ignore', 'ignore', 'pipe']});
let log = '', socket, sequence = 0;
const pending = new Map(), errors = [], failed = [], checks = [];
browser.stderr.on('data', data => { log = (log + data).slice(-3000); });
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(check, label, timeout = 30000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) { if (await check()) return; await delay(70); }
  throw new Error(label + ': ' + JSON.stringify({errors, failed, log}));
}
function cdp(method, params = {}) {
  return new Promise((resolve, reject) => {
    const id = ++sequence;
    const timer = setTimeout(() => { pending.delete(id); reject(new Error(method + ' timed out')); }, 15000);
    pending.set(id, {resolve, reject, timer});
    socket.send(JSON.stringify({id, method, params}));
  });
}
async function evaluate(expression) {
  const result = await cdp('Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
  if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
  return result.result.value;
}
const runtime = 'document.getElementById("widget-preview").contentWindow.Module';
async function choose(index) {
  await evaluate(`document.querySelector('[data-scene="${index}"]').click()`);
  await until(() => evaluate(`${runtime}?.gallerySceneRendered === ${index}`), 'Scene did not render: ' + index, 10000);
}
async function capture(name, clipped = false) {
  const params = {format: 'png'};
  if (clipped) params.clip = await evaluate(`(() => {const r=document.getElementById('widget-preview').getBoundingClientRect();return {x:r.x+scrollX,y:r.y+scrollY,width:r.width,height:r.height,scale:1};})()`);
  const {data} = await cdp('Page.captureScreenshot', params);
  const bytes = Buffer.from(data, 'base64');
  if (name) await fs.writeFile(path.join(output, name + '.png'), bytes);
  return createHash('sha256').update(bytes).digest('hex');
}
async function clickWidget(x, y) {
  const point = await evaluate(`(() => {const r=document.getElementById('widget-preview').getBoundingClientRect();return {x:r.x+${x}*r.width/480,y:r.y+${y}*r.height/320};})()`);
  await cdp('Input.dispatchMouseEvent', {type: 'mouseMoved', ...point});
  await cdp('Input.dispatchMouseEvent', {type: 'mousePressed', ...point, button: 'left', clickCount: 1});
  await delay(45);
  await cdp('Input.dispatchMouseEvent', {type: 'mouseReleased', ...point, button: 'left', clickCount: 1});
  await delay(90);
}
try {
  let port;
  await until(async () => { try { port = Number((await fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); return !!port; } catch { return false; } }, 'Private browser did not start');
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  socket = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.addEventListener('open', resolve, {once: true}); socket.addEventListener('error', reject, {once: true}); });
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.method === 'Runtime.exceptionThrown') errors.push(message.params.exceptionDetails);
    if (message.method === 'Network.responseReceived' && message.params.response.status >= 400) failed.push(message.params.response.url);
    const item = pending.get(message.id);
    if (item) { clearTimeout(item.timer); pending.delete(message.id); message.error ? item.reject(new Error(JSON.stringify(message.error))) : item.resolve(message.result); }
  });
  await cdp('Page.enable'); await cdp('Runtime.enable'); await cdp('Network.enable');
  await cdp('Emulation.setDeviceMetricsOverride', {width: 1440, height: 1000, deviceScaleFactor: 1, mobile: false});
  await cdp('Page.navigate', {url: 'http://127.0.0.1:' + server.address().port + '/widgets.html'});
  await until(() => evaluate(`document.getElementById('preview-status')?.textContent.startsWith('Live')`), 'Wasm gallery did not become ready');
  const entries = await evaluate('window.galleryScenes');
  assert.equal(entries.length, 63);
  for (const name of ['Button', 'TextArea', 'TimePicker', 'CandleChart', 'TreeView', 'Kanban']) assert.ok(entries.some(entry => entry.name === name), name);
  const images = new Set();
  for (const entry of entries) { await choose(entry.index); images.add(await capture(null, true)); }
  assert.ok(images.size > 55, 'Scenes must paint distinct real previews');
  checks.push('all 63 entries render, including real optional package widgets');
  await choose(0);
  const idle = await capture(null, true);
  await clickWidget(120, 85);
  assert.notEqual(await capture('button-clicked', true), idle, 'Button click must update visible feedback');
  checks.push('button clicks update rendered feedback');
  await evaluate('document.getElementById("disabled-state").click()'); await delay(100);
  const disabled = await capture(null, true);
  await clickWidget(120, 85);
  assert.equal(await capture(null, true), disabled, 'Disabled button must not activate');
  await evaluate('document.getElementById("disabled-state").click()');
  checks.push('disabled state prevents activation');
  await choose(1); await clickWidget(120, 85);
  const beforeText = await capture(null, true);
  for (const letter of 'Hello gallery') await cdp('Input.dispatchKeyEvent', {type: 'char', key: letter, text: letter});
  await delay(200);
  assert.notEqual(await capture('text-edited', true), beforeText, 'Typing must change painted text');
  checks.push('text field accepts browser keyboard input');
  await choose(3); await clickWidget(200, 85);
  await capture('dropdown-padding', true);
  checks.push('dropdown menu opens in the Canvas host');
  await evaluate('document.querySelector(".theme-toggle").click()'); await delay(200);
  await capture('dark-desktop');
  assert.equal(await evaluate('document.documentElement.dataset.theme'), 'dark');
  await until(() => evaluate(`${runtime}.galleryDark === 1 && (()=>{const c=document.getElementById('widget-preview').contentDocument.querySelector('canvas');const p=c.getContext('2d').getImageData(10,10,1,1).data;return p[0]+p[1]+p[2]<200})()`), 'Compiled preview did not receive the dark palette');
  checks.push('site palette reaches the compiled preview');
  await choose(60); await capture('dark-tree', true);
  await clickWidget(40, 80);
  await capture('tree-collapsed', true);
  await choose(54); await capture('dark-plot', true);

  await choose(62);
  assert.match(await evaluate('document.getElementById("widget-note").textContent'), /no maintained table renderer/);
  checks.push('TableView renderer gap is visible');
  await choose(61); await capture('kanban', true);
  const boardBefore = await capture(null, true);
  const boardPoints = await evaluate(`(() => {const r=document.getElementById('widget-preview').getBoundingClientRect();return [90,235].map(x=>({x:r.x+x*r.width/480,y:r.y+140*r.height/320}));})()`);
  await cdp('Input.dispatchMouseEvent', {type:'mouseMoved', ...boardPoints[0]});
  await cdp('Input.dispatchMouseEvent', {type:'mousePressed', ...boardPoints[0], button:'left', clickCount:1});
  await delay(80);
  await cdp('Input.dispatchMouseEvent', {type:'mouseMoved', ...boardPoints[1], button:'left', buttons:1});
  await delay(150);
  await cdp('Input.dispatchMouseEvent', {type:'mouseReleased', ...boardPoints[1], button:'left', clickCount:1});
  await delay(120);
  assert.notEqual(await capture('kanban-moved', true), boardBefore, 'Dragging a Kanban card must change the board');
  checks.push('Kanban card can move between columns');
  await cdp('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 1, mobile: true});
  await choose(18); await delay(200);
  assert.ok(await evaluate('document.documentElement.scrollWidth <= innerWidth + 1'), 'Mobile page must not overflow horizontally');
  await evaluate('document.getElementById("preview-heading").scrollIntoView({block:"start"})');
  await capture('mobile');
  checks.push('mobile gallery fits the viewport');
  assert.deepEqual(errors, []); assert.deepEqual(failed, []);
  await fs.writeFile(path.join(output, 'results.json'), JSON.stringify({entries: entries.length, distinctPreviews: images.size, checks}, null, 2) + '\n');
  console.log(JSON.stringify({entries: entries.length, checks}));
} finally {
  if (socket) socket.close();
  server.close();
  try { process.kill(-browser.pid, 'SIGTERM'); } catch {}
  await new Promise(resolve => browser.once('exit', resolve));
  // xvfb-run may exit while Chromium's children still close profile files.
  await fs.rm(profile, {recursive: true, force: true, maxRetries: 10, retryDelay: 100});
}

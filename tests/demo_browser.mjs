// Exercise the real browser compiler and Canvas host on a private display.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {inflateSync} from 'node:zlib';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const site = path.join(root, 'docs/site');
const output = path.join(root, 'build/scratch/web-preview');
await fs.mkdir(output, {recursive: true});
const profile = await fs.mkdtemp(path.join(os.tmpdir(), 'kryon-demo-browser-'));
const env = {...process.env};
for (const name of ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY']) delete env[name];
const server = http.createServer(async (request, response) => {
  const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
  const file = path.resolve(site, '.' + pathname);
  if (!file.startsWith(site + path.sep)) { response.writeHead(403).end(); return; }
  try {
    const data = await fs.readFile(file);
    const mime = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.wasm': 'application/wasm', '.svg': 'image/svg+xml', '.png': 'image/png', '.woff2': 'font/woff2', '.zi': 'text/plain'}[path.extname(file)];
    response.writeHead(200, {'Content-Type': mime || 'application/octet-stream'}).end(data);
  } catch { response.writeHead(404).end(); }
});
const targetURL = process.argv[2];
if (!targetURL) await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const url = targetURL || 'http://127.0.0.1:' + server.address().port + '/demo.html';
const browser = spawn('xvfb-run', ['-a', process.env.CHROMIUM || 'chromium', '--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage', '--disable-background-networking', '--disable-site-isolation-trials', '--remote-debugging-port=0', '--user-data-dir=' + profile, 'about:blank'], {env, detached: true, stdio: ['ignore', 'ignore', 'pipe']});
let browserLog = '', socket, sequence = 0;
const pending = new Map(), exceptions = [], requests = [], consoleLines = [];
browser.stderr.on('data', chunk => { browserLog = (browserLog + chunk).slice(-4000); });
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function waitFor(check, label, timeout = 25000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) { if (await check()) return; await delay(80); }
  let state;
  try { state = await evaluate(`({status:document.getElementById('status')?.textContent,diagnostics:document.getElementById('diagnostics')?.textContent})`); } catch {}
  throw new Error(label + ': ' + JSON.stringify({state, exceptions, consoleLines, browserLog}));
}
function cdp(method, params = {}) {
  return new Promise((resolve, reject) => {
    const id = ++sequence;
    const timer = setTimeout(() => { pending.delete(id); reject(new Error('CDP timeout: ' + method)); }, 15000);
    pending.set(id, {resolve, reject, timer});
    socket.send(JSON.stringify({id, method, params}));
  });
}
async function evaluate(expression, contextId) {
  const params = {expression, returnByValue: true, awaitPromise: true};
  if (contextId) params.contextId = contextId;
  const result = await cdp('Runtime.evaluate', params);
  if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
  return result.result.value;
}
async function settled() {
  await waitFor(() => evaluate(`document.getElementById('status').dataset.state !== 'busy'`), 'Preview did not settle');
  assert.equal(await evaluate(`document.getElementById('diagnostics').hidden`), true,
    await evaluate(`document.getElementById('diagnostics').textContent`));
}
async function edit(source) {
  await evaluate(`(() => {const editor=document.getElementById('source-editor');editor.value=${JSON.stringify(source)};editor.dispatchEvent(new Event('input'));})()`);
}
async function pixelHash() {
  // Capture the painted preview across the browser's iframe process boundary.
  const clip = await evaluate(`(() => {const box=document.getElementById('live-preview').getBoundingClientRect();return {x:box.x+scrollX,y:box.y+scrollY,width:box.width,height:box.height,scale:1};})()`);
  const {data} = await cdp('Page.captureScreenshot', {format: 'png', clip});
  return createHash('sha256').update(Buffer.from(data, 'base64')).digest('hex');
}
async function previewPixel(point, embedded = false) {
  const clip = await evaluate(`(() => {
    const parentFrame = ${embedded} ? document.querySelector('iframe[src="demo.html?mini"]') : null;
    const page = parentFrame ? parentFrame.contentDocument : document;
    const frame = page.querySelector('#live-preview.is-active') || page.getElementById('demo-preview');
    const box = frame.getBoundingClientRect(), outer = parentFrame?.getBoundingClientRect();
    const left = (box.width - Math.min(500, box.width - 40)) / 2;
    return {x: Math.floor(scrollX + (outer?.x || 0) + box.x + (${JSON.stringify(point)} === 'button' ? left + 24 : 10)),
      y: Math.floor(scrollY + (outer?.y || 0) + box.y + (${JSON.stringify(point)} === 'button' ? 120 : 10)), width: 1, height: 1, scale: 1};
  })()`);
  const {data} = await cdp('Page.captureScreenshot', {format: 'png', clip});
  // Decode a single painted pixel, including cross-process sandboxed frames.
  // With one pixel, every PNG filter's previous and neighboring bytes are zero.
  const png = Buffer.from(data, 'base64'), parts = [];
  let colorType;
  for (let offset = 8; offset < png.length;) {
    const length = png.readUInt32BE(offset), type = png.toString('ascii', offset + 4, offset + 8);
    const chunk = png.subarray(offset + 8, offset + 8 + length);
    if (type === 'IHDR') {
      assert.equal(chunk.readUInt32BE(0), 1); assert.equal(chunk.readUInt32BE(4), 1);
      assert.equal(chunk[8], 8); assert.equal(chunk[12], 0);
      colorType = chunk[9];
    }
    if (type === 'IDAT') parts.push(chunk);
    offset += length + 12;
  }
  assert.ok(colorType === 2 || colorType === 6, 'Screenshot is RGB or RGBA');
  const scanline = inflateSync(Buffer.concat(parts));
  return [...scanline.subarray(1, 4)];
}
async function expectPreviewColor(point, expected, label, embedded = false) {
  let actual;
  try {
    await waitFor(async () => {
      actual = await previewPixel(point, embedded);
      return actual.every((component, index) => component === expected[index]);
    }, label);
  } catch (error) {
    throw new Error(`${label}: expected RGB ${expected}, observed ${actual}`, {cause: error});
  }
  assert.deepEqual(actual, expected, label);
}
async function screenshot(name) {
  const {data} = await cdp('Page.captureScreenshot', {format: 'png'});
  await fs.writeFile(path.join(output, name + '.png'), Buffer.from(data, 'base64'));
}

try {
  let port;
  await waitFor(async () => { try { port = Number((await fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]); return !!port; } catch { return false; } }, 'Private browser did not start');
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  socket = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.addEventListener('open', resolve, {once: true}); socket.addEventListener('error', reject, {once: true}); });
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.method === 'Runtime.exceptionThrown') exceptions.push(message.params.exceptionDetails);
    if (message.method === 'Runtime.consoleAPICalled') { consoleLines.push(message.params.args.map(item => item.value || item.description).join(' ')); if (consoleLines.length > 30) consoleLines.shift(); }
    if (message.method === 'Network.requestWillBeSent') requests.push(message.params.request.url);
    if (message.id && pending.has(message.id)) {
      const item = pending.get(message.id); pending.delete(message.id); clearTimeout(item.timer);
      if (message.error) item.reject(new Error(JSON.stringify(message.error))); else item.resolve(message.result);
    }
  });
  await cdp('Page.enable'); await cdp('Runtime.enable'); await cdp('Network.enable');
  await cdp('Emulation.setDeviceMetricsOverride', {width: 1440, height: 1080, deviceScaleFactor: 1, mobile: false});
  const started = Date.now();
  await cdp('Page.navigate', {url});
  await waitFor(() => evaluate(`document.getElementById('status')?.textContent === 'Live · changes compiled'`), 'Default source did not render');
  console.log('Initial compile and rendering: ' + (Date.now() - started) + ' ms');
  assert.equal(await evaluate(`document.getElementById('workspace').dataset.view`), 'split');
  const original = await evaluate(`document.getElementById('source-editor').value`);
  await expectPreviewColor('background', [247, 243, 235], 'The initial live preview uses the light palette');
  const initialPixels = await pixelHash();
  await screenshot('desktop');
  const edited = original.replace('"Made with Kryon"', '"Edited in the browser"');
  const editedAt = Date.now();
  await edit(edited);
  await settled();
  assert.notEqual(await pixelHash(), initialPixels, 'Editing source changes actual rendered pixels');
  console.log('Live edit and rendering: ' + (Date.now() - editedAt) + ' ms');
  await edit('using UI :: #import "kryon/Widgets";\n#program_export\nFrame :: (session: Session, viewport: Rectangle) -> s32 { while true {} return 0; }');
  await waitFor(() => evaluate(`!document.getElementById('diagnostics').hidden`), 'The runaway frame did not stop');
  assert.match(await evaluate(`document.getElementById('diagnostics').textContent`), /stopped after 1000000 statements/);
  await edit(edited);
  await settled();
  await edit('Frame :: (');
  await waitFor(() => evaluate(`!document.getElementById('diagnostics').hidden`), 'The parser error did not appear');
  await edit(edited);
  await settled();
  const beforeClick = await pixelHash();
  const bounds = await evaluate(`(() => {const box=document.getElementById('live-preview').getBoundingClientRect();return {x:box.x,y:box.y,width:box.width};})()`);
  const cardWidth = Math.min(500, bounds.width - 40);
  const x = bounds.x + (bounds.width - cardWidth) / 2 + 40, y = bounds.y + 125;
  await cdp('Input.dispatchMouseEvent', {type: 'mousePressed', x, y, button: 'left', clickCount: 1});
  await delay(120);
  await cdp('Input.dispatchMouseEvent', {type: 'mouseReleased', x, y, button: 'left', clickCount: 1});
  await delay(400);
  assert.notEqual(await pixelHash(), beforeClick, 'The compiled button changes app state across VM frames');
  await cdp('Input.dispatchMouseEvent', {type: 'mouseMoved', x: bounds.x + 5, y: bounds.y + 5});
  await delay(150);
  const beforeTheme = await pixelHash();
  const lightEditor = await evaluate(`getComputedStyle(document.querySelector('.code-area')).backgroundColor`);
  await evaluate(`document.querySelector('.theme-toggle').click()`);
  await expectPreviewColor('background', [24, 34, 31], 'Switching to dark changes the rendered app background');
  await expectPreviewColor('button', [181, 212, 169], 'Switching to dark changes rendered widget styles');
  assert.notEqual(await evaluate(`getComputedStyle(document.querySelector('.code-area')).backgroundColor`), lightEditor, 'The source editor follows the theme too');
  await screenshot('dark-preview');
  await evaluate(`document.querySelector('.theme-toggle').click()`);
  await expectPreviewColor('background', [247, 243, 235], 'Switching back restores the light palette');
  assert.equal(await pixelHash(), beforeTheme, 'Theme changes preserve the running app state');
  const lastGoodPixels = await pixelHash();
  await edit(edited.replace('return 0', 'return missing_value'));
  await waitFor(() => evaluate(`!document.getElementById('diagnostics').hidden`), 'Compile error did not appear');
  assert.match(await evaluate(`document.getElementById('diagnostics').textContent`), /missing_value/);
  assert.equal(await pixelHash(), lastGoodPixels, 'Compile errors preserve the last working preview');
  await screenshot('diagnostics');
  await edit('using UI :: #import "kryon/Widgets";\nhost_api :: #system_library "host_api";\nOtherCall :: () -> s32 #foreign host_api;\n#program_export\nFrame :: (session: Session, viewport: Rectangle) -> s32 { return OtherCall(); }');
  await waitFor(() => evaluate(`document.getElementById('diagnostics').textContent.includes('OtherCall')`), 'The unsupported capability was not reported');
  await delay(150);
  assert.equal(await evaluate(`document.getElementById('status').dataset.state`), 'error', 'Rendering the previous program must not mark a rejected bundle successful');
  assert.equal(await pixelHash(), lastGoodPixels, 'Unsupported capabilities preserve the working instance');
  await edit(edited);
  await settled();
  await evaluate(`document.querySelector('[data-view="source"]').click()`);
  assert.equal(await evaluate(`document.getElementById('preview-panel').hidden`), true);
  await evaluate(`document.querySelector('[data-view="preview"]').click()`);
  assert.equal(await evaluate(`document.getElementById('source-editor').value`), edited, 'View switching preserves edits');
  await evaluate(`document.querySelector('[data-view="split"]').click()`);
  await edit(edited + '\n// <img src=x onerror=alert(1)>');
  assert.equal(await evaluate(`document.querySelectorAll('#highlight img').length`), 0, 'Highlighted source is escaped');
  await settled();
  await evaluate(`document.getElementById('compile').click();document.getElementById('stop').click()`);
  await delay(300);
  assert.equal(await evaluate(`document.getElementById('status').textContent`), 'Compilation stopped', 'Stopped worker results are ignored');
  await evaluate(`document.getElementById('reset').click()`);
  await settled();
  assert.equal(await evaluate(`document.getElementById('source-editor').value`), original);
  const styled = original.replace('    InstallPreviewStyle()', `    InstallPreviewStyle()
    rules: StyleRules
    rule: StyleRule
    rule.selector = StyleDefaultSelector()
    rule.selector.kind = StyleKindButton()
    rule.selector.tone = cast(s32)ButtonTone.ButtonToneAccent
    rule.style.fields = cast(u32)StyleField.StyleBackground | cast(u32)StyleField.StyleBorder | cast(u32)StyleField.StyleMaterial
    rule.style.background = 0xb02a60ff
    rule.style.border = 0xb02a60ff
    rule.style.material = MaterialKind.MaterialFlat
    rules.items[0] = rule
    rules.count = 1
    InstallStyleRules(rules)`);
  await edit(styled);
  await settled();
  await expectPreviewColor('button', [176, 42, 96], 'StyleRules edited in source change the actual widget color');
  await screenshot('source-style');
  await edit(original);
  await settled();
  await cdp('Emulation.setDeviceMetricsOverride', {width: 390, height: 640, deviceScaleFactor: 1, mobile: true});
  await delay(200);
  assert.equal(await evaluate(`document.getElementById('workspace').dataset.view`), 'preview');
  assert.equal(await evaluate(`document.documentElement.scrollWidth <= innerWidth`), true, 'The narrow editor does not overflow horizontally');
  await screenshot('mobile');
  requests.length = 0;
  await cdp('Page.navigate', {url: url + '?mini&theme=waozi'});
  await waitFor(() => evaluate(`document.readyState === 'complete'`), 'Embedded demo did not load');
  assert.equal(requests.some(request => /playground-runtime|preview\.html/.test(request)), false, 'Embedded preview loads the compiler only when source is opened');
  await expectPreviewColor('button', [49, 94, 72], 'The prebuilt embedded demo uses the requested Waozi theme');
  await screenshot('embedded-preview');
  await evaluate(`document.querySelector('[data-view="source"]').click()`);
  await waitFor(() => evaluate(`document.getElementById('status').textContent === 'Compiled · open Preview'`), 'Embedded editor did not compile');
  await screenshot('embedded-source');
  await evaluate(`document.querySelector('[data-view="preview"]').click()`);
  await waitFor(() => evaluate(`document.getElementById('status').textContent === 'Live · changes compiled'`), 'The compiled embedded preview did not render');
  await expectPreviewColor('button', [49, 94, 72], 'Compiling the embedded source preserves its Waozi widget theme');
  assert.equal(await evaluate(`document.documentElement.scrollWidth <= innerWidth`), true);
  await cdp('Emulation.setDeviceMetricsOverride', {width: 1440, height: 1080, deviceScaleFactor: 1, mobile: false});
  await cdp('Page.navigate', {url: new URL('index.html', url).href});
  await waitFor(() => evaluate(`(() => {const page=document.querySelector('iframe[src="demo.html?mini"]')?.contentDocument;return page?.readyState === 'complete' && !!page.getElementById('demo-preview');})()`), 'The homepage embed did not load');
  assert.equal(await evaluate(`(() => {const page=document.querySelector('iframe[src="demo.html?mini"]').contentDocument.documentElement;return page.scrollHeight <= page.clientHeight;})()`), true, 'The homepage embed shows its preview, status, and full-editor link without vertical overflow');
  assert.equal(await evaluate(`!!document.querySelector('.hero .actions a[href="demo.html"]')`), true, 'The homepage links directly to the live editor');
  await evaluate(`document.querySelector('iframe[src="demo.html?mini"]').scrollIntoView()`);
  await evaluate(`document.querySelector('.theme-toggle').click()`);
  await waitFor(() => evaluate(`document.querySelector('iframe[src="demo.html?mini"]').contentDocument.documentElement.dataset.theme === 'dark'`), 'The embedded editor did not receive the homepage theme');
  await expectPreviewColor('background', [24, 34, 31], 'The homepage theme reaches the embedded Canvas app', true);
  await expectPreviewColor('button', [181, 212, 169], 'The homepage theme reaches embedded widget styles', true);
  await evaluate(`window.beforeThemeReload = true`);
  await cdp('Page.reload');
  await waitFor(() => evaluate(`(() => {const page=document.querySelector('iframe[src="demo.html?mini"]')?.contentDocument;return !window.beforeThemeReload && page?.readyState === 'complete' && !!page.getElementById('demo-preview');})()`), 'The saved-theme homepage did not load');
  assert.equal(await evaluate(`document.documentElement.dataset.theme`), 'dark', 'The saved theme survives reload');
  await evaluate(`document.querySelector('iframe[src="demo.html?mini"]').scrollIntoView()`);
  await expectPreviewColor('background', [24, 34, 31], 'The reloaded embedded preview follows the saved theme', true);
  assert.deepEqual(exceptions, [], 'There are no browser runtime exceptions');
  console.log('Kryon live editor: compilation, rendered theme colors, source StyleRules, state-preserving theme switching, inherited and saved themes, interaction, errors, bounded execution, recovery, view switching, cancellation, reset, escaped source, responsive layout, lazy loading, and homepage integration passed');
} finally {
  if (socket) socket.close();
  try { process.kill(-browser.pid, 'SIGTERM'); } catch {}
  await new Promise(resolve => { if (browser.exitCode !== null || browser.signalCode !== null) resolve(); else browser.once('exit', resolve); });
  if (server.listening) { server.closeAllConnections(); await new Promise(resolve => server.close(resolve)); }
  await fs.rm(profile, {recursive: true, force: true});
}

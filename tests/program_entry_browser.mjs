// Load a program entry's page in a private headless Chromium and wait until
// its own main has named the page and painted the whole canvas.
import {spawn} from 'node:child_process';
import {readFile, stat} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';

const [html, profile] = process.argv.slice(2);
const launchedAt = Date.now();
const env = {...process.env, YUE_DESKTOP_RECOVERY: '0'};
for (const name of ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER']) delete env[name];
const browser = spawn(process.env.CHROMIUM || 'chromium', ['--headless', '--no-sandbox', '--disable-gpu',
  '--disable-dev-shm-usage', '--no-first-run', '--no-default-browser-check', '--remote-debugging-port=0',
  '--user-data-dir=' + profile, '--window-size=640,480', html], {env, stdio: ['ignore', 'ignore', 'pipe']});
let errors = '';
browser.stderr.on('data', (data) => { errors = (errors + data).slice(-8000); });
let socket;
try {
  const deadline = Date.now() + 40000;
  let port;
  while (Date.now() < deadline && !port) {
    try {
      const path = join(profile, 'DevToolsActivePort');
      if ((await stat(path)).mtimeMs >= launchedAt) [port] = (await readFile(path, 'utf8')).split('\n');
    } catch {}
    if (browser.exitCode !== null) throw new Error('Chromium exited: ' + errors);
    if (!port) await delay(50);
  }
  if (!port) throw new Error('Chromium debug endpoint unavailable: ' + errors);
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  socket = new WebSocket(pages.find((page) => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', resolve, {once: true});
    socket.addEventListener('error', reject, {once: true});
  });
  const pending = new Map();
  let sequence = 0;
  socket.addEventListener('message', (event) => {
    const message = JSON.parse(event.data);
    pending.get(message.id)?.(message);
    pending.delete(message.id);
  });
  const evaluate = (expression) => new Promise((resolve, reject) => {
    const id = ++sequence;
    const timer = setTimeout(() => reject(new Error('page evaluation timed out: ' + errors)), 10000);
    pending.set(id, (message) => { clearTimeout(timer); resolve(message.result?.result?.value); });
    socket.send(JSON.stringify({id, method: 'Runtime.evaluate', params: {expression, returnByValue: true}}));
  });
  const state = `(() => {
    const canvas = document.querySelector('canvas');
    if (!canvas) return {title: document.title, painted: 0, pixels: 0};
    const data = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
    let painted = 0;
    for (let index = 3; index < data.length; index += 4) if (data[index]) painted += 1;
    return {title: document.title, painted, pixels: canvas.width * canvas.height};
  })()`;
  let seen = {};
  while (Date.now() < deadline) {
    seen = await evaluate(state) || {};
    if (seen.title === 'Window' && seen.pixels > 0 && seen.painted === seen.pixels) break;
    await delay(100);
  }
  if (seen.title !== 'Window' || !seen.pixels || seen.painted !== seen.pixels) {
    throw new Error('the program did not run in the page: ' + JSON.stringify(seen) + ' ' + errors);
  }
  console.log('Program entry page: main named the page and painted ' + seen.pixels + ' pixels');
} finally {
  socket?.close();
  browser.kill('SIGTERM');
}

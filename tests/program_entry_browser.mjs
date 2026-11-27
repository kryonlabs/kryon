// Load a program entry's page in a private headless Chromium and wait until
// its own main has named the page and booted its canvas, then check the
// canvas's pixels by MODE: "opaque" paints every pixel, "clear" leaves every
// pixel at alpha 0, and "drawn" paints some but leaves most at alpha 0.
import {spawn} from 'node:child_process';
import {readFile, stat} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';

const [html, profile, title, mode] = process.argv.slice(2);
const launchedAt = Date.now();
const env = {...process.env, YUE_DESKTOP_RECOVERY: '0'};
for (const name of ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER']) delete env[name];
const browser = spawn(process.env.CHROMIUM || 'chromium', ['--headless', '--no-sandbox',
  '--enable-unsafe-swiftshader', '--enable-webgl', '--use-gl=angle', '--use-angle=swiftshader',
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
    const gl = canvas.getContext('webgl2');
    if (!gl) return {title: document.title, painted: 0, pixels: 0};
    // The GPU presenter keeps a premultiplied drawing buffer; alpha decides
    // what painted, and readPixels reports it without row flipping.
    const data = new Uint8ClampedArray(gl.drawingBufferWidth * gl.drawingBufferHeight * 4);
    gl.readPixels(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight, gl.RGBA, gl.UNSIGNED_BYTE, data);
    let painted = 0;
    for (let index = 3; index < data.length; index += 4) if (data[index]) painted += 1;
    // The host sets the canvas's touch action as it boots, before main first
    // yields, so its clear has happened by the time the page can be read.
    return {title: document.title, booted: canvas.style.touchAction === 'none', painted,
      pixels: gl.drawingBufferWidth * gl.drawingBufferHeight};
  })()`;
  const expected = (seen) => mode === 'opaque' ? seen.painted === seen.pixels :
    mode === 'clear' ? seen.painted === 0 : seen.painted > 0 && seen.painted < seen.pixels / 2;
  const running = (seen) => seen.title === title && seen.booted && seen.pixels > 0;
  // Reading pixels before the host boots (the canvas is still at its
  // default size) stalls SwiftShader for the page's lifetime, so poll
  // the cheap boot state first and read pixels only once it can present.
  const booting = `(() => ({title: document.title, booted: document.querySelector('canvas')?.style.touchAction === 'none'}))()`;
  let seen = {};
  while (Date.now() < deadline) {
    const light = await evaluate(booting) || {};
    if (!(light.title === title && light.booted)) { await delay(100); continue; }
    seen = await evaluate(state) || {};
    if (running(seen) && expected(seen)) break;
    await delay(100);
  }
  // A later repaint must not change what the page shows.
  for (let sample = 0; sample < 10 && running(seen) && expected(seen); sample += 1) {
    await delay(100);
    seen = await evaluate(state) || {};
  }
  if (!running(seen) || !expected(seen)) {
    throw new Error('the program did not show a ' + mode + ' page: ' + JSON.stringify(seen) + ' ' + errors);
  }
  console.log('Program entry page: main named the page ' + title + ' and painted ' + seen.painted +
    ' of ' + seen.pixels + ' pixels (' + mode + ')');
} finally {
  socket?.close();
  browser.kill('SIGTERM');
}

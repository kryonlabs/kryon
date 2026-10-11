// Drive only the private headless process created here; no desktop connection.
import {spawn} from 'node:child_process';
import {readFile, readdir} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [html, profile, downloads] = process.argv.slice(2);
const browser = spawn(process.env.CHROMIUM || 'chromium', ['--headless', '--no-sandbox', '--disable-gpu',
  '--disable-dev-shm-usage', '--no-first-run', '--no-default-browser-check', '--remote-debugging-port=0',
  '--user-data-dir=' + profile, 'about:blank'], {detached: true, stdio: ['ignore', 'ignore', 'pipe']});
let errors = '';
browser.stderr.on('data', data => { errors = (errors + data).slice(-8000); });
let socket;
try {
  const deadline = Date.now() + 30000;
  let port;
  while (Date.now() < deadline) {
    try { port = (await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]; break; } catch {}
    if (browser.exitCode !== null) throw new Error('Chromium exited: ' + errors);
    await delay(50);
  }
  if (!port) throw new Error('Chromium debug endpoint unavailable: ' + errors);
  const version = await (await fetch('http://127.0.0.1:' + port + '/json/version')).json();
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  // Downloads are allowed by the browser target, the page is driven by its own.
  const browserSocket = new WebSocket(version.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    browserSocket.addEventListener('open', resolve, {once: true});
    browserSocket.addEventListener('error', reject, {once: true});
  });
  browserSocket.send(JSON.stringify({id: 1, method: 'Browser.setDownloadBehavior',
    params: {behavior: 'allow', downloadPath: downloads}}));
  await new Promise(resolve => browserSocket.addEventListener('message', resolve, {once: true}));
  socket = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', resolve, {once: true});
    socket.addEventListener('error', reject, {once: true});
  });
  const pending = new Map();
  let sequence = 0;
  let diagnostics = '';
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.method === 'Runtime.consoleAPICalled' || message.method === 'Runtime.exceptionThrown')
      diagnostics = (diagnostics + JSON.stringify(message.params || {})).slice(-4000);
    if (pending.has(message.id)) { pending.get(message.id)(message); pending.delete(message.id); }
  });
  const command = (method, params = {}) => {
    const id = ++sequence;
    const result = new Promise(resolve => pending.set(id, resolve));
    socket.send(JSON.stringify({id, method, params}));
    return result;
  };
  const evaluate = async expression =>
    (await command('Runtime.evaluate', {expression, returnByValue: true})).result?.result?.value;
  await command('Runtime.enable');
  await command('Page.enable');
  await command('Page.navigate', {url: html});
  let result = 'pending';
  while (Date.now() < deadline) {
    result = await evaluate('document.getElementById("result")?.textContent');
    if (result && result !== 'pending') break;
    await delay(50);
  }
  if (result !== 'PASS') throw new Error('File export test: ' + result + ' ' + diagnostics);
  const expected = {'notes.txt': 'hello export\n', 'report.json': '{"ok":true}'};
  while (Date.now() < deadline) {
    const names = (await readdir(downloads)).filter(name => name in expected);
    if (names.length === 2) break;
    await delay(50);
  }
  for (const [name, text] of Object.entries(expected)) {
    const actual = await readFile(join(downloads, name), 'utf8');
    if (actual !== text) throw new Error(name + ' downloaded as ' + JSON.stringify(actual));
  }
  const links = await evaluate('document.querySelectorAll("a").length');
  if (links !== 0) throw new Error('The export left ' + links + ' links in the page');
  browserSocket.close();
  console.log('File export browser: PASS');
} finally {
  if (socket) socket.close();
  try { process.kill(-browser.pid, 'SIGTERM'); } catch {}
  await new Promise(resolve => {
    if (browser.exitCode !== null || browser.signalCode !== null) resolve(); else browser.once('exit', resolve);
  });
}

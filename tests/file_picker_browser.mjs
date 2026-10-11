// Drive only the private headless process created here; no desktop connection.
import {spawn} from 'node:child_process';
import {readFile, writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [html, profile] = process.argv.slice(2);
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
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  socket = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', resolve, {once: true});
    socket.addEventListener('error', reject, {once: true});
  });
  const pending = new Map();
  const choosers = [];
  let sequence = 0;
  let diagnostics = '';
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.method === 'Page.fileChooserOpened') choosers.push(message.params);
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
  const evaluate = async (expression, userGesture = false) =>
    (await command('Runtime.evaluate', {expression, returnByValue: true, userGesture})).result?.result?.value;
  const chooser = async () => {
    while (Date.now() < deadline) {
      if (choosers.length) return choosers.shift();
      await delay(20);
    }
    const state = await evaluate('JSON.stringify({result: document.getElementById("result")?.textContent, ' +
      'inputs: document.querySelectorAll("input[type=file]").length, open: globalThis.kryonPickOpen, ' +
      'active: navigator.userActivation?.isActive})');
    throw new Error('No file chooser opened ' + state + ' ' + diagnostics);
  };
  await command('Runtime.enable');
  await command('Page.enable');
  await command('DOM.enable');
  await command('Page.setInterceptFileChooserDialog', {enabled: true});
  await command('Page.navigate', {url: html});
  while (Date.now() < deadline &&
    await evaluate('document.getElementById("result")?.textContent + ":" + typeof Module') !== 'pending:object') {
    await delay(20);
  }
  const notes = join(profile, 'notes.txt');
  await writeFile(notes, 'hello picker\n');
  // A chooser needs a user gesture, as when someone clicks Open.
  await evaluate('globalThis.kryonPickOpen = true', true);
  const opened = await chooser();
  await evaluate(`(() => { const input = document.querySelector('input[type=file]');
    globalThis.kryonPickAcceptOk = input.accept === '.txt,.md' &&
      input.getAttribute('aria-label') === 'Open notes'; })()`);
  await command('DOM.setFileInputFiles', {files: [notes], backendNodeId: opened.backendNodeId});
  await evaluate('globalThis.kryonPickCancel = true', true);
  await chooser();
  await evaluate(`document.querySelector('input[type=file]').dispatchEvent(new Event('cancel'))`);
  let result = 'pending';
  while (Date.now() < deadline) {
    result = await evaluate('document.getElementById("result")?.textContent');
    if (result === 'PASS') break;
    if (result && result !== 'pending') throw new Error(result + ' ' + diagnostics);
    await delay(50);
  }
  if (result !== 'PASS') throw new Error('File picker test timed out: ' + result + ' ' + diagnostics);
  const leftover = await evaluate('document.querySelectorAll("input[type=file]").length');
  if (leftover !== 0) throw new Error('The picker left ' + leftover + ' input elements in the page');
  console.log('File picker browser: PASS');
} finally {
  if (socket) socket.close();
  try { process.kill(-browser.pid, 'SIGTERM'); } catch {}
  await new Promise(resolve => {
    if (browser.exitCode !== null || browser.signalCode !== null) resolve(); else browser.once('exit', resolve);
  });
}

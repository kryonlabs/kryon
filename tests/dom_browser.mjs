// Drive only the private headless process created here; no desktop connection.
import {spawn} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';

const [html, profile] = process.argv.slice(2);
const browser = spawn(process.env.CHROMIUM || 'chromium', [
  '--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage',
  '--no-first-run', '--no-default-browser-check', '--remote-debugging-port=0',
  '--user-data-dir=' + profile, html
], {detached: true, stdio: ['ignore', 'ignore', 'pipe']});
let errors = '';
browser.stderr.on('data', data => { errors = (errors + data).slice(-8000); });
let socket;
try {
  const deadline = Date.now() + 25000;
  let port;
  while (Date.now() < deadline) {
    try {
      port = (await readFile(join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0];
      break;
    } catch {}
    if (browser.exitCode !== null) throw new Error('Chromium exited: ' + errors);
    await delay(50);
  }
  if (!port) throw new Error('Chromium debug endpoint unavailable: ' + errors);
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  const page = pages.find(page => page.type === 'page');
  socket = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', resolve, {once: true});
    socket.addEventListener('error', reject, {once: true});
  });
  const pending = new Map();
  let sequence = 0;
  let diagnostics = '';
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.method === 'Runtime.consoleAPICalled') {
      const text = (message.params.args || []).map(arg => arg.value ?? '').join(' ');
      diagnostics = (diagnostics + text).slice(-4000);
    }
    if (message.method === 'Runtime.exceptionThrown') {
      const detail = message.params.exceptionDetails || {};
      diagnostics = (diagnostics + (detail.exception?.description || detail.text || '')).slice(-4000);
    }
    if (pending.has(message.id)) {
      pending.get(message.id)(message);
      pending.delete(message.id);
    }
  });
  async function command(method, params = {}) {
    const id = ++sequence;
    const result = new Promise(resolve => pending.set(id, resolve));
    socket.send(JSON.stringify({id, method, params}));
    return result;
  }
  await command('Runtime.enable');
  let result = 'pending';
  while (Date.now() < deadline && result !== 'PASS') {
    const response = await command('Runtime.evaluate', {
      expression: 'document.getElementById("result")?.textContent',
      returnByValue: true
    });
    result = response.result?.result?.value;
    if (result && result !== 'pending' && result !== 'PASS') {
      throw new Error(result + ' ' + diagnostics);
    }
    await delay(50);
  }
  if (result !== 'PASS') {
    const state = await command('Runtime.evaluate', {
      expression: `JSON.stringify({
        closeRequested: globalThis.__kryonCanvas?.closeRequested,
        closed: globalThis.__kryonCanvas?.closed,
        domNodes: globalThis.__kryonDom?.root?.querySelectorAll('[data-kryon-id]').length
      })`,
      returnByValue: true
    });
    throw new Error('DOM test timed out: ' + result + ' ' +
      state.result?.result?.value + ' ' + errors + ' ' + diagnostics);
  }
  const validation = await command('Runtime.evaluate', {
    expression: `(() => {
      const snapshot = globalThis.__kryonDomSnapshot;
      const failures = [];
      if (!snapshot) failures.push('missing DOM snapshot');
      if (snapshot && snapshot.title !== 'DOM Probe') failures.push('title ' + snapshot.title);
      if (snapshot) {
        const nodes = snapshot.nodes;
        const byTag = tag => nodes.filter(node => node.tag === tag);
        const main = byTag('main');
        const heading = byTag('h1');
        const button = byTag('button');
        const link = byTag('a');
        if (main.length !== 1 || main[0].semantic !== '1') failures.push('main landmark');
        if (heading.length !== 1 || heading[0].text !== 'Semantic DOM') failures.push('heading');
        if (button.length !== 1 || button[0].text !== 'Choose' || button[0].disabled !== true) failures.push('button');
        if (link.length !== 1 || link[0].text !== 'Docs' || link[0].href !== 'https://example.test/docs') failures.push('link');
        if (!nodes.some(node => node.parent === main[0]?.id && node.tag === 'h1')) failures.push('parent identity');
      }
      return failures;
    })()`,
    returnByValue: true
  });
  const failures = validation.result?.result?.value;
  if (!Array.isArray(failures) || failures.length) {
    throw new Error('DOM semantic failures: ' + JSON.stringify(failures) + ' ' + diagnostics);
  }
  console.log('Semantic DOM browser: PASS');
} finally {
  if (socket) socket.close();
  try { process.kill(-browser.pid, 'SIGTERM'); } catch {}
  await new Promise(resolve => {
    if (browser.exitCode !== null || browser.signalCode !== null) resolve();
    else browser.once('exit', resolve);
  });
}

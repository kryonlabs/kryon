// Drive only the private headless process created here; no desktop connection.
import {spawn} from 'node:child_process';
import {readFile, unlink} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';

const [html, profile] = process.argv.slice(2);
// The profile belongs to this test; the prior process was awaited at close.
// Chromium rewrites this endpoint file, but leaving it lets a reused profile
// briefly point at the port of the already-closed previous process.
await unlink(join(profile, 'DevToolsActivePort')).catch(error => {
  if (error.code !== 'ENOENT') throw error;
});
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
  if (process.env.KRYON_DOM_INTERACTION) {
    async function evaluate(expression) {
      const response = await command('Runtime.evaluate', {expression, returnByValue: true});
      if (response.result?.exceptionDetails) throw new Error(JSON.stringify(response.result.exceptionDetails));
      return response.result?.result?.value;
    }
    async function until(expression) {
      const end = Date.now() + 8000;
      while (Date.now() < end) {
        if (await evaluate(expression)) return;
        await delay(50);
      }
      throw new Error('DOM interaction timed out: ' + expression + ' ' + diagnostics);
    }
    await until(`globalThis.__kryonAccepted === 'Aé' && document.querySelector('[aria-label="Editor"]')?.value === 'Aé'`);
    const initial = await evaluate(`(() => {
      const editor = document.querySelector('[aria-label="Editor"]');
      globalThis.__editor = editor;
      editor.focus(); editor.setSelectionRange(3, 3);
      globalThis.__changes = 0;
      globalThis.__changeDetails = [];
      new MutationObserver(changes => { globalThis.__changes += changes.length; globalThis.__changeDetails.push(...changes.map(change => [change.type, change.attributeName, change.target.getAttribute('aria-label')])); }).observe(document.getElementById('kryon-dom-root'), {subtree:true,childList:true,attributes:true});
      return {width:editor.getBoundingClientRect().width, password:document.querySelector('[aria-label="Password"]').type, readonly:document.querySelector('[aria-label="Read only"]').readOnly, color:getComputedStyle(document.querySelector('[aria-label="Run"]')).backgroundColor};
    })()`);
    if (initial.width !== 200 || initial.password !== 'password' || !initial.readonly || initial.color !== 'rgb(17, 34, 51)') {
      throw new Error('Native presentation or KSS mismatch: ' + JSON.stringify(initial));
    }
    await delay(150);
    await evaluate('globalThis.__changes = 0; globalThis.__changeDetails = []');
    await delay(150);
    if (await evaluate('globalThis.__changes') !== 0) throw new Error('Unchanged frames mutated the DOM: ' + JSON.stringify(await evaluate('globalThis.__changeDetails')));
    await command('Input.insertText', {text:'界'});
    await until(`globalThis.__kryonAccepted === 'Aé界'`);
    await evaluate('globalThis.__kryonTestReorder = true');
    await delay(150);
    if (!await evaluate(`globalThis.__editor === document.querySelector('[aria-label="Editor"]') && document.activeElement === globalThis.__editor && globalThis.__editor.selectionStart === 4`)) {
      throw new Error('Reordering lost editor identity, focus or Unicode caret');
    }
    await evaluate(`document.querySelector('[aria-label="Run"]').click()`);
    await until(`document.querySelector('h1')?.textContent === 'Clicked'`);
    const accessibility = await command('Accessibility.getFullAXTree');
    if (!(accessibility.result?.nodes || []).some(node => node.role?.value === 'textbox' && node.name?.value === 'Editor')) {
      throw new Error('Native editor missing from browser accessibility tree');
    }
    await command('Emulation.setDeviceMetricsOverride', {width:400,height:600,deviceScaleFactor:1,mobile:false});
    await until(`document.querySelector('[aria-label="Editor"]').getBoundingClientRect().width === 120`);
    await evaluate('globalThis.__kryonTestDone = true');
    await until(`document.getElementById('result')?.textContent === 'PASS'`);
    if (!await evaluate(`!JSON.stringify(globalThis.__kryonDomSnapshot).includes('test secret')`)) throw new Error('Snapshot leaked secure editor text');
    console.log('Native DOM: stable identity/focus, incremental updates, Unicode input, KSS, responsive layout and browser accessibility PASS');
    process.exitCode = 0;
  } else {
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
        domNodes: document.getElementById('kryon-dom-root')?.querySelectorAll('[data-kryon-id]').length
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
  }
} finally {
  if (socket) socket.close();
  try { process.kill(-browser.pid, 'SIGTERM'); } catch {}
  await new Promise(resolve => {
    if (browser.exitCode !== null || browser.signalCode !== null) resolve();
    else browser.once('exit', resolve);
  });
}

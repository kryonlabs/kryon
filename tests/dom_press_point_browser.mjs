// Drive only the private headless process created here; no desktop connection.
// Real browser input on a full-page transparent Press must reach the app with
// the point it landed on, while keyboard clicks, an ordinary button, drags and
// replaced nodes keep their own meaning.
import {spawn} from 'node:child_process';
import {readFile, unlink} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';

const [html, profile] = process.argv.slice(2);
await unlink(join(profile, 'DevToolsActivePort')).catch(error => {
  if (error.code !== 'ENOENT') throw error;
});
const browser = spawn(process.env.CHROMIUM || 'chromium', [
  '--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage',
  '--no-first-run', '--no-default-browser-check', '--remote-debugging-port=0',
  '--user-data-dir=' + profile, 'about:blank'
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
    const response = await result;
    if (response.error) throw new Error(method + ': ' + JSON.stringify(response.error));
    return response;
  }
  async function evaluate(expression) {
    const response = await command('Runtime.evaluate', {expression, returnByValue: true});
    if (response.result?.exceptionDetails) throw new Error(JSON.stringify(response.result.exceptionDetails));
    return response.result?.result?.value;
  }
  async function until(expression, what) {
    const end = Date.now() + 8000;
    while (Date.now() < end) {
      if (await evaluate(expression)) return;
      await delay(30);
    }
    throw new Error('Timed out: ' + what + ' ' + JSON.stringify(await state()) + ' ' + diagnostics);
  }
  // A few frames pass, so a wrongly routed click would have arrived.
  const settle = () => delay(250);
  const state = () => evaluate(`({
    clicks: globalThis.__kryonPressClicks, replaced: globalThis.__kryonReplacedClicks,
    plain: globalThis.__kryonPlainClicks, x: globalThis.__kryonPressX, y: globalThis.__kryonPressY,
    released: globalThis.__kryonPressReleased,
    trace: (globalThis.__trace || []).slice(-8)
  })`);
  await command('Runtime.enable');
  await command('Page.enable');
  await command('Emulation.setDeviceMetricsOverride', {width: 800, height: 600, deviceScaleFactor: 1, mobile: false});
  await command('Page.navigate', {url: html});
  await until(`globalThis.__kryonPressReady && document.querySelector('[aria-label="Turn the page"]') && document.querySelector('[aria-label="Plain"]')`, 'page ready');
  const shape = await evaluate(`(() => {
    globalThis.__trace = [];
    for (const type of ['pointerdown', 'mousedown', 'pointerup', 'mouseup', 'click']) {
      addEventListener(type, event => globalThis.__trace.push({type, detail: event.detail,
        buttons: event.buttons, pointer: event.pointerType ?? null, x: event.clientX, y: event.clientY,
        target: event.target.closest?.('[aria-label]')?.getAttribute('aria-label') ?? null}), true);
    }
    const press = document.querySelector('[aria-label="Turn the page"]');
    const style = getComputedStyle(press);
    return {tag: press.tagName, background: style.backgroundColor, border: style.borderTopWidth};
  })()`);
  if (shape.tag !== 'BUTTON' || shape.background !== 'rgba(0, 0, 0, 0)') {
    throw new Error('Press is not a transparent native button: ' + JSON.stringify(shape));
  }
  async function rect(label) {
    return evaluate(`(() => {
      const box = document.querySelector('[aria-label="${label}"]').getBoundingClientRect();
      return {left: box.left, top: box.top, width: box.width, height: box.height};
    })()`);
  }
  // Where the app should see a page point: its place on the element, mapped
  // onto the press's own bounds.
  async function expected(point) {
    const box = await rect('Turn the page');
    const bounds = await evaluate(`[__kryonPressLeft, __kryonPressTop, __kryonPressWidth, __kryonPressHeight]`);
    return {x: bounds[0] + (point.x - box.left) / box.width * bounds[2],
      y: bounds[1] + (point.y - box.top) / box.height * bounds[3], bounds};
  }
  async function at(label, across, down) {
    const box = await rect(label);
    return {x: Math.round(box.left + box.width * across), y: Math.round(box.top + box.height * down)};
  }
  async function mouse(from, to = from, buttonState = true) {
    const states = buttonState ? [{buttons: 1}, {buttons: 0}] : [{}, {}];
    await command('Input.dispatchMouseEvent', {type: 'mouseMoved', x: from.x, y: from.y});
    await command('Input.dispatchMouseEvent', {type: 'mousePressed', x: from.x, y: from.y,
      button: 'left', clickCount: 1, ...states[0]});
    await delay(70);
    if (to !== from) {
      for (let step = 1; step <= 4; step++) {
        await command('Input.dispatchMouseEvent', {type: 'mouseMoved', button: 'left', buttons: 1,
          x: from.x + (to.x - from.x) * step / 4, y: from.y + (to.y - from.y) * step / 4});
      }
    }
    await command('Input.dispatchMouseEvent', {type: 'mouseReleased', x: to.x, y: to.y,
      button: 'left', clickCount: 1, ...states[1]});
  }
  const results = {};
  // A click on the press reaches it once, at the point it landed. Mouse
  // clicks arrive as the button's native click; touch may instead arrive
  // through the page's own touch input, which cancels the browser's click.
  async function pointed(name, point, input, counter = '__kryonPressClicks', touch = false) {
    const before = await evaluate(`globalThis.${counter}`);
    await evaluate('globalThis.__trace = []');
    await input();
    await until(`globalThis.${counter} === ${before + 1}`, name + ' click');
    await settle();
    const want = await expected(point);
    const got = await state();
    const clicks = got.trace.filter(entry => entry.type === 'click');
    results[name] = {x: got.x, y: got.y, want: {x: want.x, y: want.y},
      path: clicks.length ? 'click' : 'touch input', click: clicks.at(-1) ?? null};
    if (await evaluate(`globalThis.${counter}`) !== before + 1) throw new Error(name + ' clicked twice: ' + JSON.stringify(got));
    if (clicks.length > 1 || (!touch && clicks.length !== 1) || clicks.some(click => click.detail < 1)) {
      throw new Error(name + ' native click: ' + JSON.stringify(got.trace));
    }
    if (Math.abs(got.x - want.x) > 1 || Math.abs(got.y - want.y) > 1) {
      throw new Error(name + ' point: ' + JSON.stringify(results[name]));
    }
    return {got, want};
  }
  const right = await at('Turn the page', 0.85, 0.6);
  const left = await at('Turn the page', 0.1, 0.3);
  let hit = await pointed('mouse right', right, () => mouse(right));
  if (hit.got.x < hit.want.bounds[0] + hit.want.bounds[2] * 2 / 3) throw new Error('right click not in the right third');
  hit = await pointed('mouse left', left, () => mouse(left));
  if (hit.got.x >= hit.want.bounds[0] + hit.want.bounds[2] / 3) throw new Error('left click not in the left third');
  // Press and release events that leave out the button state still click
  // where they landed.
  await pointed('mouse without button state', right, () => mouse(right, right, false));
  await pointed('touch', left, async () => {
    await command('Input.dispatchTouchEvent', {type: 'touchStart', touchPoints: [{x: left.x, y: left.y}]});
    await delay(40);
    await command('Input.dispatchTouchEvent', {type: 'touchEnd', touchPoints: []});
  }, '__kryonPressClicks', true);
  await pointed('touch tap gesture', right, () => command('Input.synthesizeTapGesture',
    {x: right.x, y: right.y, duration: 40, tapCount: 1, gestureSourceType: 'touch'}), '__kryonPressClicks', true);
  // A drag released on the press clicks where it was released, as a native
  // pointer released on the same control does.
  const end = await at('Turn the page', 0.15, 0.5);
  await pointed('drag released on the press', end, () => mouse(right, end));
  // Enter on the focused press clicks it with no pointer point.
  const keyed = await evaluate('globalThis.__kryonPressClicks');
  await evaluate(`document.querySelector('[aria-label="Turn the page"]').focus()`);
  await command('Input.dispatchKeyEvent', {type: 'keyDown', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13, text: '\r'});
  await command('Input.dispatchKeyEvent', {type: 'keyUp', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13});
  await until(`globalThis.__kryonPressClicks === ${keyed + 1}`, 'keyboard click');
  results.keyboard = await state();
  if (results.keyboard.x !== 0 || results.keyboard.y !== 0 || results.keyboard.released) {
    throw new Error('keyboard click kept a pointer point: ' + JSON.stringify(results.keyboard));
  }
  // The ordinary button clicks and the press does not.
  const plain = await at('Plain', 0.5, 0.5);
  const press_before = await evaluate('globalThis.__kryonPressClicks');
  const plain_before = await evaluate('globalThis.__kryonPlainClicks');
  await mouse(plain);
  await until(`globalThis.__kryonPlainClicks === ${plain_before + 1}`, 'plain button click');
  await settle();
  if (await evaluate('globalThis.__kryonPressClicks') !== press_before) throw new Error('plain click also clicked the press');
  // A drag from the press onto the button clicks neither.
  await mouse(right, plain);
  await settle();
  if (await evaluate(`__kryonPressClicks !== ${press_before} || __kryonPlainClicks !== ${plain_before + 1}`)) {
    throw new Error('drag across controls clicked: ' + JSON.stringify(await state()));
  }
  // The click's own frame replaces the press: the new node never receives
  // the click resolved for the old element.
  const old_identity = await evaluate(`(() => {
    const press = document.querySelector('[aria-label="Turn the page"]');
    press.addEventListener('click', () => { globalThis.__kryonReplacePress = true; }, {once: true});
    return press.getAttribute('data-kryon-id');
  })()`);
  await mouse(right);
  await until(`document.querySelector('[aria-label="Turn the page"]')?.getAttribute('data-kryon-id') !== ${JSON.stringify(old_identity)}`, 'replaced press');
  await settle();
  if (await evaluate(`__kryonReplacedClicks !== 0 || __kryonPressClicks !== ${press_before}`)) {
    throw new Error('replaced press received the old click: ' + JSON.stringify(await state()));
  }
  // The replacement clicks normally once it is the element clicked.
  await pointed('replaced press', left, () => mouse(left), '__kryonReplacedClicks');
  await evaluate('globalThis.__kryonTestDone = true');
  console.log('DOM press point: ' + JSON.stringify(results));
  console.log('Native DOM press: right/left mouse, touch, drag, keyboard, plain button and replaced identity PASS');
  process.exitCode = 0;
} finally {
  if (socket) socket.close();
  try { process.kill(-browser.pid, 'SIGTERM'); } catch {}
  await new Promise(resolve => {
    if (browser.exitCode !== null || browser.signalCode !== null) resolve();
    else browser.once('exit', resolve);
  });
}

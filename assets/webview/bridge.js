// Kryon island protocol v1. No eval, no native methods exposed to content.
// A staged page installs window.KryonPage (receive/state) and calls
// KryonIsland.ready() once its handlers exist; commands sent earlier wait in
// the island until then. Focus supplies the shared visibility lifecycle.
const send = value => window.webkit.messageHandlers.kryon.postMessage(JSON.stringify({ v: 1, ...value }));
let active = true;
const browserHidden = Object.getOwnPropertyDescriptor(Document.prototype, 'hidden')?.get;
Object.defineProperty(document, 'hidden', { configurable: true, get: () => !active || !!browserHidden?.call(document) });
function lifecycle(value) {
  active = value;
  document.dispatchEvent(new Event('visibilitychange'));
  const scene = window.KryonScene;
  if (scene) { cancelAnimationFrame(scene.frame); scene.frame = 0; if (active && scene.draw) scene.frame = requestAnimationFrame(scene.draw); }
}
async function receive(command) {
  try {
    if (!command || command.v !== 1) throw Error('Unsupported island protocol version');
    if (command.type === 'focus') {
      if (typeof command.active !== 'boolean') throw Error('Invalid focus state');
      lifecycle(command.active);
    }
    const page = window.KryonPage;
    if (page?.receive) await page.receive(command);
    send({ type: 'ack', id: command.id, command: command.type,
      state: page?.state ? page.state(active) : { active, viewport: { width: innerWidth, height: innerHeight } } });
  } catch (error) { send({ type: 'error', id: command?.id, message: error.message }); }
}
window.KryonIsland = {
  receive,
  ready(state) { send({ type: 'ready', state: state || {} }); },
  fail(message) { send({ type: 'error', message: String(message) }); }
};
window.addEventListener('error', event => send({ type: 'error', message: event.message }));
window.addEventListener('unhandledrejection', event => send({ type: 'error', message: String(event.reason) }));

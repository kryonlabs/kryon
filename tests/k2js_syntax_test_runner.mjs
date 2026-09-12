import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [generatedPath, runtimePath] = process.argv.slice(2);
const generated = await import(pathToFileURL(generatedPath).href);
const runtime = await import(pathToFileURL(runtimePath).href);
function rectangle(value) {
  const [x, y, width, height] = Array.isArray(value)
    ? value : [value.x, value.y, value.width, value.height];
  return { x, y, width, height };
}

assert.equal(generated.app.title, "JS Smoke");
assert.equal(generated.app.width, 320);
assert.equal(generated.app.height, 240);
assert.deepEqual(generated.app.styles, [
  { kind: "builtin", target: "kryon.vanilla", alias: "vanilla" },
  { kind: "file", target: "brand.kss", alias: "brand" }
]);

const state = generated.createState();
assert.equal(state.count, 0);
assert.equal(typeof generated.setHost, "function");
assert.equal(typeof generated.frame, "function");

const host = {
  HostValue(value) {
    return value + 41;
  }
};
generated.setHost(host);

const rt = runtime.createRuntime({ app: generated.app });
const webStyleSheet = runtime.parseWebStyleSheet(`
  @pack smoke;
  @layer components;
  Button.primary {
    background: #102030;
    foreground: #f0f0f0;
    radius: 9;
    padding-x: 13;
  }
  Button#tap-button {
    background: #203040;
  }
  TextField.field {
    border-width: 2;
    padding-y: 5;
  }
`);
assert.equal(webStyleSheet.pack, "smoke");
runtime.setWebStyleSheets(rt, webStyleSheet);
assert.equal(generated.Valid_ApplyPreviewMode(rt, state, host, 1), 2);
assert.equal(runtime.GetTheme().mode, 1);
assert.equal(generated.Valid_ApplyPreviewMode(rt, state, host, 2), 3);
assert.equal(runtime.GetTheme().mode, 2);
assert.equal(generated.Valid_FractionalPreviewMode(rt, state, host, 1.75), 1);
assert.equal(generated.Valid_FractionalPreviewMode(rt, state, host, -1.75), -1);
const snap = generated.frame(rt, state, host);
assert.equal(state.count, 1);
assert.equal(state.viewport_width, 320);
assert.equal(state.viewport_height, 240);
assert.equal(snap.frame.length, 4);
assert.equal(snap.frame[0].name, "Screen");
assert.equal(snap.frame[1].name, "Text");
assert.equal(snap.frame[2].name, "Button");
assert.equal(typeof snap.frame[2].args, "object");
assert.deepEqual(rectangle(snap.frame[2].args.bounds), { x: 10, y: 50, width: 120, height: 28 });
assert.equal(snap.frame[2].args.label, "Tap");
assert.equal(snap.frame[2].args.style.normal.radius, 6);
assert.equal(snap.frame[2].args.style.normal.fields, 16);
const webDoc = runtime.webDocumentFrame(rt);
assert.deepEqual(webDoc.nodes.map((node) => [node.kind, node.tag]), [
  ["Screen", "main"],
  ["Text", "div"],
  ["Button", "button"],
  ["TextField", "input"]
]);
assert.equal(webDoc.nodes[2].text, "Tap");
assert.deepEqual(webDoc.nodes[2].bounds, { x: 10, y: 50, width: 120, height: 28 });
assert.equal(webDoc.nodes[2].key, "tap");
assert.equal(webDoc.nodes[2].name, "tap");
assert.equal(webDoc.nodes[2].path, "Scene/root/tap");
assert.equal(webDoc.nodes[2].parentPath, "Scene/root");
assert.equal(webDoc.nodes[2].domId, "tap-button");
assert.deepEqual(webDoc.nodes[2].classes, ["primary", "action"]);
assert.equal(webDoc.nodes[2].role, "button");
assert.equal(webDoc.nodes[2].ariaLabel, "Tap the action");
assert.equal(webDoc.nodes[2].onClick, "call_host");
assert.deepEqual(webDoc.nodes[2].styleFacts, {
  kind: "Button",
  tag: "button",
  key: "tap",
  name: "tap",
  path: "Scene/root/tap",
  parentPath: "Scene/root",
  id: "tap-button",
  classes: ["primary", "action"],
  role: "button",
  state: {
    disabled: false,
    loading: false,
    selected: false,
    checked: false,
    invalid: false,
    expanded: false,
    open: false
  }
});
assert.deepEqual(runtime.webNodeStyleFacts(webDoc.nodes[2]), webDoc.nodes[2].styleFacts);
assert.deepEqual(runtime.resolveWebStyle(webDoc.nodes[2], webStyleSheet), {
  background: "#203040",
  foreground: "#f0f0f0",
  radius: 9,
  "padding-x": 13
});
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], runtime.parseWebStyleSheet(`
  @layer components;
  Button#tap-button {
    background: #203040;
  }
  @layer app;
  Button.primary {
    background: #405060;
  }
`)).background, "#405060");
assert.equal(webDoc.nodes[2].action(), 42);
assert.equal(webDoc.nodes[3].key, "search");
assert.equal(webDoc.nodes[3].tag, "input");
assert.equal(webDoc.nodes[3].domId, "search-field");
assert.deepEqual(webDoc.nodes[3].classes, ["field"]);
assert.equal(webDoc.nodes[3].ariaLabel, "Search");
assert.equal(webDoc.nodes[3].onInput, "note_input");
assert.equal(webDoc.nodes[3].onChange, "note_change");
assert.equal(webDoc.nodes[3].value, "label");
assert.equal(runtime.webAccessibilitySnapshot(webDoc).nodes[2].role, "button");
assert.equal(runtime.webAccessibilitySnapshot(webDoc).nodes[3].role, "textbox");
assert.equal(runtime.webAccessibilitySnapshot(webDoc).nodes[3].label, "Search");
assert.equal(generated.Valid_CallHost(rt, state, host), 42);

function fakeDocument() {
  const makeElement = (tag) => {
    const element = {
      tagName: tag.toUpperCase(),
      children: [],
      parentNode: null,
      dataset: {},
      style: {},
      attributes: {},
      className: "",
      textContent: "",
      checked: false,
      value: "",
      setAttribute(name, value) {
        this.attributes[name] = String(value);
        if (name === "id")
          this.id = String(value);
        if (name === "type")
          this.type = String(value);
      },
      removeAttribute(name) {
        delete this.attributes[name];
        if (name === "id")
          delete this.id;
        if (name === "type")
          delete this.type;
      },
      appendChild(child) {
        if (child.parentNode)
          child.parentNode.removeChild(child);
        child.parentNode = this;
        this.children.push(child);
      },
      removeChild(child) {
        const index = this.children.indexOf(child);
        if (index >= 0)
          this.children.splice(index, 1);
        child.parentNode = null;
      },
      addEventListener(type, fn) { this["on" + type] = fn; },
      click() { if (this.onclick) this.onclick(); },
      input(value) { this.value = value; if (this.oninput) this.oninput(); },
      change(value) { this.value = value; if (this.onchange) this.onchange(); }
    };
    return element;
  };
  const head = makeElement("head");
  return {
    title: "",
    head,
    createElement: makeElement,
    querySelector(selector) {
      if (selector === 'meta[name="description"]')
        return head.children.find((child) => child.tagName === "META" && child.attributes.name === "description") || null;
      if (selector === 'meta[name="theme-color"]')
        return head.children.find((child) => child.tagName === "META" && child.attributes.name === "theme-color") || null;
      if (selector === 'link[rel="canonical"]')
        return head.children.find((child) => child.tagName === "LINK" && child.attributes.rel === "canonical") || null;
      return null;
    }
  };
}

{
  const previousDocument = globalThis.document;
  globalThis.document = fakeDocument();
  try {
    const ariaRt = runtime.createRuntime();
    runtime.beginFrame(ariaRt);
    runtime.widget(ariaRt, "Heading", { level: 2, text: "Welcome" }, null,
      { nodeName: "welcome", path: "Page/welcome" });
    runtime.widget(ariaRt, "Link", { href: "/docs", text: "Docs", selected: true }, null,
      { nodeName: "docs", path: "Page/docs" });
    runtime.widget(ariaRt, "Checkbox", { checked: true, disabled: true, loading: true }, null,
      { nodeName: "accept", path: "Page/accept" });
    runtime.endFrame(ariaRt);
    const snapshot = runtime.webAccessibilitySnapshot(ariaRt);
    assert.equal(snapshot.nodes[0].role, "heading");
    assert.equal(snapshot.nodes[0].level, 2);
    assert.equal(snapshot.nodes[1].role, "link");
    assert.equal(snapshot.nodes[1].href, "/docs");
    assert.equal(snapshot.nodes[2].role, "checkbox");
    assert.equal(snapshot.nodes[2].inputType, "checkbox");
    assert.equal(snapshot.nodes[2].state.checked, true);
    const target = document.createElement("div");
    runtime.renderWebDocument(ariaRt, target);
    const root = target.children[0];
    const link = runtime.findWebElement(target, "Page/docs");
    const checkbox = runtime.findWebElement(target, "Page/accept");
    assert.equal(link.attributes["aria-current"], "page");
    assert.equal(checkbox.attributes["aria-checked"], "true");
    assert.equal(checkbox.attributes["aria-disabled"], "true");
    assert.equal(checkbox.attributes["aria-busy"], "true");
    assert.equal(runtime.webFormValue(target, "accept"), true);
    assert.equal(root.children.length, 3);
  } finally {
    globalThis.document = previousDocument;
  }
}

{
  const previousDocument = globalThis.document;
  globalThis.document = fakeDocument();
  try {
    const domState = generated.createState();
    const domRt = runtime.createRuntime({ app: generated.app });
    runtime.setWebStyleSheets(domRt, webStyleSheet);
    generated.frame(domRt, domState, host);
    const target = document.createElement("div");
    runtime.SetPageTitle("Runtime title");
    runtime.SetPageDescription("Runtime description");
    runtime.SetPageCanonicalURL("https://example.test/page");
    runtime.SetPageThemeColor(runtime.Color(1, 2, 3, 255));
    runtime.renderWebDocument(domRt, target);
    assert.equal(document.title, "Runtime title");
    assert.equal(document.querySelector('meta[name="description"]').attributes.content, "Runtime description");
    assert.equal(document.querySelector('link[rel="canonical"]').attributes.href, "https://example.test/page");
    assert.equal(document.querySelector('meta[name="theme-color"]').attributes.content, "rgb(1, 2, 3)");
    const root = target.children[0];
    const screen = root.children.find((child) => child.tagName === "MAIN");
    const firstButton = screen.children[0];
    assert.equal(firstButton.tagName, "BUTTON");
    assert.equal(firstButton.id, "tap-button");
    assert.equal(firstButton.dataset.kryName, "tap");
    assert.equal(firstButton.attributes.role, "button");
    assert.equal(firstButton.attributes["aria-label"], "Tap the action");
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(firstButton.style.color, "#f0f0f0");
    assert.equal(firstButton.style.borderRadius, "9px");
    assert.equal(firstButton.style.paddingLeft, "13px");
    assert.equal(runtime.findWebNode(domRt, "Scene/root/tap").domId, "tap-button");
    assert.equal(runtime.findWebElement(target, "Scene/root/tap"), firstButton);
    assert.equal(runtime.findWebElement(target, "tap"), firstButton);
    assert.equal(runtime.findWebElement(target, "tap-button"), firstButton);
    const firstField = screen.children[1];
    assert.equal(firstField.tagName, "INPUT");
    assert.equal(firstField.id, "search-field");
    assert.equal(firstField.dataset.kryOnInput, "note_input");
    assert.equal(firstField.dataset.kryOnChange, "note_change");
    assert.equal(firstField.style.borderWidth, "2px");
    assert.equal(firstField.style.paddingTop, "5px");
    assert.equal(runtime.webFormValue(target, "Scene/root/search"), "label");
    assert.equal(runtime.webFormValues(target)["search-field"], "label");
    firstField.input("needle");
    assert.equal(runtime.webFormValue(target, "Scene/root/search"), "needle");
    assert.equal(runtime.webFormValue(target, "search"), "needle");
    assert.equal(runtime.webFormValues(target)["search-field"], "needle");
    assert.equal(domState.count, 11);
    firstField.change("needle");
    assert.equal(runtime.webFormValue(target, "search-field"), "needle");
    assert.equal(domState.count, 111);
    generated.frame(domRt, domState, host);
    runtime.renderWebDocument(domRt, target);
    assert.equal(target.children[0], root);
    assert.equal(root.children.find((child) => child.tagName === "MAIN"), screen);
    assert.equal(screen.children[0], firstButton);
    assert.equal(screen.children[1], firstField);
    runtime.setWebStyleSheets(domRt, []);
    runtime.renderWebDocument(domRt, target);
    assert.equal(firstButton.style.background, "");
    assert.equal(firstButton.style.color, "");
    runtime.setWebStyleSheets(domRt, webStyleSheet);
    runtime.renderWebDocument(domRt, target);
    assert.equal(firstButton.style.background, "#203040");
    firstButton.click();
    assert.equal(domRt.input.events.at(-1).type, "tap");
    const previousEventCount = domRt.input.events.length;
    const nextRt = runtime.createRuntime({ app: generated.app });
    const nextState = generated.createState();
    generated.frame(nextRt, nextState, host);
    runtime.renderWebDocument(nextRt, target);
    assert.equal(screen.children[0], firstButton);
    firstButton.click();
    assert.equal(domRt.input.events.length, previousEventCount);
    assert.equal(nextRt.input.events.at(-1).type, "tap");
  } finally {
    globalThis.document = previousDocument;
  }
}

{
  const startVersion = runtime.GetRouteVersion();
  assert.equal(runtime.ReplaceRoute("/docs#intro"), "/docs#intro");
  assert.equal(runtime.GetRoutePath(), "/docs");
  assert.equal(runtime.GetRouteHash(), "#intro");
  assert.equal(runtime.GetRouteVersion(), startVersion + 1);
  assert.equal(runtime.ReplaceRoute("/docs#intro"), "/docs#intro");
  assert.equal(runtime.GetRouteVersion(), startVersion + 1);
  assert.equal(runtime.PushRoute("/docs/api"), "/docs/api");
  assert.equal(runtime.GetRoutePath(), "/docs/api");
  assert.equal(runtime.GetRouteHash(), "");
  assert.equal(runtime.GetRouteVersion(), startVersion + 2);
}

{
  const previousLocation = globalThis.location;
  const previousHistory = globalThis.history;
  const previousAddEventListener = globalThis.addEventListener;
  const listeners = new Map();
  globalThis.location = { pathname: "/browser", hash: "#one" };
  globalThis.history = {
    pushState(_state, _title, url) {
      const [path, hash = ""] = String(url).split("#");
      globalThis.location.pathname = path || "/";
      globalThis.location.hash = hash ? "#" + hash : "";
    },
    replaceState(_state, _title, url) {
      this.pushState(_state, _title, url);
    }
  };
  globalThis.addEventListener = (type, fn) => listeners.set(type, fn);
  try {
    runtime.createRuntime();
    assert.equal(runtime.GetRoutePath(), "/browser");
    assert.equal(runtime.GetRouteHash(), "#one");
    runtime.PushRoute("/browser/two#part");
    assert.equal(globalThis.location.pathname, "/browser/two");
    assert.equal(globalThis.location.hash, "#part");
    assert.equal(runtime.GetRoutePath(), "/browser/two");
    assert.equal(runtime.GetRouteHash(), "#part");
    globalThis.location.pathname = "/browser/back";
    globalThis.location.hash = "";
    listeners.get("popstate")?.();
    assert.equal(runtime.GetRoutePath(), "/browser/back");
    assert.equal(runtime.GetRouteHash(), "");
  } finally {
    if (previousLocation === undefined)
      delete globalThis.location;
    else
      globalThis.location = previousLocation;
    if (previousHistory === undefined)
      delete globalThis.history;
    else
      globalThis.history = previousHistory;
    if (previousAddEventListener === undefined)
      delete globalThis.addEventListener;
    else
      globalThis.addEventListener = previousAddEventListener;
  }
}

rt.target = { clientWidth: 640, clientHeight: 480 };
generated.frame(rt, state, host);
assert.equal(state.count, 2);
assert.equal(state.viewport_width, 640);
assert.equal(state.viewport_height, 480);
rt.target.clientWidth = 0;
rt.target.clientHeight = 0;
generated.frame(rt, state, host);
assert.equal(state.viewport_width, 320);
assert.equal(state.viewport_height, 240);

const defaultSnapshot = generated.frame();
assert.equal(generated.moduleState.viewport_width, 320);
assert.equal(generated.moduleState.viewport_height, 240);
assert.equal(defaultSnapshot.frame.length, 4);

const mounted = generated.main(null, host);
assert.equal(mounted.mounted, false);

const styleRuntime = runtime.createRuntime();
runtime.beginFrame(styleRuntime);
generated.Valid_StyleCopies(styleRuntime, state, host);
const styleFrame = runtime.endFrame(styleRuntime).frame;
assert.deepEqual(rectangle(styleFrame[0].args.bounds), { x: 10, y: 20, width: 50, height: 40 });
const styles = styleFrame.map(item => item.args.style.normal);
assert.deepEqual(styles.map(style => style.font_size), [19, 24, 32]);
assert.deepEqual(styles.map(({ content_offset: { x, y } }) => ({ x, y })),
  [{ x: 0, y: 1 }, { x: 0, y: 1 }, { x: 0, y: -1 }]);

for (const action of [generated.Valid_DirectAction, generated.Valid_StoredAction, generated.Valid_AssignedAction]) {
  const actionRuntime = runtime.createRuntime({ app: generated.app });
  for (const tapped of [false, true, false]) {
    if (tapped) actionRuntime.QueueTap(30, 110);
    runtime.beginFrame(actionRuntime);
    assert.equal(action(actionRuntime, state, host, 20), tapped);
    const result = runtime.endFrame(actionRuntime);
    assert.equal(result.frame.length, 1);
    assert.equal(result.frame[0].name, "Button");
    assert.deepEqual(rectangle(result.frame[0].args.bounds), { x: 20, y: 100, width: 80, height: 32 });
  }
}

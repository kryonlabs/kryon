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
assert.equal(generated.app.styles.length, 2);
assert.deepEqual(generated.app.styles.map(({ kind, target, alias }) => ({ kind, target, alias })), [
  { kind: "builtin", target: "kryon.material", alias: "material" },
  { kind: "file", target: "brand.kss", alias: "brand" }
]);
assert.match(generated.app.styles[0].source, /@pack kryon\.material;/);
assert.match(generated.app.styles[1].source, /@pack brand;/);

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
  tokens {
    color { button-face: #102030; button-ink: #f0f0f0; id-face: #203040; }
    length { radius.md: 9; space.3: 13; field-y: 5; line: 2; }
  }
  @layer components;
  Button.primary {
    background: button-face;
    foreground: button-ink;
    radius: radius.md;
    padding-x: space.3;
  }
  Button#tap-button {
    background: id-face;
  }
  Button#tap-button:hover {
    background: #304050;
  }
  Button#tap-button:pressed {
    background: #405060;
  }
  Button#tap-button:focus {
    border: #506070;
  }
  Button[data-tracking-id="tap-1"] {
    opacity: 0.75;
  }
  Button[aria-current=page] {
    offset-y: 8;
  }
  Button.runtime-selected {
    border-width: 6;
  }
  Button[data-runtime="1"] {
    padding-y: 9;
  }
  Button:disabled {
    opacity: 0.25;
  }
  TextField.field {
    border-width: line;
    padding-y: field-y;
  }
  TextField[data.role="search"] {
    opacity: 0.9;
  }
  TextField[required=true] {
    gap: 3;
  }
  TextField[required] {
    font-size: 11;
  }
  TextField[maxlength=64] {
    offset-x: 4;
  }
  TextField[spellcheck=false] {
    offset-y: 6;
  }
  TextField[formnovalidate] {
    content-offset-y: 7;
  }
  TextField[scrolltop=22] {
    font-size: 18;
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
assert.equal(snap.frame.length, 5);
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
  ["TextField", "input"],
  ["Text", "label"]
]);
assert.equal(webDoc.nodes[2].text, "Tap");
assert.deepEqual(webDoc.nodes[2].bounds, { x: 10, y: 50, width: 120, height: 28 });
assert.equal(webDoc.nodes[2].key, "tap");
assert.equal(webDoc.nodes[2].name, "tap");
assert.equal(webDoc.nodes[2].path, "Scene/root/tap");
assert.equal(webDoc.nodes[2].parentPath, "Scene/root");
assert.equal(webDoc.nodes[2].webRef, "primary-action");
assert.equal(webDoc.nodes[2].sourcePath, "src/valid.kry");
assert.ok(webDoc.nodes[2].sourceLine > 0);
assert.ok(webDoc.nodes[2].sourceColumn > 0);
const tapSourceRef = `${webDoc.nodes[2].sourcePath}:${webDoc.nodes[2].sourceLine}`;
const tapSourceColumnRef = `${tapSourceRef}:${webDoc.nodes[2].sourceColumn}`;
assert.equal(runtime.webSourceRef("src/valid.kry", webDoc.nodes[2].sourceLine), tapSourceRef);
assert.equal(runtime.webSourceRef("src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn), tapSourceColumnRef);
assert.equal(runtime.findWebNode(rt, tapSourceRef).path, webDoc.nodes[2].path);
assert.equal(runtime.findWebNode(rt, tapSourceColumnRef).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeAtSource(rt, "src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn).path, webDoc.nodes[2].path);
assert.deepEqual(runtime.webNodesAtSource(rt, "src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn).map((node) => node.path), [webDoc.nodes[2].path]);
assert.equal(runtime.findWebNode(rt, "primary-action").path, webDoc.nodes[2].path);
assert.equal(webDoc.nodes[2].domId, "tap-button");
assert.equal(webDoc.nodes[2].domValue, "tap-value");
assert.deepEqual(webDoc.nodes[2].dataAttrs, { "tracking-id": "tap-1" });
assert.deepEqual(webDoc.nodes[2].classes, ["primary", "action"]);
assert.equal(webDoc.nodes[2].title, "Tap details");
assert.equal(webDoc.nodes[2].tabIndex, 3);
assert.equal(webDoc.nodes[2].role, "button");
assert.equal(webDoc.nodes[2].ariaLabel, "Tap the action");
assert.equal(webDoc.nodes[2].ariaDescription, "Runs the host action");
assert.equal(webDoc.nodes[2].ariaControls, "search-field");
assert.equal(webDoc.nodes[2].popoverTarget, "search-menu");
assert.equal(webDoc.nodes[2].popoverTargetAction, "toggle");
assert.deepEqual(webDoc.nodes[2].extraAttrs, {
  fetchpriority: "high",
  part: "primary-action"
});
assert.equal(webDoc.nodes[2].onClick, "call_host");
assert.deepEqual(webDoc.nodes[2].styleFacts, {
  index: 2,
  kind: "Button",
  tag: "button",
  key: "tap",
  name: "tap",
  path: "Scene/root/tap",
  parentPath: "Scene/root",
  ref: "primary-action",
  webRef: "primary-action",
  sourcePath: webDoc.nodes[2].sourcePath,
  sourceLine: webDoc.nodes[2].sourceLine,
  sourceColumn: webDoc.nodes[2].sourceColumn,
  sourceRef: tapSourceRef,
  sourceColumnRef: tapSourceColumnRef,
  id: "tap-button",
  domName: "",
  domValue: "tap-value",
  href: "",
  target: "",
  rel: "",
  htmlFor: "",
  inputType: "",
  formAction: "",
  formMethod: "",
  formEncType: "",
  autoComplete: "",
  hidden: false,
  draggable: "",
  spellCheck: "",
  contentEditable: "",
  autoFocus: false,
  download: "",
  formNoValidate: false,
  noValidate: false,
  popover: "",
  popoverTarget: "search-menu",
  popoverTargetAction: "toggle",
  open: false,
  scrollLeft: 0,
  scrollTop: 0,
  readOnly: false,
  required: false,
  min: "",
  max: "",
  step: "",
  minLength: "",
  maxLength: "",
  pattern: "",
  accept: "",
  multiple: false,
  inputMode: "",
  classes: ["primary", "action"],
  dataAttrs: { "tracking-id": "tap-1" },
  ariaAttrs: { current: "page", pressed: "false" },
  extraAttrs: { fetchpriority: "high", part: "primary-action" },
  role: "button",
  state: {
    disabled: false,
    loading: false,
    selected: false,
    checked: false,
    invalid: false,
    expanded: false,
    open: false,
    hover: false,
    pressed: false,
    focus: false
  }
});
assert.deepEqual(runtime.webNodeStyleFacts(webDoc.nodes[2]), webDoc.nodes[2].styleFacts);
assert.deepEqual(runtime.webNodeIdentity(webDoc.nodes[2]), {
  ref: "primary-action",
  aliases: [
    "primary-action",
    "Scene/root/tap",
    "tap",
    "tap-button",
    tapSourceRef,
    tapSourceColumnRef
  ],
  index: 2,
  kind: "Button",
  tag: "button",
  key: "tap",
  name: "tap",
  path: "Scene/root/tap",
  parentPath: "Scene/root",
  webRef: "primary-action",
  domId: "tap-button",
  domName: "",
  sourcePath: webDoc.nodes[2].sourcePath,
  sourceLine: webDoc.nodes[2].sourceLine,
  sourceColumn: webDoc.nodes[2].sourceColumn,
  sourceRef: tapSourceRef,
  sourceColumnRef: tapSourceColumnRef
});
assert.deepEqual(runtime.resolveWebStyle(webDoc.nodes[2], webStyleSheet), {
  background: "#203040",
  foreground: "#f0f0f0",
  radius: 9,
  "padding-x": 13,
  "offset-y": 8,
  opacity: 0.75
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
assert.equal(runtime.webNodeQuery(rt, "Scene/root/tap").path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, "Button.primary").path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, "[data-tracking-id=\"tap-1\"]").path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, "[value=\"tap-value\"]").path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, "[domValue=\"tap-value\"]").path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, "[name=q]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[name]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[type=search]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[readonly=true]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[required=true]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[required]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[min=1]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[max=100]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[step=1]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[minlength=2]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[maxlength=64]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[pattern=\"needle.*\"]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[accept=\".txt\"]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[multiple=true]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[multiple]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[inputmode=search]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[data-role]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[for=\"search-field\"]").path, "Scene/root/search_label");
assert.equal(runtime.webNodeQuery(rt, "[htmlFor=\"search-field\"]").path, "Scene/root/search_label");
assert.equal(runtime.webNodeQuery(rt, "[popover=manual]").path, "Scene/root/search_label");
assert.equal(runtime.webNodeQuery(rt, "[popoverTarget=\"search-menu\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[fetchpriority=high]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[part=\"primary-action\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt,
  `[source="src/valid.kry"][line=${webDoc.nodes[2].sourceLine}][column=${webDoc.nodes[2].sourceColumn}]`).path,
  "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, `[sourceRef="${tapSourceRef}"]`).path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, `[sourceColumnRef="${tapSourceColumnRef}"]`).path,
  "Scene/root/tap");
assert.equal(runtime.webNodeMatches(rt, "primary-action", "Button.primary"), true);
assert.equal(runtime.webNodeQuery(rt, "[ref=\"primary-action\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[webRef=\"primary-action\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeMatches(rt, "Scene/root/tap", "Button.primary"), true);
assert.equal(runtime.webNodeMatches(rt, "Scene/root/tap", "TextField"), false);
assert.deepEqual(runtime.webNodeQueryAll(rt, "[data.role=search]").map((node) => node.path), [
  "Scene/root/search"
]);
assert.equal(runtime.webNodeParent(rt, "Scene/root/tap").path, "Scene/root");
assert.deepEqual(runtime.webNodeChildren(rt, "Scene/root").map((node) => node.path), [
  webDoc.nodes[1].path,
  "Scene/root/tap",
  "Scene/root/search",
  "Scene/root/search_label"
]);
assert.deepEqual(runtime.webNodeChildren(rt).map((node) => node.path), ["Scene/root"]);
assert.equal(runtime.webNodeClosest(rt, "Scene/root/tap", "Screen").path, "Scene/root");
assert.equal(webDoc.nodes[2].action(), 42);
assert.equal(webDoc.nodes[3].key, "search");
assert.equal(webDoc.nodes[3].tag, "input");
assert.equal(webDoc.nodes[3].domId, "search-field");
assert.equal(webDoc.nodes[3].webRef, "search-box");
assert.equal(webDoc.nodes[3].domName, "q");
assert.deepEqual(webDoc.nodes[3].dataAttrs, { role: "search" });
assert.equal(webDoc.nodes[3].inputType, "search");
assert.equal(webDoc.nodes[3].readOnly, true);
assert.equal(webDoc.nodes[3].required, true);
assert.equal(webDoc.nodes[3].min, "1");
assert.equal(webDoc.nodes[3].max, "100");
assert.equal(webDoc.nodes[3].step, "1");
assert.equal(webDoc.nodes[3].minLength, "2");
assert.equal(webDoc.nodes[3].maxLength, "64");
assert.equal(webDoc.nodes[3].pattern, "needle.*");
assert.equal(webDoc.nodes[3].accept, ".txt");
assert.equal(webDoc.nodes[3].multiple, true);
assert.equal(webDoc.nodes[3].inputMode, "search");
assert.equal(webDoc.nodes[3].draggable, "true");
assert.equal(webDoc.nodes[3].spellCheck, "false");
assert.equal(webDoc.nodes[3].contentEditable, "plaintext-only");
assert.equal(webDoc.nodes[3].autoFocus, true);
assert.equal(webDoc.nodes[3].formNoValidate, true);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).gap, 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-x"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-y"], 6);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["content-offset-y"], 7);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-size"], 11);
assert.deepEqual(webDoc.nodes[3].classes, ["field"]);
assert.equal(webDoc.nodes[3].placeholder, "Search terms");
assert.equal(webDoc.nodes[3].ariaLabel, "Search");
assert.equal(webDoc.nodes[3].ariaDescribedBy, "tap-button");
assert.equal(webDoc.nodes[3].onInput, "note_input");
assert.equal(webDoc.nodes[3].onBeforeInput, "note_before_input");
assert.equal(webDoc.nodes[3].onChange, "note_change");
assert.equal(webDoc.nodes[3].onSelect, "note_select");
assert.equal(webDoc.nodes[3].onKey, "note_key");
assert.equal(webDoc.nodes[3].onInvalid, "invalid_search");
assert.equal(webDoc.nodes[3].onScroll, "scroll_search");
assert.equal(webDoc.nodes[3].onSubmit, "submit_search");
assert.equal(webDoc.nodes[3].onFocus, "focus_search");
assert.equal(webDoc.nodes[3].onBlur, "blur_search");
assert.equal(webDoc.nodes[3].value, "label");
assert.equal(webDoc.nodes[4].onToggle, "toggle_search");
assert.equal(webDoc.nodes[4].onClose, "close_search");
assert.equal(webDoc.nodes[4].onCancel, "cancel_search");
assert.equal(runtime.webAccessibilitySnapshot(webDoc).nodes[2].role, "button");
assert.equal(runtime.webAccessibilitySnapshot(webDoc).nodes[2].description, "Runs the host action");
assert.equal(runtime.webAccessibilitySnapshot(webDoc).nodes[3].role, "textbox");
assert.equal(runtime.webAccessibilitySnapshot(webDoc).nodes[3].label, "Search");
assert.equal(generated.Valid_CallHost(rt, state, host), 42);

function fakeDocument() {
  const fakeDataTransfer = () => {
    const values = {};
    return {
      setData(type, value) { values[type] = String(value); },
      getData(type) { return values[type] || ""; }
    };
  };
  const makeElement = (tag) => {
    const element = {
      tagName: tag.toUpperCase(),
      children: [],
      parentNode: null,
      dataset: {},
      style: {},
      attributes: {},
      listeners: {},
      className: "",
      textContent: "",
      checked: false,
      open: false,
      formNoValidate: false,
      noValidate: false,
      value: "",
      clientWidth: 0,
      clientHeight: 0,
      scrollWidth: 0,
      scrollHeight: 0,
      getBoundingClientRect() {
        const left = Number.parseFloat(this.style.left || 0) || 0;
        const top = Number.parseFloat(this.style.top || 0) || 0;
        const width = Number.parseFloat(this.style.width || this.clientWidth || 0) || 0;
        const height = Number.parseFloat(this.style.height || this.clientHeight || 0) || 0;
        return { x: left, y: top, left, top, width, height, right: left + width, bottom: top + height };
      },
      setAttribute(name, value) {
        this.attributes[name] = String(value);
        if (name === "id")
          this.id = String(value);
        if (name === "type")
          this.type = String(value);
        if (name === "open")
          this.open = true;
      },
      removeAttribute(name) {
        delete this.attributes[name];
        if (name === "id")
          delete this.id;
        if (name === "type")
          delete this.type;
        if (name === "open")
          this.open = false;
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
      addEventListener(type, fn) {
        if (!this.listeners[type])
          this.listeners[type] = [];
        this.listeners[type].push(fn);
        this["on" + type] = fn;
      },
      removeEventListener(type, fn) {
        const listeners = this.listeners[type] || [];
        const index = listeners.indexOf(fn);
        if (index >= 0)
          listeners.splice(index, 1);
        this["on" + type] = listeners[listeners.length - 1] || null;
      },
      dispatchEvent(event) {
        if (!event)
          return false;
        if (!event.target) {
          try {
            event.target = this;
          } catch {
            Object.defineProperty(event, "target", { value: this, configurable: true });
          }
        }
        try {
          event.currentTarget = this;
        } catch {
          Object.defineProperty(event, "currentTarget", { value: this, configurable: true });
        }
        if (!event.preventDefault) {
          event.defaultPrevented = false;
          event.preventDefault = function() { this.defaultPrevented = true; };
        }
        for (const handler of [...(this.listeners[event.type] || [])])
          handler(event);
        if (event.bubbles !== false && this.parentNode?.dispatchEvent)
          this.parentNode.dispatchEvent(event);
        return !event.defaultPrevented;
      },
      click() { if (this.onclick) this.onclick(); },
      dragstart(dataTransfer) {
        const transfer = dataTransfer || fakeDataTransfer();
        if (this.ondragstart)
          this.ondragstart({ dataTransfer: transfer });
        return transfer;
      },
      dragend() { if (this.ondragend) this.ondragend(); },
      dragover() {
        if (this.ondragover)
          this.ondragover({ preventDefault() {} });
      },
      drop(dataTransfer) {
        if (this.ondrop)
          this.ondrop({ preventDefault() {}, dataTransfer });
      },
      copy(clipboardData) {
        const clipboard = clipboardData || fakeDataTransfer();
        if (this.oncopy)
          this.oncopy({ clipboardData: clipboard });
        return clipboard;
      },
      cut(clipboardData) {
        const clipboard = clipboardData || fakeDataTransfer();
        if (this.oncut)
          this.oncut({ clipboardData: clipboard });
        return clipboard;
      },
      paste(text) {
        const clipboard = fakeDataTransfer();
        clipboard.setData("text/plain", text);
        if (this.onpaste)
          this.onpaste({ clipboardData: clipboard });
      },
      invalid() { if (this.oninvalid) this.oninvalid({ preventDefault() {} }); },
      beforeinput(data, inputType = "") {
        if (this.onbeforeinput)
          this.onbeforeinput({ data, inputType });
      },
      input(value) {
        if (typeof value === "boolean")
          this.checked = value;
        else
          this.value = value;
        if (this.oninput)
          this.oninput();
      },
      select(start, end) {
        this.selectionStart = start;
        this.selectionEnd = end;
        if (this.onselect)
          this.onselect();
      },
      change(value) {
        if (typeof value === "boolean")
          this.checked = value;
        else
          this.value = value;
        if (this.onchange)
          this.onchange();
      },
      keydown(key) { if (this.onkeydown) this.onkeydown({ key }); },
      scroll(left, top) {
        this.scrollLeft = left;
        this.scrollTop = top;
        if (this.onscroll)
          this.onscroll();
      },
      scrollTo(left, top) {
        this.scrollLeft = left;
        this.scrollTop = top;
      },
      scrollIntoView(options) { this.scrolledIntoView = options; },
      submit() { if (this.onsubmit) this.onsubmit({ preventDefault() {} }); },
      reset() { if (this.onreset) this.onreset({ preventDefault() {} }); },
      showModal() { this.toggle(true); },
      close(returnValue = "") {
        this.returnValue = returnValue;
        this.open = false;
        this.removeAttribute("open");
        if (this.onclose)
          this.onclose();
      },
      toggle(open = !this.open) {
        this.open = !!open;
        if (this.open)
          this.setAttribute("open", "");
        else
          this.removeAttribute("open");
        if (this.ontoggle)
          this.ontoggle();
      },
      cancel() {
        if (this.oncancel)
          this.oncancel({ preventDefault() {} });
      },
      showPopover() {
        this.popoverOpen = true;
        if (this.ontoggle)
          this.ontoggle();
      },
      hidePopover() {
        this.popoverOpen = false;
        if (this.ontoggle)
          this.ontoggle();
      },
      togglePopover(force) {
        this.popoverOpen = force === undefined ? !this.popoverOpen : !!force;
        if (this.ontoggle)
          this.ontoggle();
      },
      mouseenter() { if (this.onmouseenter) this.onmouseenter(); },
      mouseleave() { if (this.onmouseleave) this.onmouseleave(); },
      mousemove() { if (this.onmousemove) this.onmousemove(); },
      mousedown() { if (this.onmousedown) this.onmousedown(); },
      mouseup() { if (this.onmouseup) this.onmouseup(); },
      wheel(deltaY) { if (this.onwheel) this.onwheel({ deltaY }); },
      focus() { if (this.onfocus) this.onfocus(); },
      blur() { if (this.onblur) this.onblur(); }
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
      { nodeName: "docs", path: "Page/docs", href: "/reference", target: "_blank", rel: "noopener" });
    runtime.widget(ariaRt, "Checkbox", { checked: true, disabled: true, loading: true }, null,
      { nodeName: "accept", path: "Page/accept" });
    runtime.endFrame(ariaRt);
    const snapshot = runtime.webAccessibilitySnapshot(ariaRt);
    assert.equal(snapshot.nodes[0].role, "heading");
    assert.equal(snapshot.nodes[0].level, 2);
    assert.equal(snapshot.nodes[1].role, "link");
    assert.equal(snapshot.nodes[1].href, "/reference");
    assert.equal(snapshot.nodes[2].role, "checkbox");
    assert.equal(snapshot.nodes[2].inputType, "checkbox");
    assert.equal(snapshot.nodes[2].state.checked, true);
    const target = document.createElement("div");
    runtime.renderWebDocument(ariaRt, target);
    const root = target.children[0];
    const link = runtime.findWebElement(target, "Page/docs");
    const checkbox = runtime.findWebElement(target, "Page/accept");
    assert.equal(link.attributes["aria-current"], "page");
    assert.equal(link.attributes.href, "/reference");
    assert.equal(link.attributes.target, "_blank");
    assert.equal(link.attributes.rel, "noopener");
    assert.equal(checkbox.attributes["aria-checked"], "true");
    assert.equal(checkbox.attributes.checked, "");
    assert.equal(checkbox.attributes["aria-disabled"], "true");
    assert.equal(checkbox.attributes["aria-busy"], "true");
    assert.equal(runtime.webFormValue(target, "accept"), true);
    assert.equal(root.children.length, 3);

    let inputValue = null;
    let changeValue = null;
    const formRt = runtime.createRuntime();
    runtime.beginFrame(formRt);
    runtime.widget(formRt, "Checkbox", { checked: false }, null,
      {
        nodeName: "confirm",
        path: "Page/confirm",
        inputAction(value) { inputValue = value; },
        changeAction(value) { changeValue = value; }
      });
    runtime.endFrame(formRt);
    const formTarget = document.createElement("div");
    runtime.renderWebDocument(formRt, formTarget);
    const formCheckbox = runtime.findWebElement(formTarget, "confirm");
    formCheckbox.input(true);
    assert.equal(inputValue, true);
    assert.equal(runtime.webFormValue(formTarget, "confirm"), true);
    formCheckbox.change(false);
    assert.equal(changeValue, false);
    assert.equal(runtime.webFormValue(formTarget, "confirm"), false);
    assert.equal(runtime.webDOMSetValue(formTarget, "confirm", true), true);
    assert.equal(runtime.webDOMGetValue(formTarget, "confirm"), true);
    assert.equal(runtime.webFormValue(formTarget, "confirm"), true);

    let submitValues = null;
    let resetValues = null;
    const submitRt = runtime.createRuntime();
    runtime.beginFrame(submitRt);
    runtime.widget(submitRt, "Column", {}, null,
      {
        nodeName: "contact",
        path: "Page/contact",
        tag: "form",
        formAction: "/contact",
        formMethod: "post",
        formEncType: "multipart/form-data",
        autoComplete: "off",
        noValidate: true,
        onReset: "clear_contact",
        submitAction(values) { submitValues = values; },
        resetAction(values) { resetValues = values; }
      });
    runtime.widget(submitRt, "TextField", { text: "hello@example.test" }, null,
      { nodeName: "email", path: "Page/contact/email", parentPath: "Page/contact", domName: "email" });
    runtime.endFrame(submitRt);
    assert.equal(runtime.webNodeQuery(submitRt, "[action=\"/contact\"]").path, "Page/contact");
    assert.equal(runtime.webNodeQuery(submitRt, "[method=post]").path, "Page/contact");
    assert.equal(runtime.webNodeQuery(submitRt, "[enctype=\"multipart/form-data\"]").path, "Page/contact");
    assert.equal(runtime.webNodeQuery(submitRt, "[autocomplete=off]").path, "Page/contact");
    assert.equal(runtime.webNodeQuery(submitRt, "[novalidate]").path, "Page/contact");
    const submitTarget = document.createElement("div");
    runtime.renderWebDocument(submitRt, submitTarget);
    const submitForm = runtime.findWebElement(submitTarget, "contact");
    assert.equal(submitForm.attributes.action, "/contact");
    assert.equal(submitForm.attributes.method, "post");
    assert.equal(submitForm.attributes.enctype, "multipart/form-data");
    assert.equal(submitForm.attributes.autocomplete, "off");
    assert.equal(submitForm.attributes.novalidate, "");
    assert.equal(submitForm.noValidate, true);
    assert.equal(runtime.webDOMQuery(submitTarget, "[action=\"/contact\"]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[method=post]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[enctype=\"multipart/form-data\"]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[autocomplete=off]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[novalidate]").element, submitForm);
    assert.equal(submitForm.dataset.kryOnReset, "clear_contact");
    assert.equal(runtime.webDOMSubmit(submitTarget, "contact"), true);
    assert.equal(submitValues.email, "hello@example.test");
    assert.equal(submitValues["Page/contact/email"], "hello@example.test");
    assert.equal(runtime.webDOMReset(submitTarget, "Page/contact"), true);
    assert.equal(resetValues.email, "hello@example.test");
    assert.equal(resetValues["Page/contact/email"], "hello@example.test");

    const nativeRt = runtime.createRuntime();
    runtime.beginFrame(nativeRt);
    const nativeEvents = [];
    runtime.widget(nativeRt, "Section", { open: true }, null,
      {
        nodeName: "details",
        path: "Page/details",
        tag: "details",
        toggleAction() { nativeEvents.push("details-toggle"); }
      });
    runtime.widget(nativeRt, "Section", {}, null,
      {
        nodeName: "dialog",
        path: "Page/dialog",
        tag: "dialog",
        closeAction() { nativeEvents.push("dialog-close"); },
        cancelAction() { nativeEvents.push("dialog-cancel"); }
      });
    runtime.widget(nativeRt, "Section", {}, null,
      {
        nodeName: "popover",
        path: "Page/popover",
        tag: "div",
        popover: "auto",
        data: { menu: "main" },
        toggleAction() { nativeEvents.push("popover-toggle"); }
      });
    runtime.widget(nativeRt, "Button", { label: "Menu" }, null,
      {
        nodeName: "popoverButton",
        path: "Page/popoverButton",
        popoverTarget: "popover",
        popoverTargetAction: "toggle"
      });
    runtime.endFrame(nativeRt);
    assert.equal(runtime.webNodeQuery(nativeRt, "Section[open=true]").path, "Page/details");
    assert.equal(runtime.webNodeQuery(nativeRt, "[open]").path, "Page/details");
    const nativeTarget = document.createElement("div");
    runtime.renderWebDocument(nativeRt, nativeTarget);
    const details = runtime.findWebElement(nativeTarget, "details");
    const dialog = runtime.findWebElement(nativeTarget, "dialog");
    const popover = runtime.findWebElement(nativeTarget, "popover");
    const popoverButton = runtime.findWebElement(nativeTarget, "popoverButton");
    assert.equal(details.open, true);
    assert.equal(details.attributes.open, "");
    assert.equal(popover.attributes.popover, "auto");
    assert.equal(popoverButton.attributes.popovertarget, "popover");
    assert.equal(popoverButton.attributes.popovertargetaction, "toggle");
    assert.equal(runtime.webDOMQuery(nativeTarget, "[popover=auto]").element, popover);
    assert.equal(runtime.webDOMQuery(nativeTarget, "[popoverTarget=popover]").element, popoverButton);
    details.toggle(false);
    assert.equal(details.open, false);
    assert.equal(runtime.webDOMGetState(nativeTarget, "details", "open"), false);
    assert.deepEqual(nativeEvents, ["details-toggle"]);
    assert.equal(runtime.webDOMShowModal(nativeTarget, "dialog"), true);
    assert.equal(dialog.open, true);
    assert.equal(runtime.webDOMQuery(nativeTarget, "Section[open=true]").element, dialog);
    dialog.cancel();
    assert.equal(runtime.webDOMClose(nativeTarget, "dialog", "accepted"), true);
    assert.equal(dialog.open, false);
    assert.equal(dialog.returnValue, "accepted");
    assert.deepEqual(nativeEvents.slice(1), ["dialog-cancel", "dialog-close"]);
    assert.equal(runtime.webDOMShowPopover(nativeTarget, "[data-menu=main]"), true);
    assert.equal(popover.popoverOpen, true);
    assert.equal(runtime.webDOMGetState(nativeTarget, "popover", "open"), true);
    assert.equal(runtime.webDOMTogglePopover(nativeTarget, "popover"), true);
    assert.equal(popover.popoverOpen, false);
    assert.equal(runtime.webDOMHidePopover(nativeTarget, "popover"), true);
    assert.equal(runtime.webDOMGetState(nativeTarget, "popover", "open"), false);
    assert.deepEqual(nativeEvents.slice(3), ["popover-toggle", "popover-toggle", "popover-toggle"]);

    const pointerEvents = [];
    const pointerRt = runtime.createRuntime();
    runtime.beginFrame(pointerRt);
    runtime.widget(pointerRt, "Button", { label: "Hover" }, null,
      {
        nodeName: "hover",
        path: "Page/hover",
        onMouseEnter: "enter",
        onMouseLeave: "leave",
        onMouseMove: "move",
        onMouseDown: "down",
        onMouseUp: "up",
        onWheel: "wheel",
        mouseEnterAction() { pointerEvents.push("enter"); },
        mouseLeaveAction() { pointerEvents.push("leave"); },
        mouseMoveAction() { pointerEvents.push("move"); },
        mouseDownAction() { pointerEvents.push("down"); },
        mouseUpAction() { pointerEvents.push("up"); },
        wheelAction(value) { pointerEvents.push("wheel:" + value); }
      });
    runtime.endFrame(pointerRt);
    const pointerTarget = document.createElement("div");
    runtime.renderWebDocument(pointerRt, pointerTarget);
    const pointerButton = runtime.findWebElement(pointerTarget, "hover");
    assert.equal(pointerButton.dataset.kryOnMouseEnter, "enter");
    assert.equal(pointerButton.dataset.kryOnMouseLeave, "leave");
    assert.equal(pointerButton.dataset.kryOnMouseMove, "move");
    assert.equal(pointerButton.dataset.kryOnMouseDown, "down");
    assert.equal(pointerButton.dataset.kryOnMouseUp, "up");
    assert.equal(pointerButton.dataset.kryOnWheel, "wheel");
    pointerButton.mouseenter();
    pointerButton.mousedown();
    pointerButton.mouseup();
    pointerButton.mouseleave();
    pointerButton.mousemove();
    pointerButton.wheel(12);
    assert.deepEqual(pointerEvents, ["enter", "down", "up", "leave", "move", "wheel:12"]);

    const dragEvents = [];
    const dragRt = runtime.createRuntime();
    runtime.beginFrame(dragRt);
    runtime.widget(dragRt, "Button", { label: "Drag" }, null,
      {
        nodeName: "drag",
        path: "Page/drag",
        domValue: "drag-payload",
        draggable: "true",
        onDragStart: "drag_start",
        onDragEnd: "drag_end",
        onDragOver: "drag_over",
        onDrop: "drop",
        onCopy: "copy",
        onCut: "cut",
        onPaste: "paste",
        dragStartAction(value) { dragEvents.push(["start", value]); },
        dragEndAction(value) { dragEvents.push(["end", value]); },
        dragOverAction() { dragEvents.push(["over"]); },
        dropAction(value) { dragEvents.push(["drop", value]); },
        copyAction(value) { dragEvents.push(["copy", value]); },
        cutAction(value) { dragEvents.push(["cut", value]); },
        pasteAction(value) { dragEvents.push(["paste", value]); }
      });
    runtime.endFrame(dragRt);
    const dragTarget = document.createElement("div");
    runtime.renderWebDocument(dragRt, dragTarget);
    const dragButton = runtime.findWebElement(dragTarget, "drag");
    assert.equal(dragButton.attributes.draggable, "true");
    assert.equal(dragButton.dataset.kryOnDragStart, "drag_start");
    assert.equal(dragButton.dataset.kryOnDragEnd, "drag_end");
    assert.equal(dragButton.dataset.kryOnDragOver, "drag_over");
    assert.equal(dragButton.dataset.kryOnDrop, "drop");
    assert.equal(dragButton.dataset.kryOnCopy, "copy");
    assert.equal(dragButton.dataset.kryOnCut, "cut");
    assert.equal(dragButton.dataset.kryOnPaste, "paste");
    const transfer = dragButton.dragstart();
    assert.equal(transfer.getData("text/plain"), "drag-payload");
    dragButton.dragover();
    dragButton.drop(transfer);
    dragButton.dragend();
    const copied = dragButton.copy();
    assert.equal(copied.getData("text/plain"), "drag-payload");
    const cut = dragButton.cut();
    assert.equal(cut.getData("text/plain"), "drag-payload");
    dragButton.paste("pasted text");
    assert.deepEqual(dragEvents, [
      ["start", "drag-payload"],
      ["over"],
      ["drop", "drag-payload"],
      ["end", "drag-payload"],
      ["copy", "drag-payload"],
      ["cut", "drag-payload"],
      ["paste", "pasted text"]
    ]);

    const linkRt = runtime.createRuntime();
    runtime.beginFrame(linkRt);
    runtime.widget(linkRt, "Link", { text: "Manual" }, null,
      {
        nodeName: "manual",
        path: "Page/manual",
        href: "/manual.pdf",
        download: "manual.pdf",
        hidden: true,
        draggable: "false",
        contentEditable: "false"
      });
    runtime.endFrame(linkRt);
    assert.equal(runtime.webNodeQuery(linkRt, "[download=\"manual.pdf\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[hidden]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[draggable=false]").path,
      "Page/manual");
    const linkTarget = document.createElement("div");
    runtime.renderWebDocument(linkRt, linkTarget);
    const manual = runtime.findWebElement(linkTarget, "manual");
    assert.equal(manual.attributes.href, "/manual.pdf");
    assert.equal(manual.attributes.download, "manual.pdf");
    assert.equal(manual.attributes.hidden, "");
    assert.equal(manual.hidden, true);
    assert.equal(manual.attributes.draggable, "false");
    assert.equal(manual.draggable, false);
    assert.equal(manual.attributes.contenteditable, "false");
    assert.equal(manual.contentEditable, "false");

    const sharedRt = runtime.createRuntime();
    runtime.beginFrame(sharedRt);
    runtime.widget(sharedRt, "Text", { text: "Alpha" }, null,
      {
        nodeName: "alpha",
        path: "Shared/alpha",
        sourcePath: "shared.kry",
        sourceLine: 7
      });
    runtime.widget(sharedRt, "Text", { text: "Beta" }, null,
      {
        nodeName: "beta",
        path: "Shared/beta",
        sourcePath: "shared.kry",
        sourceLine: 7
      });
    runtime.endFrame(sharedRt);
    assert.deepEqual(runtime.webNodeQueryAll(sharedRt, "shared.kry:7")
      .map((node) => node.path), ["Shared/alpha", "Shared/beta"]);
    const sharedTarget = document.createElement("div");
    runtime.renderWebDocument(sharedRt, sharedTarget);
    assert.equal(runtime.findWebElement(sharedTarget, "shared.kry:7").textContent,
      "Alpha");
    assert.deepEqual(runtime.webDOMQueryAll(sharedTarget, "shared.kry:7")
      .map((object) => object.node.path), ["Shared/alpha", "Shared/beta"]);
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
    const renderEvents = [];
    target.addEventListener("kry-render", (event) => renderEvents.push(event.detail));
    runtime.SetPageTitle("Runtime title");
    runtime.SetPageDescription("Runtime description");
    runtime.SetPageCanonicalURL("https://example.test/page");
    runtime.SetPageThemeColor(runtime.Color(1, 2, 3, 255));
    runtime.renderWebDocument(domRt, target);
    assert.equal(renderEvents.length, 1);
    assert.equal(renderEvents[0].frame.nodes.length, runtime.webDocumentFrame(domRt).nodes.length);
    assert.equal(renderEvents[0].root, target.children[0]);
    assert.ok(renderEvents[0].objects.some((object) => object.ref === "primary-action"));
    assert.equal(document.title, "Runtime title");
    assert.equal(document.querySelector('meta[name="description"]').attributes.content, "Runtime description");
    assert.equal(document.querySelector('link[rel="canonical"]').attributes.href, "https://example.test/page");
    assert.equal(document.querySelector('meta[name="theme-color"]').attributes.content, "rgb(1, 2, 3)");
    const root = target.children[0];
    assert.equal(runtime.webDOMRoot(target), root);
    assert.equal(runtime.webDOMRoot(root), root);
    assert.equal(runtime.webDOMFrame(target).nodes.length, runtime.webDocumentFrame(domRt).nodes.length);
    assert.equal(root.kryRuntime, domRt);
    assert.equal(root.kryFrame.nodes.length, runtime.webDocumentFrame(domRt).nodes.length);
    assert.ok(root.kryObjects.some((object) => object.ref === "primary-action"));
    assert.equal(Object.keys(root).includes("kryObjects"), false);
    const screen = root.children.find((child) => child.tagName === "MAIN");
    const firstText = screen.children[0];
    assert.equal(firstText.tagName, "DIV");
    assert.match(firstText.dataset.kryRef, /^Scene\/root\/Text@\d+$/);
    assert.equal(firstText.dataset.kryParentPath, "Scene/root");
    assert.equal(firstText.dataset.krySource, "src/valid.kry");
    assert.ok(Number(firstText.dataset.kryLine) > 0);
    const firstButton = screen.children[1];
    assert.equal(firstButton.tagName, "BUTTON");
    assert.equal(firstButton.id, "tap-button");
    assert.equal(firstButton.attributes.value, "tap-value");
    assert.equal(firstButton.dataset.kryIndex, "2");
    assert.equal(firstButton.dataset.kryRef, "primary-action");
    assert.deepEqual(JSON.parse(firstButton.dataset.kryAliases).slice(0, 4), [
      "primary-action",
      "Scene/root/tap",
      "tap",
      "tap-button"
    ]);
    assert.equal(firstButton.dataset.kryPath, "Scene/root/tap");
    assert.equal(firstButton.dataset.kryKey, "tap");
    assert.equal(firstButton.kryRef, "primary-action");
    assert.equal(firstButton.kryPath, "Scene/root/tap");
    assert.deepEqual(firstButton.kryAliases.slice(0, 4), [
      "primary-action",
      "Scene/root/tap",
      "tap",
      "tap-button"
    ]);
    assert.equal(firstButton.kryIndex, 2);
    assert.equal(firstButton.kryKind, "Button");
    assert.equal(firstButton.kryTag, "button");
    assert.equal(firstButton.kryName, "tap");
    assert.equal(firstButton.kryKey, "tap");
    assert.equal(firstButton.krySourceRef, tapSourceRef);
    assert.equal(firstButton.krySourceColumnRef, tapSourceColumnRef);
    assert.equal(firstButton.krySourcePath, "src/valid.kry");
    assert.equal(firstButton.krySourceLine, webDoc.nodes[2].sourceLine);
    assert.equal(firstButton.krySourceColumn, webDoc.nodes[2].sourceColumn);
    assert.equal(firstButton.kryNode.path, "Scene/root/tap");
    assert.equal(firstButton.kryRoot, root);
    assert.equal(firstButton.kryNode.webRef, "primary-action");
    assert.equal(firstButton.kryObject.ref, "primary-action");
    assert.equal(firstButton.kryObject.element, firstButton);
    assert.equal(firstButton.kryIdentity.ref, "primary-action");
    assert.ok(firstButton.kryIdentity.aliases.includes("Scene/root/tap"));
    assert.equal(firstButton.krySnapshot.ref, "primary-action");
    assert.equal(firstButton.krySnapshot.parentRef, "Scene/root");
    assert.equal(firstButton.krySnapshot.element, undefined);
    assert.equal(firstButton.kryParent.node.path, "Scene/root");
    assert.deepEqual(firstButton.kryChildren.map((object) => object.ref), []);
    assert.equal(firstButton.kryMatches("Button.primary"), true);
    assert.equal(firstButton.kryMatches("TextField"), false);
    assert.equal(firstButton.kryClosest("Screen").node.path, "Scene/root");
    assert.equal(Object.keys(firstButton).includes("kryObject"), false);
    assert.equal(Object.keys(firstButton).includes("kryIdentity"), false);
    assert.equal(Object.keys(firstButton).includes("krySnapshot"), false);
    assert.equal(Object.keys(firstButton).includes("kryMatches"), false);
    assert.equal(Object.keys(firstButton).includes("kryPath"), false);
    assert.equal(Object.keys(firstButton).includes("kryAliases"), false);
    assert.equal(root.kryElement("primary-action"), firstButton);
    assert.equal(root.kryObject("primary-action").element, firstButton);
    assert.equal(root.kryQuery("Button.primary").element, firstButton);
    assert.deepEqual(root.kryQueryAll("Button.primary").map((object) => object.ref), ["primary-action"]);
    assert.equal(root.kryAtSource("src/valid.kry", webDoc.nodes[2].sourceLine,
      webDoc.nodes[2].sourceColumn).element, firstButton);
    assert.equal(Object.keys(root).includes("kryQuery"), false);
    assert.equal(firstButton.dataset.krySource, "src/valid.kry");
    assert.ok(Number(firstButton.dataset.kryLine) > 0);
    assert.ok(Number(firstButton.dataset.kryColumn) > 0);
    assert.equal(firstButton.dataset.krySourceRef, tapSourceRef);
    assert.equal(firstButton.dataset.krySourceColumnRef, tapSourceColumnRef);
    assert.equal(firstButton.dataset.kryName, "tap");
    assert.equal(firstButton.attributes["data-tracking-id"], "tap-1");
    assert.equal(firstButton.attributes.title, "Tap details");
    assert.equal(firstButton.attributes.tabindex, "3");
    assert.equal(firstButton.attributes.role, "button");
    assert.equal(firstButton.attributes["aria-label"], "Tap the action");
    assert.equal(firstButton.attributes["aria-description"], "Runs the host action");
    assert.equal(firstButton.attributes["aria-controls"], "search-field");
    assert.equal(firstButton.attributes.popovertarget, "search-menu");
    assert.equal(firstButton.attributes.popovertargetaction, "toggle");
    assert.equal(firstButton.attributes.fetchpriority, "high");
    assert.equal(firstButton.attributes.part, "primary-action");
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(firstButton.style.color, "#f0f0f0");
    assert.equal(firstButton.style.borderRadius, "9px");
    assert.equal(firstButton.style.paddingLeft, "13px");
    firstButton.mouseenter();
    assert.equal(firstButton.style.background, "#304050");
    assert.equal(firstButton.__kryDocNode.state.hover, true);
    firstButton.mousedown();
    assert.equal(firstButton.style.background, "#405060");
    assert.equal(firstButton.__kryDocNode.state.pressed, true);
    firstButton.mouseup();
    assert.equal(firstButton.style.background, "#304050");
    firstButton.mouseleave();
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(firstButton.__kryDocNode.state.hover, false);
    assert.equal(runtime.webDOMFocus(target, "tap-button"), true);
    assert.equal(firstButton.style.borderColor, "#506070");
    assert.equal(firstButton.__kryDocNode.state.focus, true);
    assert.equal(runtime.webDOMBlur(target, "Scene/root/tap"), true);
    assert.equal(firstButton.style.borderColor, "");
    assert.equal(runtime.findWebNode(domRt, "Scene/root/tap").domId, "tap-button");
    assert.equal(runtime.findWebElement(target, "Scene/root/tap"), firstButton);
    assert.equal(runtime.findWebElement(target, "tap"), firstButton);
    assert.equal(runtime.findWebElement(target, "tap-button"), firstButton);
    assert.equal(runtime.findWebElement(target, tapSourceRef), firstButton);
    assert.equal(runtime.findWebElement(target, tapSourceColumnRef), firstButton);
    assert.equal(runtime.webDOMObject(target, "Scene/root/tap").element, firstButton);
    assert.equal(runtime.webDOMObject(target, "tap-button").node.path, "Scene/root/tap");
    assert.equal(runtime.webDOMIdentity(target, "primary-action").domId, "tap-button");
    assert.deepEqual(runtime.webDOMIdentity(target, "tap-button").aliases.slice(0, 4), [
      "primary-action",
      "Scene/root/tap",
      "tap",
      "tap-button"
    ]);
    assert.equal(runtime.webDOMObject(target, tapSourceRef).element, firstButton);
    assert.equal(runtime.webDOMObject(target, tapSourceRef).ref, tapSourceRef);
    assert.equal(runtime.webDOMObject(target, tapSourceColumnRef).ref, tapSourceColumnRef);
    assert.equal(runtime.webDOMObjectFromElement(firstButton).node.path, "Scene/root/tap");
    const nestedSpan = document.createElement("span");
    firstButton.appendChild(nestedSpan);
    assert.equal(runtime.webDOMObjectFromElement(nestedSpan).node.path, "Scene/root/tap");
    const targetEvent = { target: nestedSpan };
    assert.equal(runtime.webDOMDecorateEvent(targetEvent).ref, "primary-action");
    assert.equal(targetEvent.kryRef, "primary-action");
    assert.equal(targetEvent.kryPath, "Scene/root/tap");
    assert.deepEqual(targetEvent.kryAliases.slice(0, 2), ["primary-action", "Scene/root/tap"]);
    assert.equal(targetEvent.kryIndex, 2);
    assert.equal(targetEvent.kryKind, "Button");
    assert.equal(targetEvent.kryTag, "button");
    assert.equal(targetEvent.krySourceRef, tapSourceRef);
    assert.equal(targetEvent.krySourceColumnRef, tapSourceColumnRef);
    assert.equal(targetEvent.krySourcePath, "src/valid.kry");
    assert.equal(targetEvent.krySourceLine, webDoc.nodes[2].sourceLine);
    assert.equal(targetEvent.krySourceColumn, webDoc.nodes[2].sourceColumn);
    assert.equal(targetEvent.kryRoot, root);
    assert.equal(targetEvent.kryObject.node.path, "Scene/root/tap");
    assert.equal(targetEvent.kryIdentity.ref, "primary-action");
    assert.equal(targetEvent.krySnapshot.parentRef, "Scene/root");
    assert.equal(Object.keys(targetEvent).includes("kryObject"), false);
    assert.equal(runtime.webDOMObjectFromEvent({ target: nestedSpan }).ref, "primary-action");
    assert.equal(runtime.webDOMObjectFromEvent({ currentTarget: firstButton }).node.path,
      "Scene/root/tap");
    assert.equal(runtime.webDOMIdentityFromEvent({ target: nestedSpan }).ref, "primary-action");
    assert.equal(runtime.webDOMSnapshotFromElement(nestedSpan).parentRef, "Scene/root");
    assert.equal(runtime.webDOMSnapshotFromEvent({ target: nestedSpan }).ref, "primary-action");
    const buttonSnapshot = runtime.webDOMSnapshot(target, "tap-button");
    assert.equal(buttonSnapshot.ref, "primary-action");
    assert.equal(buttonSnapshot.webRef, "primary-action");
    assert.equal(buttonSnapshot.element, undefined);
    assert.equal(buttonSnapshot.parentRef, "Scene/root");
    assert.equal(buttonSnapshot.attrs.id, "tap-button");
    assert.equal(buttonSnapshot.dataset.kryPath, "Scene/root/tap");
    assert.equal(buttonSnapshot.style.background, "#203040");
    assert.deepEqual(buttonSnapshot.rect, {
      x: 10,
      y: 50,
      width: 120,
      height: 28,
      left: 10,
      top: 50,
      right: 130,
      bottom: 78
    });
    assert.deepEqual(runtime.webDOMSnapshots(target, "Button.primary").map((snapshot) => snapshot.ref),
      ["primary-action"]);
    assert.equal(runtime.webDOMElementMatches(nestedSpan, "Button.primary"), true);
    assert.equal(runtime.webDOMMatches(target, "tap-button", "Button.primary"), true);
    assert.equal(runtime.webDOMMatches(target, "tap-button", "TextField"), false);
    const elementMethodEvents = [];
    const removeElementMethod = firstButton.kryListen("kry-element-method",
      (event, object) => elementMethodEvents.push([event.kryRef, object?.ref]));
    assert.equal(typeof removeElementMethod, "function");
    firstButton.dispatchEvent({ type: "kry-element-method" });
    assert.deepEqual(elementMethodEvents, [["primary-action", "primary-action"]]);
    removeElementMethod();
    firstButton.dispatchEvent({ type: "kry-element-method" });
    assert.deepEqual(elementMethodEvents, [["primary-action", "primary-action"]]);
    const rootMethodEvents = [];
    const removeRootMethod = root.kryListen("tap-button", "kry-root-method",
      (event, object) => rootMethodEvents.push([event.kryPath, object?.node.path]));
    assert.equal(typeof removeRootMethod, "function");
    firstButton.dispatchEvent({ type: "kry-root-method" });
    assert.deepEqual(rootMethodEvents, [["Scene/root/tap", "Scene/root/tap"]]);
    removeRootMethod();
    firstButton.dispatchEvent({ type: "kry-root-method" });
    assert.deepEqual(rootMethodEvents, [["Scene/root/tap", "Scene/root/tap"]]);
    const rootDelegatedEvents = [];
    const removeRootDelegated = root.kryDelegate("Button.primary", "kry-root-delegated",
      (event, object) => rootDelegatedEvents.push([event.kryKind, object.node.path]));
    assert.equal(typeof removeRootDelegated, "function");
    nestedSpan.dispatchEvent({ type: "kry-root-delegated" });
    assert.deepEqual(rootDelegatedEvents, [["Button", "Scene/root/tap"]]);
    removeRootDelegated();
    nestedSpan.dispatchEvent({ type: "kry-root-delegated" });
    assert.deepEqual(rootDelegatedEvents, [["Button", "Scene/root/tap"]]);
    const directEvents = [];
    const removeDirect = runtime.webDOMAddEventListener(target, "tap-button", "kry-test",
      (event, object) => directEvents.push([
        event.type,
        object?.node.path,
        event.kryRef,
        event.kryRoot === root,
        event.kryObject?.ref,
        event.kryIdentity?.ref,
        event.krySnapshot?.ref
      ]));
    assert.equal(typeof removeDirect, "function");
    firstButton.dispatchEvent({ type: "kry-test" });
    assert.deepEqual(directEvents, [[
      "kry-test",
      "Scene/root/tap",
      "primary-action",
      true,
      "primary-action",
      "primary-action",
      "primary-action"
    ]]);
    removeDirect();
    firstButton.dispatchEvent({ type: "kry-test" });
    assert.deepEqual(directEvents, [[
      "kry-test",
      "Scene/root/tap",
      "primary-action",
      true,
      "primary-action",
      "primary-action",
      "primary-action"
    ]]);
    const delegatedEvents = [];
    const removeDelegated = runtime.webDOMAddDelegatedEventListener(target, "Button.primary",
      "kry-delegated", (event, object) =>
        delegatedEvents.push([
          event.type,
          event.target.tagName,
          object.node.path,
          event.kryObject?.node.path
        ]));
    assert.equal(typeof removeDelegated, "function");
    nestedSpan.dispatchEvent({ type: "kry-delegated" });
    assert.deepEqual(delegatedEvents, [["kry-delegated", "SPAN", "Scene/root/tap", "Scene/root/tap"]]);
    removeDelegated();
    nestedSpan.dispatchEvent({ type: "kry-delegated" });
    assert.deepEqual(delegatedEvents, [["kry-delegated", "SPAN", "Scene/root/tap", "Scene/root/tap"]]);
    assert.equal(runtime.webDOMParent(target, "tap-button").node.path, "Scene/root");
    assert.deepEqual(runtime.webDOMChildren(target, "Scene/root")
      .map((object) => object.node.path), [
        firstText.dataset.kryPath,
        "Scene/root/tap",
        "Scene/root/search",
        "Scene/root/search_label"
      ]);
    assert.deepEqual(runtime.webDOMChildren(target).map((object) => object.node.path),
      ["Scene/root"]);
    assert.equal(runtime.webDOMClosest(target, "tap-button", "Screen").node.path,
      "Scene/root");
    assert.deepEqual(runtime.webDOMRect(target, "tap-button"), {
      x: 10,
      y: 50,
      width: 120,
      height: 28,
      left: 10,
      top: 50,
      right: 130,
      bottom: 78
    });
    assert.equal(runtime.webDOMQuery(target, "Button.primary").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[ref=\"primary-action\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[webRef=\"primary-action\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "#tap-button").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[value=\"tap-value\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[index=2]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[domValue=\"tap-value\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[role=button]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[data-tracking-id=\"tap-1\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[aria-current=page]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[aria.pressed=false]").element, firstButton);
    assert.equal(runtime.webDOMHasClass(target, "tap-button", "runtime-selected"), false);
    assert.equal(runtime.webDOMAddClass(target, "tap-button", "runtime-selected"), true);
    assert.equal(runtime.webDOMHasClass(target, "Scene/root/tap", "runtime-selected"), true);
    assert.equal(runtime.webDOMQuery(target, "Button.runtime-selected").element, firstButton);
    assert.equal(firstButton.style.borderWidth, "6px");
    runtime.renderWebDocument(domRt, target);
    assert.equal(runtime.webDOMHasClass(target, "tap-button", "runtime-selected"), true);
    assert.equal(runtime.webDOMToggleClass(target, "tap-button", "runtime-selected"), true);
    assert.equal(runtime.webDOMHasClass(target, "tap-button", "runtime-selected"), false);
    assert.equal(runtime.webDOMToggleClass(target, "tap-button", "runtime-selected", true), true);
    assert.equal(runtime.webDOMRemoveClass(target, "tap-button", "runtime-selected"), true);
    assert.equal(runtime.webDOMHasClass(target, "tap-button", "runtime-selected"), false);
    assert.equal(runtime.webDOMSetAttribute(target, "tap-button", "data-runtime", "1"), true);
    assert.equal(runtime.webDOMGetAttribute(target, "Scene/root/tap", "data-runtime"), "1");
    assert.equal(runtime.webDOMHasAttribute(target, "tap-button", "data-runtime"), true);
    assert.equal(runtime.webDOMQuery(target, "[data-runtime=\"1\"]").element, firstButton);
    assert.equal(firstButton.krySetAttr("data-local", "2"), true);
    assert.equal(firstButton.kryGetAttr("data-local"), "2");
    assert.equal(firstButton.kryHasAttr("data-local"), true);
    assert.equal(runtime.webDOMQuery(target, "[data-local=\"2\"]").element, firstButton);
    assert.equal(firstButton.kryRemoveAttr("data-local"), true);
    assert.equal(firstButton.kryHasAttr("data-local"), false);
    assert.equal(Object.keys(firstButton).includes("krySetAttr"), false);
    assert.equal(firstButton.style.paddingTop, "9px");
    assert.equal(runtime.webDOMSetStyle(target, "tap-button", "background", "pink"), true);
    assert.equal(runtime.webDOMSetStyle(target, "tap-button", "--accent-level", "2"), true);
    assert.equal(runtime.webDOMGetStyle(target, "Scene/root/tap", "background"), "pink");
    assert.equal(runtime.webDOMComputedStyle(target, "Scene/root/tap", "background"), "pink");
    assert.equal(runtime.webDOMGetStyle(target, "tap-button", "--accent-level"), "2");
    assert.equal(runtime.webDOMComputedStyle(target, "tap-button", "--accent-level"), "2");
    runtime.renderWebDocument(domRt, target);
    assert.equal(firstButton.style.background, "pink");
    assert.equal(firstButton.style["--accent-level"], "2");
    assert.equal(runtime.webDOMRemoveStyle(target, "tap-button", "background"), true);
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(runtime.webDOMRemoveStyle(target, "tap-button", "--accent-level"), true);
    assert.equal(runtime.webDOMGetStyle(target, "tap-button", "--accent-level"), "");
    assert.equal(firstButton.krySetStyle("background", "lavender"), true);
    assert.equal(firstButton.kryGetStyle("background"), "lavender");
    assert.equal(firstButton.style.background, "lavender");
    assert.equal(firstButton.kryRemoveStyle("background"), true);
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(runtime.webDOMGetAttribute(target, "tap-button", "data-runtime"), "1");
    assert.equal(runtime.webDOMRemoveAttribute(target, "tap-button", "data-runtime"), true);
    assert.equal(runtime.webDOMHasAttribute(target, "tap-button", "data-runtime"), false);
    assert.equal(runtime.webDOMGetState(target, "tap-button", "disabled"), false);
    assert.equal(runtime.webDOMSetProperty(target, "tap-button", "disabled", true), true);
    assert.equal(runtime.webDOMGetProperty(target, "tap-button", "disabled"), true);
    assert.equal(runtime.webDOMGetState(target, "tap-button", "disabled"), true);
    assert.equal(runtime.webDOMSetProperty(target, "tap-button", "disabled", false), true);
    assert.equal(runtime.webDOMSetState(target, "tap-button", "disabled", true), true);
    assert.equal(runtime.webDOMGetState(target, "Scene/root/tap", "disabled"), true);
    assert.equal(firstButton.kryGetState("disabled"), true);
    assert.equal(firstButton.krySetState("disabled", false), true);
    assert.equal(firstButton.kryGetState("disabled"), false);
    assert.equal(firstButton.krySetState("disabled", true), true);
    assert.equal(firstButton.attributes.disabled, "");
    assert.equal(firstButton.attributes["aria-disabled"], "true");
    assert.equal(firstButton.style.opacity, "0.25");
    assert.equal(runtime.webDOMQuery(target, "Button:disabled").element, firstButton);
    runtime.renderWebDocument(domRt, target);
    assert.equal(runtime.webDOMGetState(target, "tap-button", "disabled"), true);
    assert.equal(runtime.webDOMToggleState(target, "tap-button", "disabled"), true);
    assert.equal(runtime.webDOMGetState(target, "tap-button", "disabled"), false);
    assert.equal(runtime.webDOMGetText(target, "tap-button"), "Tap");
    assert.equal(runtime.webDOMSetProperty(target, "tap-button", "textContent", "Ready"), true);
    assert.equal(runtime.webDOMGetText(target, "tap-button"), "Ready");
    assert.equal(runtime.webDOMSetText(target, "tap-button", "Launch"), true);
    assert.equal(runtime.webDOMGetText(target, "Scene/root/tap"), "Launch");
    assert.equal(runtime.webDOMObject(target, "tap-button").node.text, "Launch");
    assert.equal(firstButton.kryText(), "Launch");
    assert.equal(firstButton.kryText("Go"), true);
    assert.equal(firstButton.kryText(), "Go");
    assert.equal(runtime.webDOMObject(target, "tap-button").node.text, "Go");
    assert.equal(runtime.webDOMQuery(target, "[sourcePath=\"src/valid.kry\"]").element, screen);
    assert.equal(runtime.webDOMQuery(target,
      `[source="src/valid.kry"][line=${webDoc.nodes[2].sourceLine}][column=${webDoc.nodes[2].sourceColumn}]`).element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, `[sourceRef="${tapSourceRef}"]`).element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, `[sourceColumnRef="${tapSourceColumnRef}"]`).element,
      firstButton);
    assert.equal(runtime.webDOMObjectAtSource(target, "src/valid.kry", webDoc.nodes[2].sourceLine,
      webDoc.nodes[2].sourceColumn).element, firstButton);
    assert.deepEqual(runtime.webDOMObjectsAtSource(target, "src/valid.kry",
      webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn).map((object) => object.ref),
      [tapSourceColumnRef]);
    assert.deepEqual(runtime.webDOMQueryAll(target, ".field").map((object) => object.ref), [
      "search-box"
    ]);
    assert.deepEqual(runtime.webDOMQueryAll(target, "[data.role=search]").map((object) => object.ref), [
      "search-box"
    ]);
    assert.equal(runtime.webDOMQuery(target, "[name=q]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[name]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[type=search]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[readonly=true]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[required=true]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[required]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[min=1]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[max=100]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[step=1]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[minlength=2]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[maxlength=64]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[pattern=\"needle.*\"]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[accept=\".txt\"]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[multiple=true]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[multiple]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[inputmode=search]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "[data-role]").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMSetScroll(target, "[data-role]", 7, 19), true);
    assert.deepEqual(runtime.webDOMGetScroll(target, "Scene/root/search"), {
      left: 7,
      top: 19,
      width: 0,
      height: 0
    });
    assert.equal(runtime.webDOMQuery(target, "[for=\"search-field\"]").ref, "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "[htmlFor=\"search-field\"]").ref, "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "[popover=manual]").ref, "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "[popoverTarget=\"search-menu\"]").ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[fetchpriority=high]").ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[part=\"primary-action\"]").ref, "primary-action");
    const domRefs = runtime.webDOMObjects(target).map((object) => object.ref);
    assert.equal(domRefs[0], "Scene/root");
    assert.match(domRefs[1], /^Scene\/root\/Text@\d+$/);
    assert.deepEqual(domRefs.slice(2), [
      "primary-action",
      "search-box",
      "Scene/root/search_label"
    ]);
    const firstField = screen.children[2];
    assert.equal(firstField.__kryDocNode.scrollLeft, 7);
    assert.equal(firstField.__kryDocNode.scrollTop, 19);
    assert.equal(runtime.webDOMScrollIntoView(target, "[data-role]", { block: "center" }), true);
    assert.deepEqual(firstField.scrolledIntoView, { block: "center" });
    assert.equal(firstButton.attributes["aria-current"], "page");
    assert.equal(firstButton.attributes["aria-pressed"], "false");
    assert.equal(firstField.tagName, "INPUT");
    assert.equal(firstField.id, "search-field");
    assert.equal(firstField.attributes.name, "q");
    assert.equal(firstField.dataset.kryRef, "search-box");
    assert.deepEqual(JSON.parse(firstField.dataset.kryAliases).slice(0, 4), [
      "search-box",
      "Scene/root/search",
      "search",
      "search-field"
    ]);
    assert.equal(firstField.attributes["data-role"], "search");
    assert.equal(firstField.attributes.type, "search");
    assert.equal(firstField.attributes.draggable, "true");
    assert.equal(firstField.attributes.spellcheck, "false");
    assert.equal(firstField.attributes.contenteditable, "plaintext-only");
    assert.equal(firstField.attributes.autofocus, "");
    assert.equal(firstField.attributes.formnovalidate, "");
    assert.equal(firstField.attributes.readonly, "");
    assert.equal(firstField.attributes.required, "");
    assert.equal(firstField.attributes.min, "1");
    assert.equal(firstField.attributes.max, "100");
    assert.equal(firstField.attributes.step, "1");
    assert.equal(firstField.attributes.minlength, "2");
    assert.equal(firstField.attributes.maxlength, "64");
    assert.equal(firstField.attributes.pattern, "needle.*");
    assert.equal(firstField.attributes.accept, ".txt");
    assert.equal(firstField.attributes.multiple, "");
    assert.equal(firstField.attributes.inputmode, "search");
    assert.equal(firstField.attributes.placeholder, "Search terms");
    assert.equal(firstField.attributes["aria-describedby"], "tap-button");
    assert.equal(firstField.dataset.kryOnInput, "note_input");
    assert.equal(firstField.dataset.kryOnBeforeInput, "note_before_input");
    assert.equal(firstField.dataset.kryOnChange, "note_change");
    assert.equal(firstField.dataset.kryOnSelect, "note_select");
    assert.equal(firstField.dataset.kryOnKey, "note_key");
    assert.equal(firstField.dataset.kryOnInvalid, "invalid_search");
    assert.equal(firstField.dataset.kryOnScroll, "scroll_search");
    assert.equal(firstField.dataset.kryOnSubmit, "submit_search");
    assert.equal(firstField.dataset.kryOnFocus, "focus_search");
    assert.equal(firstField.dataset.kryOnBlur, "blur_search");
    const searchLabel = screen.children[3];
    assert.equal(searchLabel.tagName, "LABEL");
    assert.equal(searchLabel.attributes.for, "search-field");
    assert.equal(searchLabel.attributes.popover, "manual");
    assert.equal(searchLabel.dataset.kryOnToggle, "toggle_search");
    assert.equal(searchLabel.dataset.kryOnClose, "close_search");
    assert.equal(searchLabel.dataset.kryOnCancel, "cancel_search");
    assert.equal(searchLabel.textContent, "Search");
    assert.equal(firstField.style.borderWidth, "2px");
    assert.equal(firstField.style.paddingTop, "5px");
    assert.equal(runtime.webFormValue(target, "Scene/root/search"), "label");
    assert.equal(runtime.webFormValue(target, "search-box"), "label");
    assert.equal(runtime.webFormValues(target)["search-field"], "label");
    assert.equal(runtime.webFormValues(target)["search-box"], "label");
    assert.equal(runtime.webFormValue(target, "q"), "label");
    assert.equal(runtime.findWebElement(target, "search-box"), firstField);
    assert.equal(runtime.findWebElement(target, "q"), firstField);
    assert.equal(runtime.webDOMSetValue(target, "q", "preset"), true);
    assert.equal(runtime.webDOMGetValue(target, "Scene/root/search"), "preset");
    assert.equal(runtime.webFormValue(target, "q"), "preset");
    assert.equal(firstField.kryValue(), "preset");
    assert.equal(firstField.kryValue("method"), true);
    assert.equal(firstField.kryValue(), "method");
    assert.equal(runtime.webFormValue(target, "q"), "method");
    assert.equal(runtime.webDOMSetProperty(target, "q", "value", "property"), true);
    assert.equal(runtime.webDOMGetValue(target, "Scene/root/search"), "property");
    assert.equal(runtime.webFormValue(target, "q"), "property");
    firstField.beforeinput("n");
    assert.equal(domState.count, 3);
    firstField.input("needle");
    assert.equal(runtime.webFormValue(target, "Scene/root/search"), "needle");
    assert.equal(runtime.webFormValue(target, "search"), "needle");
    assert.equal(runtime.webFormValue(target, "q"), "needle");
    assert.equal(runtime.webFormValues(target)["search-field"], "needle");
    assert.equal(domState.count, 13);
    firstField.select(1, 4);
    assert.equal(domState.count, 213);
    firstField.change("needle");
    assert.equal(runtime.webFormValue(target, "search-field"), "needle");
    assert.equal(domState.count, 313);
    firstField.keydown("Enter");
    assert.equal(domState.count, 1313);
    firstField.scroll(5, 22);
    assert.equal(domState.count, 1335);
    assert.equal(firstField.__kryDocNode.scrollLeft, 5);
    assert.equal(firstField.__kryDocNode.scrollTop, 22);
    assert.equal(firstField.style.fontSize, "18px");
    assert.equal(runtime.webDOMSetProperty(target, "q", "scrollTop", 31), true);
    assert.equal(firstField.__kryDocNode.scrollTop, 31);
    firstField.invalid();
    assert.equal(domState.count, 10001335);
    firstField.submit();
    assert.equal(domState.count, 10011335);
    firstField.focus();
    assert.equal(domState.count, 10111335);
    firstField.blur();
    assert.equal(domState.count, 11111335);
    const countBeforeDispatch = domState.count;
    const dispatchedEvents = [];
    firstField.addEventListener("keydown", (event) => dispatchedEvents.push([
      event.key,
      event.kryRoot === root,
      event.kryObject?.ref,
      event.krySnapshot?.ref,
      Object.keys(event).includes("kryObject"),
      Object.keys(event).includes("__kryEventPropertiesBound")
    ]));
    assert.equal(runtime.webDOMDispatchEvent(target, "[name=q]", "keydown", { key: "Escape" }), true);
    assert.deepEqual(dispatchedEvents, [["Escape", true, "search-box", "search-box", false, false]]);
    assert.equal(domState.count, countBeforeDispatch + 1000);
    assert.equal(firstField.kryDispatch("keydown", { key: "Escape" }), true);
    assert.equal(domState.count, countBeforeDispatch + 2000);
    assert.equal(runtime.webDOMDispatchEvent(target, "[name=q]", "focus"), true);
    assert.equal(firstField.__kryDocNode.state.focus, true);
    assert.equal(runtime.webDOMDispatchEvent(target, "[name=q]", "blur"), true);
    assert.equal(firstField.__kryDocNode.state.focus, false);
    const countBeforeLifecycleDispatch = domState.count;
    assert.equal(runtime.webDOMDispatchEvent(target, "[popover=manual]", "toggle"), true);
    assert.equal(runtime.webDOMDispatchEvent(target, "[popover=manual]", "close"), true);
    assert.equal(runtime.webDOMDispatchEvent(target, "[popover=manual]", "cancel"), true);
    assert.equal(domState.count, countBeforeLifecycleDispatch + 12000000);
    generated.frame(domRt, domState, host);
    runtime.renderWebDocument(domRt, target);
    assert.equal(target.children[0], root);
    assert.equal(root.children.find((child) => child.tagName === "MAIN"), screen);
    assert.equal(screen.children[0], firstText);
    assert.equal(screen.children[1], firstButton);
    assert.equal(screen.children[2], firstField);
    assert.equal(screen.children[3], searchLabel);
    runtime.setWebStyleSheets(domRt, []);
    runtime.renderWebDocument(domRt, target);
    assert.equal(firstButton.style.background, "");
    assert.equal(firstButton.style.color, "");
    runtime.setWebStyleSheets(domRt, webStyleSheet);
    runtime.renderWebDocument(domRt, target);
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(runtime.webDOMClick(target, "tap-button"), true);
    assert.equal(domRt.input.events.at(-1).type, "tap");
    const previousEventCount = domRt.input.events.length;
    const nextRt = runtime.createRuntime({ app: generated.app });
    const nextState = generated.createState();
    generated.frame(nextRt, nextState, host);
    runtime.renderWebDocument(nextRt, target);
    assert.equal(runtime.findWebElement(target, "Scene/root/tap"), firstButton);
    assert.equal(runtime.findWebElement(target, "Scene/root/search"), firstField);
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
assert.equal(defaultSnapshot.frame.length, 5);

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

for (const [actionName, action] of [
  ["DirectAction", generated.Valid_DirectAction],
  ["StoredAction", generated.Valid_StoredAction],
  ["AssignedAction", generated.Valid_AssignedAction]
]) {
  const actionRuntime = runtime.createRuntime({ app: generated.app });
  for (const tapped of [false, true, false]) {
    if (tapped) actionRuntime.QueueTap(30, 110);
    runtime.beginFrame(actionRuntime);
    assert.equal(action(actionRuntime, state, host, 20), tapped);
    const result = runtime.endFrame(actionRuntime);
    assert.equal(result.frame.length, 1);
    assert.equal(result.frame[0].name, "Button");
    assert.match(result.frame[0].meta.path, new RegExp(`^${actionName}/Button@\\d+$`));
    assert.equal(result.frame[0].meta.sourcePath, "src/valid.kry");
    assert.ok(result.frame[0].meta.sourceLine > 0);
    assert.ok(result.frame[0].meta.sourceColumn > 0);
    assert.deepEqual(rectangle(result.frame[0].args.bounds), { x: 20, y: 100, width: 80, height: 32 });
  }
}

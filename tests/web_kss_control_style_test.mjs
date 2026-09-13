import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [runtimePath] = process.argv.slice(2);
assert.ok(runtimePath, "usage: node tests/web_kss_control_style_test.mjs web/kryon-runtime.js");

const runtime = await import(pathToFileURL(runtimePath).href);

const sheet = runtime.parseWebStyleSheet(`
  @pack controls;
  tokens {
    color { accent: #3366ff; caret: #ff6633; }
  }
  TextField.control {
    --control-ring: 2px solid #3366ff;
    accent-color: accent;
    caret-color: caret;
    appearance: none;
    user-select: text;
    resize: vertical;
  }
`);

const css = runtime.webStyleSheetToCSS(sheet);
assert.match(css, /--control-ring: 2px solid #3366ff;/);
assert.match(css, /accent-color: #3366ff;/);
assert.match(css, /caret-color: #ff6633;/);
assert.match(css, /appearance: none;/);
assert.match(css, /user-select: text;/);
assert.match(css, /resize: vertical;/);

const node = {
  kind: "TextField",
  tag: "input",
  classes: ["control"]
};
assert.deepEqual(runtime.resolveWebStyle(node, sheet), {
  "--control-ring": "2px solid #3366ff",
  "accent-color": "#3366ff",
  "caret-color": "#ff6633",
  appearance: "none",
  "user-select": "text",
  resize: "vertical"
});

function fakeElement(tag) {
  const element = {
    tagName: String(tag || "div").toUpperCase(),
    children: [],
    parentNode: null,
    attributes: {},
    dataset: {},
    className: "",
    textContent: "",
    style: {
      setProperty(name, value) {
        this[name] = String(value);
      },
      removeProperty(name) {
        delete this[name];
      },
      getPropertyValue(name) {
        return this[name] || "";
      }
    },
    appendChild(child) {
      child.parentNode = this;
      this.children.push(child);
      return child;
    },
    insertBefore(child, before) {
      child.parentNode = this;
      const index = this.children.indexOf(before);
      if (index < 0)
        this.children.push(child);
      else
        this.children.splice(index, 0, child);
      return child;
    },
    removeChild(child) {
      const index = this.children.indexOf(child);
      if (index >= 0)
        this.children.splice(index, 1);
      child.parentNode = null;
      return child;
    },
    setAttribute(name, value) {
      this.attributes[name] = String(value);
    },
    removeAttribute(name) {
      delete this.attributes[name];
    },
    addEventListener() {},
    dispatchEvent() { return true; },
    querySelector() { return null; }
  };
  return element;
}

globalThis.document = {
  documentElement: fakeElement("html"),
  head: fakeElement("head"),
  body: fakeElement("body"),
  createElement: fakeElement,
  querySelector() { return null; }
};

const host = fakeElement("div");
const rt = runtime.createRuntime({ webStyleSheets: sheet });
runtime.beginFrame(rt);
runtime.widget(rt, "Screen", {}, null, { path: "Page" });
runtime.widget(rt, "TextField", {}, null, {
  path: "Page/control",
  parentPath: "Page",
  class: "control"
});
runtime.renderWebDocument(rt, host);
const field = runtime.findWebElement(host, "Page/control");
assert.equal(field.style["--control-ring"], "2px solid #3366ff");
assert.equal(field.style.accentColor, "#3366ff");
assert.equal(field.style.caretColor, "#ff6633");
assert.equal(field.style.appearance, "none");
assert.equal(field.style.userSelect, "text");
assert.equal(field.style.resize, "vertical");

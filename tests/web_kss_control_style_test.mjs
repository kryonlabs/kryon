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
    color: caret;
    background-color: #101820;
    background-image: linear-gradient(#101820, #203040);
    background-position-x: left;
    background-position-y: top;
    font: italic 14px system-ui;
    font-family: system-ui;
    border-radius: 6;
    border: 3px solid #203040;
    border-top: 1px solid #3366ff;
    border-inline-start: 2px solid #ff6633;
    border-image: linear-gradient(#3366ff, #ff6633) 30;
    border-image-source: linear-gradient(#3366ff, #ff6633);
    border-image-slice: 30;
    border-image-width: 2;
    border-image-outset: 1;
    border-image-repeat: round;
    font-size-adjust: 0.5;
    font-synthesis: none;
    font-synthesis-weight: none;
    font-synthesis-style: none;
    font-synthesis-small-caps: none;
    font-synthesis-position: none;
    text-align-last: center;
    text-rendering: geometricPrecision;
    text-decoration-line: underline;
    text-decoration-skip-ink: auto;
    text-size-adjust: 100%;
    text-orientation: mixed;
    vertical-align: middle;
    transform-box: border-box;
    transform-style: preserve-3d;
    translate: 4px 5px;
    rotate: 12deg;
    scale: 1.2;
    perspective: 800;
    perspective-origin: 50% 50%;
    backface-visibility: hidden;
    accent-color: accent;
    caret-color: caret;
    appearance: none;
    user-select: text;
    resize: vertical;
  }
`);

const css = runtime.webStyleSheetToCSS(sheet);
assert.match(css, /--control-ring: 2px solid #3366ff;/);
assert.match(css, /color: #ff6633;/);
assert.match(css, /background-color: #101820;/);
assert.match(css, /background-image: linear-gradient\(#101820, #203040\);/);
assert.match(css, /background-position-x: left;/);
assert.match(css, /background-position-y: top;/);
assert.match(css, /font: italic 14px system-ui;/);
assert.match(css, /font-family: system-ui;/);
assert.match(css, /border-radius: 6px;/);
assert.match(css, /border: 3px solid #203040;/);
assert.match(css, /border-top: 1px solid #3366ff;/);
assert.match(css, /border-inline-start: 2px solid #ff6633;/);
assert.match(css, /border-image: linear-gradient\(#3366ff, #ff6633\) 30;/);
assert.match(css, /border-image-source: linear-gradient\(#3366ff, #ff6633\);/);
assert.match(css, /border-image-slice: 30;/);
assert.match(css, /border-image-width: 2;/);
assert.match(css, /border-image-outset: 1;/);
assert.match(css, /border-image-repeat: round;/);
assert.match(css, /font-size-adjust: 0.5;/);
assert.match(css, /font-synthesis: none;/);
assert.match(css, /font-synthesis-weight: none;/);
assert.match(css, /font-synthesis-style: none;/);
assert.match(css, /font-synthesis-small-caps: none;/);
assert.match(css, /font-synthesis-position: none;/);
assert.match(css, /text-align-last: center;/);
assert.match(css, /text-rendering: geometricPrecision;/);
assert.match(css, /text-decoration-line: underline;/);
assert.match(css, /text-decoration-skip-ink: auto;/);
assert.match(css, /text-size-adjust: 100%;/);
assert.match(css, /text-orientation: mixed;/);
assert.match(css, /vertical-align: middle;/);
assert.match(css, /transform-box: border-box;/);
assert.match(css, /transform-style: preserve-3d;/);
assert.match(css, /translate: 4px 5px;/);
assert.match(css, /rotate: 12deg;/);
assert.match(css, /scale: 1.2;/);
assert.match(css, /perspective: 800px;/);
assert.match(css, /perspective-origin: 50% 50%;/);
assert.match(css, /backface-visibility: hidden;/);
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
  color: "#ff6633",
  "background-color": "#101820",
  "background-image": "linear-gradient(#101820, #203040)",
  "background-position-x": "left",
  "background-position-y": "top",
  font: "italic 14px system-ui",
  "font-family": "system-ui",
  "border-radius": 6,
  border: "3px solid #203040",
  "border-top": "1px solid #3366ff",
  "border-inline-start": "2px solid #ff6633",
  "border-image": "linear-gradient(#3366ff, #ff6633) 30",
  "border-image-source": "linear-gradient(#3366ff, #ff6633)",
  "border-image-slice": 30,
  "border-image-width": 2,
  "border-image-outset": 1,
  "border-image-repeat": "round",
  "font-size-adjust": 0.5,
  "font-synthesis": "none",
  "font-synthesis-weight": "none",
  "font-synthesis-style": "none",
  "font-synthesis-small-caps": "none",
  "font-synthesis-position": "none",
  "text-align-last": "center",
  "text-rendering": "geometricPrecision",
  "text-decoration-line": "underline",
  "text-decoration-skip-ink": "auto",
  "text-size-adjust": "100%",
  "text-orientation": "mixed",
  "vertical-align": "middle",
  "transform-box": "border-box",
  "transform-style": "preserve-3d",
  translate: "4px 5px",
  rotate: "12deg",
  scale: 1.2,
  perspective: 800,
  "perspective-origin": "50% 50%",
  "backface-visibility": "hidden",
  "accent-color": "#3366ff",
  "caret-color": "#ff6633",
  appearance: "none",
  "user-select": "text",
  resize: "vertical"
});

const lexicalSheet = runtime.parseWebStyleSheet(`
  Disabled:disabled {
    opacity: 0.5;
  }
  Popup[role=dialog] {
    z-index: 20;
  }
  Popup > Text {
    foreground: #223344;
  }
`);

assert.match(runtime.webStyleSheetToCSS(lexicalSheet),
  /\[data-kry-kind="Disabled"\]:is\(:disabled,\[data-kry-state~="disabled"\]\)/);

const disabledNode = {
  kind: "Disabled",
  tag: "fieldset",
  state: { disabled: true }
};
assert.deepEqual(runtime.resolveWebStyle(disabledNode, lexicalSheet), {
  opacity: 0.5
});

const popupNode = {
  kind: "Popup",
  tag: "div",
  role: "dialog",
  path: "PopupBlockNodes/tools"
};
assert.deepEqual(runtime.resolveWebStyle(popupNode, lexicalSheet), {
  "z-index": 20
});

const popupChildNode = {
  kind: "Text",
  tag: "span",
  path: "PopupBlockNodes/tools/label",
  parentPath: "PopupBlockNodes/tools",
  __kryFrameNodes: [popupNode]
};
assert.deepEqual(runtime.resolveWebStyle(popupChildNode, lexicalSheet), {
  foreground: "#223344"
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
assert.equal(field.style.color, "#ff6633");
assert.equal(field.style.backgroundColor, "#101820");
assert.equal(field.style.backgroundImage, "linear-gradient(#101820, #203040)");
assert.equal(field.style.backgroundPositionX, "left");
assert.equal(field.style.backgroundPositionY, "top");
assert.equal(field.style.font, "italic 14px system-ui");
assert.equal(field.style.fontFamily, "system-ui");
assert.equal(field.style.borderRadius, "6px");
assert.equal(field.style.border, "3px solid #203040");
assert.equal(field.style.borderTop, "1px solid #3366ff");
assert.equal(field.style.borderInlineStart, "2px solid #ff6633");
assert.equal(field.style.borderImage, "linear-gradient(#3366ff, #ff6633) 30");
assert.equal(field.style.borderImageSource, "linear-gradient(#3366ff, #ff6633)");
assert.equal(field.style.borderImageSlice, "30");
assert.equal(field.style.borderImageWidth, "2");
assert.equal(field.style.borderImageOutset, "1");
assert.equal(field.style.borderImageRepeat, "round");
assert.equal(field.style.fontSizeAdjust, "0.5");
assert.equal(field.style.fontSynthesis, "none");
assert.equal(field.style.fontSynthesisWeight, "none");
assert.equal(field.style.fontSynthesisStyle, "none");
assert.equal(field.style.fontSynthesisSmallCaps, "none");
assert.equal(field.style.fontSynthesisPosition, "none");
assert.equal(field.style.textAlignLast, "center");
assert.equal(field.style.textRendering, "geometricPrecision");
assert.equal(field.style.textDecorationLine, "underline");
assert.equal(field.style.textDecorationSkipInk, "auto");
assert.equal(field.style.textSizeAdjust, "100%");
assert.equal(field.style.textOrientation, "mixed");
assert.equal(field.style.verticalAlign, "middle");
assert.equal(field.style.transformBox, "border-box");
assert.equal(field.style.transformStyle, "preserve-3d");
assert.equal(field.style.translate, "4px 5px");
assert.equal(field.style.rotate, "12deg");
assert.equal(field.style.scale, "1.2");
assert.equal(field.style.perspective, "800px");
assert.equal(field.style.perspectiveOrigin, "50% 50%");
assert.equal(field.style.backfaceVisibility, "hidden");
assert.equal(field.style.accentColor, "#3366ff");
assert.equal(field.style.caretColor, "#ff6633");
assert.equal(field.style.appearance, "none");
assert.equal(field.style.userSelect, "text");
assert.equal(field.style.resize, "vertical");

import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [runtimePath] = process.argv.slice(2);
assert.ok(runtimePath, "usage: node tests/web_kss_control_style_test.mjs web/kryon-runtime.js");

const runtime = await import(pathToFileURL(runtimePath).href);

// The same cascade decisions are checked in C and Go.
const { readFileSync } = await import("node:fs");
const sharedStyle = await import(new URL("./style_sheet.js", pathToFileURL(runtimePath)));
const cases = readFileSync(new URL("./fixtures/kss/cascade.txt", import.meta.url), "utf8").trim().split("\n");
assert.equal(cases.length, 11);
for (const line of cases) {
  const [kinds, attributes, classes, names, specificity, layer, order, score, currentLayer, currentSpecificity, currentOrder, present, wins] = line.split(/\s+/).map(Number);
  assert.equal(sharedStyle.StyleSheet_StyleSpecificity(null, null, null, kinds, attributes, classes, names), specificity);
  assert.equal(sharedStyle.StyleSheet_StylePriorityScore(null, null, null, layer, specificity, order), score);
  const priority = {present: true, layer, specificity, order};
  const current = {present: Boolean(present), layer: currentLayer, specificity: currentSpecificity, order: currentOrder};
  assert.equal(sharedStyle.StyleSheet_StylePriorityWins(null, null, null, priority, current), Boolean(wins));
}

// Parsed and prebuilt rules take the same shared priority path. Ties choose
// the later declaration, including across sheets; fields cascade independently.
{
  const target = {kind: "Button", classes: ["accent"]};
  const layers = runtime.parseWebStyleSheet('@layer base; Button' + '.accent'.repeat(51) +
    ' {background:#112233;} @layer components; Button {background:#445566;}');
  assert.equal(runtime.resolveWebStyle(target, layers).background, "#445566");
  assert.equal(runtime.traceWebStyle(target, layers).winners.background.layer, 1);

  const parsed = runtime.parseWebStyleSheet(`
    Button.accent { background: #112233; border: #445566; }
    Button { background: #ffffff; foreground: #010203; }
    Button.accent { background: #778899; }
  `);
  assert.equal(parsed.rules[0].selector.specificity, 21);
  const prebuilt = {rules: parsed.rules.map(({score, ...rule}) => rule)};
  const expected = {background: "#778899", border: "#445566", foreground: "#010203"};
  assert.deepEqual(runtime.resolveWebStyle(target, parsed), expected);
  assert.deepEqual(runtime.resolveWebStyle(target, prebuilt), expected);
  assert.deepEqual(runtime.traceWebStyle(target, prebuilt).resolved, expected);
  const misleadingScores = {rules: prebuilt.rules.map((rule, index) => ({
    ...rule, score: index === 1 ? 2147483647 : 0
  }))};
  assert.deepEqual(runtime.resolveWebStyle(target, misleadingScores), expected);
  assert.deepEqual(runtime.traceWebStyle(target, misleadingScores).resolved, expected);
  const later = {rules: [{...prebuilt.rules[2], style: {background: "#aabbcc"}}]};
  assert.equal(runtime.resolveWebStyle(target, [prebuilt, later]).background, "#aabbcc");
  assert.equal(runtime.traceWebStyle(target, [prebuilt, later]).winners.background.value, "#aabbcc");
}

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
    text-spacing-trim: trim-start;
    text-autospace: ideograph-alpha;
    text-box-trim: trim-both;
    text-box-edge: cap alphabetic;
    vertical-align: middle;
    transform-box: border-box;
    transform-style: preserve-3d;
    translate: 4px 5px;
    rotate: 12deg;
    scale: 1.2;
    perspective: 800;
    perspective-origin: 50% 50%;
    backface-visibility: hidden;
    transition-behavior: allow-discrete;
    animation-composition: accumulate;
    animation-timeline: --field-scroll;
    animation-range: entry 0% cover 80%;
    animation-range-start: entry 10%;
    animation-range-end: cover 90%;
    scroll-timeline: --field-scroll block;
    scroll-timeline-name: --field-scroll;
    scroll-timeline-axis: block;
    view-timeline: --field-view inline;
    view-timeline-name: --field-view;
    view-timeline-axis: inline;
    view-timeline-inset: 10% 20%;
    timeline-scope: --field-scroll;
    content-visibility: auto;
    contain-intrinsic-size: 320;
    contain-intrinsic-inline-size: 320;
    contain-intrinsic-block-size: 180;
    overflow-clip-margin: 12;
    anchor-name: --search-control;
    position-anchor: --search-control;
    position-area: bottom span-right;
    position-try: flip-block;
    position-try-fallbacks: flip-inline;
    position-try-order: most-width;
    position-visibility: anchors-visible;
    view-transition-name: search-control;
    accent-color: accent;
    caret-color: caret;
    appearance: none;
    user-select: text;
    resize: vertical;
    flex-flow: row wrap;
    grid: auto-flow / 1fr 2fr;
    grid-template: "label field" auto / auto 1fr;
    align-tracks: stretch;
    justify-tracks: center;
    field-sizing: content;
    interpolate-size: allow-keywords;
    overlay: auto;
    forced-color-adjust: none;
    print-color-adjust: exact;
    color-interpolation: sRGB;
    color-interpolation-filters: linearRGB;
    paint-order: stroke fill markers;
    shape-outside: circle(50%);
    shape-margin: 8;
    shape-image-threshold: 0.4;
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
assert.match(css, /text-spacing-trim: trim-start;/);
assert.match(css, /text-autospace: ideograph-alpha;/);
assert.match(css, /text-box-trim: trim-both;/);
assert.match(css, /text-box-edge: cap alphabetic;/);
assert.match(css, /vertical-align: middle;/);
assert.match(css, /transform-box: border-box;/);
assert.match(css, /transform-style: preserve-3d;/);
assert.match(css, /translate: 4px 5px;/);
assert.match(css, /rotate: 12deg;/);
assert.match(css, /scale: 1.2;/);
assert.match(css, /perspective: 800px;/);
assert.match(css, /perspective-origin: 50% 50%;/);
assert.match(css, /backface-visibility: hidden;/);
assert.match(css, /transition-behavior: allow-discrete;/);
assert.match(css, /animation-composition: accumulate;/);
assert.match(css, /animation-timeline: --field-scroll;/);
assert.match(css, /animation-range: entry 0% cover 80%;/);
assert.match(css, /animation-range-start: entry 10%;/);
assert.match(css, /animation-range-end: cover 90%;/);
assert.match(css, /scroll-timeline: --field-scroll block;/);
assert.match(css, /scroll-timeline-name: --field-scroll;/);
assert.match(css, /scroll-timeline-axis: block;/);
assert.match(css, /view-timeline: --field-view inline;/);
assert.match(css, /view-timeline-name: --field-view;/);
assert.match(css, /view-timeline-axis: inline;/);
assert.match(css, /view-timeline-inset: 10% 20%;/);
assert.match(css, /timeline-scope: --field-scroll;/);
assert.match(css, /content-visibility: auto;/);
assert.match(css, /contain-intrinsic-size: 320px;/);
assert.match(css, /contain-intrinsic-inline-size: 320px;/);
assert.match(css, /contain-intrinsic-block-size: 180px;/);
assert.match(css, /overflow-clip-margin: 12px;/);
assert.match(css, /anchor-name: --search-control;/);
assert.match(css, /position-anchor: --search-control;/);
assert.match(css, /position-area: bottom span-right;/);
assert.match(css, /position-try: flip-block;/);
assert.match(css, /position-try-fallbacks: flip-inline;/);
assert.match(css, /position-try-order: most-width;/);
assert.match(css, /position-visibility: anchors-visible;/);
assert.match(css, /view-transition-name: search-control;/);
assert.match(css, /accent-color: #3366ff;/);
assert.match(css, /caret-color: #ff6633;/);
assert.match(css, /appearance: none;/);
assert.match(css, /user-select: text;/);
assert.match(css, /resize: vertical;/);
assert.match(css, /flex-flow: row wrap;/);
assert.match(css, /grid: auto-flow \/ 1fr 2fr;/);
assert.match(css, /grid-template: "label field" auto \/ auto 1fr;/);
assert.match(css, /align-tracks: stretch;/);
assert.match(css, /justify-tracks: center;/);
assert.match(css, /field-sizing: content;/);
assert.match(css, /interpolate-size: allow-keywords;/);
assert.match(css, /overlay: auto;/);
assert.match(css, /forced-color-adjust: none;/);
assert.match(css, /print-color-adjust: exact;/);
assert.match(css, /color-interpolation: sRGB;/);
assert.match(css, /color-interpolation-filters: linearRGB;/);
assert.match(css, /paint-order: stroke fill markers;/);
assert.match(css, /shape-outside: circle\(50%\);/);
assert.match(css, /shape-margin: 8px;/);
assert.match(css, /shape-image-threshold: 0.4;/);

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
  "text-spacing-trim": "trim-start",
  "text-autospace": "ideograph-alpha",
  "text-box-trim": "trim-both",
  "text-box-edge": "cap alphabetic",
  "vertical-align": "middle",
  "transform-box": "border-box",
  "transform-style": "preserve-3d",
  translate: "4px 5px",
  rotate: "12deg",
  scale: 1.2,
  perspective: 800,
  "perspective-origin": "50% 50%",
  "backface-visibility": "hidden",
  "transition-behavior": "allow-discrete",
  "animation-composition": "accumulate",
  "animation-timeline": "--field-scroll",
  "animation-range": "entry 0% cover 80%",
  "animation-range-start": "entry 10%",
  "animation-range-end": "cover 90%",
  "scroll-timeline": "--field-scroll block",
  "scroll-timeline-name": "--field-scroll",
  "scroll-timeline-axis": "block",
  "view-timeline": "--field-view inline",
  "view-timeline-name": "--field-view",
  "view-timeline-axis": "inline",
  "view-timeline-inset": "10% 20%",
  "timeline-scope": "--field-scroll",
  "content-visibility": "auto",
  "contain-intrinsic-size": 320,
  "contain-intrinsic-inline-size": 320,
  "contain-intrinsic-block-size": 180,
  "overflow-clip-margin": 12,
  "anchor-name": "--search-control",
  "position-anchor": "--search-control",
  "position-area": "bottom span-right",
  "position-try": "flip-block",
  "position-try-fallbacks": "flip-inline",
  "position-try-order": "most-width",
  "position-visibility": "anchors-visible",
  "view-transition-name": "search-control",
  "accent-color": "#3366ff",
  "caret-color": "#ff6633",
  appearance: "none",
  "user-select": "text",
  resize: "vertical",
  "flex-flow": "row wrap",
  grid: "auto-flow / 1fr 2fr",
  "grid-template": "\"label field\" auto / auto 1fr",
  "align-tracks": "stretch",
  "justify-tracks": "center",
  "field-sizing": "content",
  "interpolate-size": "allow-keywords",
  overlay: "auto",
  "forced-color-adjust": "none",
  "print-color-adjust": "exact",
  "color-interpolation": "sRGB",
  "color-interpolation-filters": "linearRGB",
  "paint-order": "stroke fill markers",
  "shape-outside": "circle(50%)",
  "shape-margin": 8,
  "shape-image-threshold": 0.4
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
  /\[data-kry-kind="Disabled"\]:is\(:disabled,\[aria-disabled="true"\],\[data-kry-state~="disabled"\]\)/);

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
assert.equal(field.style.textSpacingTrim, "trim-start");
assert.equal(field.style.textAutospace, "ideograph-alpha");
assert.equal(field.style.textBoxTrim, "trim-both");
assert.equal(field.style.textBoxEdge, "cap alphabetic");
assert.equal(field.style.verticalAlign, "middle");
assert.equal(field.style.transformBox, "border-box");
assert.equal(field.style.transformStyle, "preserve-3d");
assert.equal(field.style.translate, "4px 5px");
assert.equal(field.style.rotate, "12deg");
assert.equal(field.style.scale, "1.2");
assert.equal(field.style.perspective, "800px");
assert.equal(field.style.perspectiveOrigin, "50% 50%");
assert.equal(field.style.backfaceVisibility, "hidden");
assert.equal(field.style.animationTimeline, "--field-scroll");
assert.equal(field.style.animationRange, "entry 0% cover 80%");
assert.equal(field.style.animationRangeStart, "entry 10%");
assert.equal(field.style.animationRangeEnd, "cover 90%");
assert.equal(field.style.scrollTimeline, "--field-scroll block");
assert.equal(field.style.scrollTimelineName, "--field-scroll");
assert.equal(field.style.scrollTimelineAxis, "block");
assert.equal(field.style.viewTimeline, "--field-view inline");
assert.equal(field.style.viewTimelineName, "--field-view");
assert.equal(field.style.viewTimelineAxis, "inline");
assert.equal(field.style.viewTimelineInset, "10% 20%");
assert.equal(field.style.timelineScope, "--field-scroll");
assert.equal(field.style.contentVisibility, "auto");
assert.equal(field.style.containIntrinsicSize, "320px");
assert.equal(field.style.containIntrinsicInlineSize, "320px");
assert.equal(field.style.containIntrinsicBlockSize, "180px");
assert.equal(field.style.overflowClipMargin, "12px");
assert.equal(field.style.anchorName, "--search-control");
assert.equal(field.style.positionAnchor, "--search-control");
assert.equal(field.style.positionArea, "bottom span-right");
assert.equal(field.style.positionTry, "flip-block");
assert.equal(field.style.positionTryFallbacks, "flip-inline");
assert.equal(field.style.positionTryOrder, "most-width");
assert.equal(field.style.positionVisibility, "anchors-visible");
assert.equal(field.style.viewTransitionName, "search-control");
assert.equal(field.style.accentColor, "#3366ff");
assert.equal(field.style.caretColor, "#ff6633");
assert.equal(field.style.appearance, "none");
assert.equal(field.style.userSelect, "text");
assert.equal(field.style.resize, "vertical");
assert.equal(field.style.fieldSizing, "content");
assert.equal(field.style.interpolateSize, "allow-keywords");
assert.equal(field.style.overlay, "auto");
assert.equal(field.style.forcedColorAdjust, "none");
assert.equal(field.style.printColorAdjust, "exact");
assert.equal(field.style.colorInterpolation, "sRGB");
assert.equal(field.style.colorInterpolationFilters, "linearRGB");
assert.equal(field.style.paintOrder, "stroke fill markers");
assert.equal(field.style.shapeOutside, "circle(50%)");
assert.equal(field.style.shapeMargin, "8px");
assert.equal(field.style.shapeImageThreshold, "0.4");

const variantSource = `@pack base; tokens { color { accent: #112233; } }
  Button { background: accent; radius: 9; material: glass; }
  Button:hover { border: accent; }`;
const variant = runtime.parseWebStyleSheet(variantSource, { accent: "#44aa88" });
assert.equal(variant.rules[0].style.background, "#44aa88");
assert.equal(variant.rules[0].style.radius, 9);
assert.equal(variant.rules[0].style.material, "glass");
assert.equal(variant.rules[1].style.border, "#44aa88");
assert.equal(runtime.parseWebStyleSheet(variantSource).rules[0].style.background, "#112233");

import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const [generatedPath, runtimePath] = process.argv.slice(2);
const generated = await import(pathToFileURL(generatedPath).href);
const runtime = await import(pathToFileURL(runtimePath).href);
for (const name of ["Page", "Section", "Heading", "ParagraphText", "Link", "Flow", "Grid", "Scroll", "Fieldset"]) {
  assert.equal(typeof runtime[name], "function");
  assert.equal(runtime[name]().type, name);
}
for (const name of [
  "Abbreviation", "Address", "Area", "Article", "Aside", "Audio", "Base",
  "BidirectionalIsolate", "BidirectionalOverride",
  "BlockQuote", "Bold", "Cite", "Code", "CodeBlock", "Data", "DataList", "Deleted", "DescriptionDetails", "DescriptionList",
  "DescriptionTerm", "Details", "Dialog", "Embed", "Emphasis",
  "Figcaption", "Figure", "Footer", "Form", "Header", "HGroup", "IFrame", "ImageMap",
  "Inserted", "Italic", "Keyboard", "Label", "Legend", "LineBreak", "ListItem", "Main", "Mark", "Meta", "Meter", "Navigation", "NoScript", "EmbeddedObject", "OrderedList",
  "OptionGroup", "Option", "Output", "Param", "Pre", "Quote",
  "Ruby", "RubyParenthesis", "RubyText", "Sample", "Script", "Search", "Select",
  "Slot", "Small", "Source", "Strong", "StyleElement", "Subscript", "Summary", "Superscript", "Table", "TableBody", "TableCaption", "TableCell",
  "TableColumn", "TableColumnGroup", "TableFoot", "TableHead", "TableRow",
  "Template", "Time", "Title", "Track", "UnorderedList", "Variable", "Video", "WordBreakOpportunity"
]) {
  assert.equal(typeof runtime[name], "function");
  assert.equal(runtime[name]().type, name);
}
function rectangle(value) {
  const [x, y, width, height] = Array.isArray(value)
    ? value : [value.x, value.y, value.width, value.height];
  return { x, y, width, height };
}

assert.equal(generated.app.title, "JS Smoke");
assert.equal(generated.app.width, 320);
assert.equal(generated.app.height, 240);
assert.equal(generated.app.styles.length, 4);
assert.deepEqual(generated.app.styles.map(({ kind, target, alias }) => ({ kind, target, alias })), [
  { kind: "builtin", target: "material", alias: "material" },
  { kind: "file", target: "brand.kss", alias: "brand" },
  { kind: "builtin", target: "brand", alias: "brand_pack" },
  { kind: "builtin", target: "acme.dark", alias: "acme_dark" }
]);
assert.match(generated.app.styles[0].source, /@pack material;/);
assert.match(generated.app.styles[1].source, /@pack local\.brand;/);
assert.match(generated.app.styles[2].source, /@pack brand;/);
assert.match(generated.app.styles[3].source, /@pack acme\.dark;/);

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
    duration { fast: 80ms; normal: 0.14s; }
  }
  @keyframes fade-in {
    from {
      opacity: 0;
      transform: translateY(4px);
    }
    50%, 75% {
      opacity: 0.5;
      outline-color: id-face;
    }
    to {
      opacity: 1;
      transform: translateY(0);
    }
  }
  @media (min-width: 720px) {
    Button.primary {
      display: flex;
    }
  }
  @supports (display: grid) {
    Screen > TextField {
      display: grid;
    }
  }
  @container sidebar (min-width: 320px) {
    TextField.control {
      padding-inline: 18;
    }
  }
  @layer components;
  Button.primary {
    background: button-face;
    background-end: #203850;
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
  Button#tap-button:hover:pressed {
    border-width: 7;
  }
  Button#tap-button:focus {
    border: #506070;
  }
  Button#tap-button:active {
    outline-color: #708090;
  }
  Button#tap-button:focus-visible {
    caret-color: #8090a0;
  }
  Button#tap-button:normal {
    outline-width: 5;
  }
  Button[state=hover] {
    outline-color: #607080;
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
  Button.duration-token {
    padding-y: fast;
    opacity: normal;
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
  TextField[aria-details="Scene/root/search_label"] {
    text-align-last: end;
  }
  TextField[required] {
    font-size: 11;
    typeface: ui-sans-serif;
    font-weight: 600;
    font-style: italic;
    font-variant: small-caps;
    font-variant-alternates: historical-forms;
    font-variant-caps: small-caps;
    font-variant-east-asian: ruby;
    font-variant-ligatures: common-ligatures;
    font-variant-numeric: tabular-nums;
    font-variant-position: sub;
    font-language-override: "TRK";
    font-palette: light;
    font-stretch: condensed;
    font-kerning: normal;
    font-optical-sizing: auto;
    font-feature-settings: "kern" 1;
    font-variation-settings: "wght" 600;
    font-size-adjust: 0.5;
    font-synthesis: none;
    font-synthesis-weight: none;
    font-synthesis-style: none;
    font-synthesis-small-caps: none;
    font-synthesis-position: none;
    line-height: 1.4;
    letter-spacing: 1;
    text-indent: 12;
    text-align: center;
    text-rendering: optimizeLegibility;
    text-decoration: underline;
    text-decoration-line: underline overline;
    text-decoration-color: #667788;
    text-decoration-style: wavy;
    text-decoration-skip: spaces;
    text-decoration-skip-ink: auto;
    text-decoration-thickness: 2;
    text-underline-offset: 3;
    text-underline-position: under;
    text-shadow: 0 1px 2px #0004;
    text-emphasis: dot;
    text-emphasis-color: #223344;
    text-emphasis-style: filled sesame;
    text-emphasis-position: over right;
    text-transform: uppercase;
    text-overflow: ellipsis;
    white-space: nowrap;
    text-size-adjust: none;
    text-orientation: mixed;
    text-wrap: balance;
    text-wrap-mode: wrap;
    text-wrap-style: pretty;
    text-justify: inter-word;
    text-combine-upright: digits 2;
    ruby-align: center;
    ruby-position: over;
    word-break: keep-all;
    overflow-wrap: anywhere;
    word-wrap: break-word;
    line-break: strict;
    hanging-punctuation: first;
    vertical-align: 5;
    direction: rtl;
    writing-mode: vertical-rl;
    tab-size: 4;
    hyphens: auto;
    line-clamp: 2;
    list-style: square inside;
    list-style-type: square;
    list-style-position: inside;
    list-style-image: none;
    counter-reset: section 2;
    counter-increment: section;
    counter-set: item 4;
    quotes: "<<" ">>";
    marker-side: match-parent;
    marker-start: open;
    marker-end: close;
    orphans: 3;
    widows: 4;
    box-decoration-break: clone;
    border-collapse: collapse;
    border-spacing: 3;
    table-layout: fixed;
    caption-side: bottom;
    empty-cells: hide;
  }
  TextField[maxlength=64] {
    offset-x: 4;
    margin-x: space.3;
    margin-y: 2;
    margin-left: 14;
    margin-right: 15;
    margin-top: 16;
    margin-bottom: 17;
    padding-left: 18;
    padding-right: 19;
    padding-top: 20;
    padding-bottom: 21;
    padding-inline: 22;
    padding-block: 23;
    padding-inline-start: 24;
    padding-inline-end: 25;
    padding-block-start: 26;
    padding-block-end: 27;
    margin-inline: 28;
    margin-block: 29;
    margin-inline-start: 30;
    margin-inline-end: 31;
    margin-block-start: 32;
    margin-block-end: 33;
    min-width: 44;
    max-height: 55;
    inline-size: 66;
    block-size: 77;
    min-inline-size: 88;
    max-inline-size: 99;
    min-block-size: 111;
    max-block-size: 122;
    display: flex;
    position: relative;
    inset: 1;
    top: 2;
    right: 3;
    bottom: 4;
    left: 5;
    inset-inline: 6;
    inset-block: 7;
    inset-inline-start: 8;
    inset-inline-end: 9;
    inset-block-start: 10;
    inset-block-end: 11;
    overflow-inline: auto;
    overflow-block: hidden;
    overflow-x: auto;
    overflow-y: hidden;
    scroll-behavior: smooth;
    overscroll-behavior: contain;
    overscroll-behavior-x: none;
    overscroll-behavior-y: auto;
    overscroll-behavior-inline: contain;
    overscroll-behavior-block: none;
    scroll-snap-type: x mandatory;
    scroll-snap-align: start center;
    scroll-snap-stop: always;
    scrollbar-color: #223344 #ddeeff;
    scrollbar-width: thin;
    scrollbar-gutter: stable both-edges;
    scroll-margin: 12;
    scroll-margin-top: 13;
    scroll-margin-right: 14;
    scroll-margin-bottom: 15;
    scroll-margin-left: 16;
    scroll-margin-inline: 17;
    scroll-margin-block: 18;
    scroll-margin-inline-start: 19;
    scroll-margin-inline-end: 20;
    scroll-margin-block-start: 21;
    scroll-margin-block-end: 22;
    scroll-padding: 23;
    scroll-padding-top: 24;
    scroll-padding-right: 25;
    scroll-padding-bottom: 26;
    scroll-padding-left: 27;
    scroll-padding-inline: 28;
    scroll-padding-block: 29;
    scroll-padding-inline-start: 30;
    scroll-padding-inline-end: 31;
    scroll-padding-block-start: 32;
    scroll-padding-block-end: 33;
    touch-action: manipulation;
    box-sizing: border-box;
    align-items: center;
    justify-content: space-between;
    align-self: stretch;
    justify-self: center;
    flex-direction: column;
    flex-wrap: wrap;
    flex: 1 1 auto;
    flex-grow: 2;
    flex-shrink: 0;
    flex-basis: space.3;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    grid-template-rows: auto 1fr;
    grid-template-areas: "header header" "nav main";
    grid-auto-columns: minmax(8px, auto);
    grid-auto-rows: 24;
    grid-auto-flow: row dense;
    grid-column: 1 / span 2;
    grid-column-start: 1;
    grid-column-end: 3;
    grid-area: main;
    grid-row: 2 / span 1;
    grid-row-start: 2;
    grid-row-end: 4;
    row-gap: 4;
    column-gap: 6;
    align-content: stretch;
    justify-items: center;
    place-items: center;
    place-content: stretch;
    place-self: center;
    object-fit: contain;
    object-position: center top;
    object-view-box: inset(10% 20% 30% 40%);
    aspect-ratio: 16 / 9;
    image-rendering: pixelated;
    image-orientation: from-image;
    image-resolution: 300dpi;
    background-image: linear-gradient(#102030, #203850);
    background-size: cover;
    background-position: center;
    background-position-x: left;
    background-position-y: top;
    background-repeat: no-repeat;
    background-repeat-x: repeat;
    background-repeat-y: no-repeat;
    background-clip: padding-box;
    background-origin: border-box;
    background-attachment: fixed;
    background-blend-mode: multiply;
    visibility: visible;
    transition: opacity 120ms ease;
    transition-property: opacity, transform;
    transition-duration: 120ms;
    transition-timing-function: ease-in-out;
    transition-delay: 20ms;
    transition-behavior: allow-discrete;
    animation: fade-in 200ms ease both;
    animation-name: fade-in;
    animation-duration: 200ms;
    animation-timing-function: ease;
    animation-delay: 10ms;
    animation-iteration-count: 2;
    animation-direction: alternate;
    animation-fill-mode: both;
    animation-play-state: running;
    animation-composition: accumulate;
    transform: scale(1.1);
    transform-origin: center;
    offset-path: path("M 0 0 L 10 10");
    offset-distance: 50%;
    offset-rotate: auto 45deg;
    offset-anchor: center;
    offset-position: normal;
    filter: contrast(1.1);
    backdrop-filter: blur(2px);
    clip-path: inset(0 round 4px);
    mask-image: linear-gradient(#000, transparent);
    mask-size: cover;
    mask-position: center;
    mask-repeat: no-repeat;
    mask-origin: border-box;
    mask-clip: padding-box;
    mask-composite: exclude;
    mask-mode: alpha;
    border-style: dashed;
    border-top-style: solid;
    border-right-style: dotted;
    border-bottom-style: double;
    border-left-style: groove;
    border-inline-style: ridge;
    border-block-style: inset;
    border-inline-start-style: outset;
    border-inline-end-style: hidden;
    border-block-start-style: none;
    border-block-end-style: solid;
    border-color: #101112;
    border-top-color: #111213;
    border-right-color: #121314;
    border-bottom-color: #131415;
    border-left-color: #141516;
    border-inline-color: #151617;
    border-block-color: #161718;
    border-inline-start-color: #171819;
    border-inline-end-color: #18191a;
    border-block-start-color: #191a1b;
    border-block-end-color: #1a1b1c;
    border-top-width: 22;
    border-right-width: 23;
    border-bottom-width: 24;
    border-left-width: 25;
    border-inline-width: 30;
    border-block-width: 31;
    border-inline-start-width: 32;
    border-inline-end-width: 33;
    border-block-start-width: 34;
    border-block-end-width: 35;
    border-top-left-radius: 26;
    border-top-right-radius: 27;
    border-bottom-right-radius: 28;
    border-bottom-left-radius: 29;
    border-start-start-radius: 36;
    border-start-end-radius: 37;
    border-end-start-radius: 38;
    border-end-end-radius: 39;
    cursor: pointer;
    pointer-events: auto;
    z-index: 3;
    outline: 2px solid #445566;
    outline-width: line;
    outline-offset: 3;
    outline-style: solid;
    outline-color: #445566;
    box-shadow: 0 1px 2px #0004;
    color-scheme: light dark;
    contain: layout paint;
    container: search / inline-size;
    container-type: inline-size;
    container-name: search;
    will-change: transform;
    isolation: isolate;
    mix-blend-mode: multiply;
    columns: 2 auto;
    column-count: 2;
    column-width: 180;
    column-fill: balance;
    column-span: all;
    column-rule: 1px solid #ccc;
    column-rule-color: #ccddee;
    column-rule-style: dashed;
    column-rule-width: 4;
    break-before: avoid;
    break-after: auto;
    break-inside: avoid;
    float: inline-start;
    clear: both;
    order: 2;
  }
  TextField[spellcheck=false] {
    offset-y: 6;
  }
  TextField[formnovalidate] {
    content-offset-y: 7;
  }
  TextField[inert=true] {
    radius: 4;
  }
  TextField[autocapitalize=words] {
    content-offset-x: 3;
  }
  TextField[enterkeyhint=search] {
    icon-size: 16;
  }
  TextField[pattern="needle.*"] {
    icon-size: 14;
  }
  TextField[scrolltop=22] {
    font-size: 18;
  }
`);
assert.equal(webStyleSheet.pack, "smoke");
const webStyleCSS = runtime.webStyleSheetToCSS(webStyleSheet);
assert.equal(webStyleSheet.keyframes[0].name, "fade-in");
assert.equal(webStyleSheet.keyframes[0].frames[1].selector, "50%, 75%");
assert.equal(webStyleSheet.keyframes[0].frames[1].style["outline-color"], "#203040");
assert.deepEqual(webStyleSheet.groups.map((group) => [group.kind, group.query, group.rules.length]), [
  ["media", "(min-width: 720px)", 1],
  ["supports", "(display: grid)", 1],
  ["container", "sidebar (min-width: 320px)", 1]
]);
assert.match(webStyleCSS, /@keyframes fade-in \{/);
assert.match(webStyleCSS, /from \{\n    opacity: 0;\n    transform: translateY\(4px\);/);
assert.match(webStyleCSS, /50%, 75% \{\n    opacity: 0\.5;\n    outline-color: #203040;/);
assert.match(webStyleCSS, /to \{\n    opacity: 1;\n    transform: translateY\(0\);/);
assert.match(webStyleCSS,
  /@media \(min-width: 720px\) \{\n  \[data-kry-kind="Button"\]\.primary \{\n    display: flex;\n  \}\n\}/);
assert.match(webStyleCSS,
  /@supports \(display: grid\) \{\n  \[data-kry-kind="Screen"\] > \[data-kry-kind="TextField"\] \{\n    display: grid;\n  \}\n\}/);
assert.match(webStyleCSS,
  /@container sidebar \(min-width: 320px\) \{\n  \[data-kry-kind="TextField"\]\.control \{\n    padding-inline: 18px;\n  \}\n\}/);
const operatorStyleSheet = runtime.parseWebStyleSheet(`
  Button[webRef^="primary"] { cursor: pointer; }
  Button[webRef$="action"] { pointer-events: auto; }
  Button[webRef*="ary-act"] { appearance: none; }
  Button[data.tracking_id|="tap"] { user-select: none; }
  Button[aria.controls~="search-box"] { resize: both; }
  Screen > Button[webRef="primary-action"] { outline-style: solid; }
  Screen TextField { border-style: dotted; }
  Screen > Text:first-child { visibility: hidden; }
  Screen > Input:last-child { user-select: text; }
  Screen > Button:nth-child(2) { outline-width: 4; }
  Screen > Button:nth-last-child(6) { outline-offset: 6; }
  Screen > Text:nth-child(odd) { line-height: 1.2; }
  Screen > Input:nth-child(even) { appearance: auto; }
  Screen > Text:nth-child(2n+1) { white-space: pre-wrap; }
  Screen > Text:nth-last-child(-n+4) { text-align-last: start; }
  Screen > Text:first-of-type { overflow-wrap: break-word; }
  Screen > Text:last-of-type { text-transform: lowercase; }
  Screen > Button:only-of-type { text-decoration-style: dotted; }
  Screen > Input:nth-of-type(1) { scroll-margin-top: 3; }
  Screen > Input:nth-of-type(n+2) { scroll-margin-bottom: 5; }
  Screen > Input:nth-last-of-type(2n) { scroll-padding-top: 6; }
  Screen > Input:nth-last-of-type(1) { scroll-padding-bottom: 4; }
  Section:empty { field-sizing: content; }
  Button:not(.secondary) { caret-color: #112233; }
  :is(Button, TextField)[webRef^="primary"] { accent-color: #223344; }
  :where(TextField, Button)[data.tracking_id|="tap"] { resize: vertical; }
  Screen:root { text-align: start; }
  :scope > Button { contain: layout; }
  Text + Button { background-size: contain; }
  Button ~ Input { background-repeat: repeat-x; }
`);
assert.match(webStyleCSS, /\[data-kry-kind="Button"\]\.primary/);
assert.match(runtime.webStyleSheetToCSS(runtime.parseWebStyleSheet(`
  Button[webRef="primary-action"] { cursor: pointer; }
`)), /\[data-kry-kind="Button"\]\[data-kry-web-ref="primary-action"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Button"\]\[data-kry-web-ref\^="primary"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Button"\]\[data-tracking-id\|="tap"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Button"\]\[aria-controls~="search-box"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Button"\]\[data-kry-web-ref="primary-action"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] \[data-kry-kind="TextField"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Text"\]:first-child/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Input"\]:last-child/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Button"\]:nth-child\(2\)/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Button"\]:nth-last-child\(6\)/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Text"\]:nth-child\(2n\+1\)/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Text"\]:nth-last-child\(-n\+4\)/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Text"\]:first-of-type/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\] > \[data-kry-kind="Input"\]:nth-last-of-type\(1\)/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Section"\]:empty/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Button"\]:not\(\.kryon-node\.secondary\)/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\.kryon-node\[data-kry-web-ref\^="primary"\]:is\(\[data-kry-kind="Button"\],\[data-kry-kind="TextField"\]\)/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Screen"\]:root/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\.kryon-node:scope > \[data-kry-kind="Button"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Text"\] \+ \[data-kry-kind="Button"\]/);
assert.match(runtime.webStyleSheetToCSS(operatorStyleSheet),
  /\[data-kry-kind="Button"\] ~ \[data-kry-kind="Input"\]/);
assert.match(webStyleCSS, /background: #102030;/);
assert.match(webStyleCSS, /--kry-background-end: #203850;/);
assert.match(webStyleCSS, /background-image: linear-gradient\(#102030, #203850\);/);
assert.match(webStyleCSS, /color: #f0f0f0;/);
assert.match(webStyleCSS, /border-radius: 9px;/);
assert.match(webStyleCSS, /padding-left: 13px;/);
assert.match(webStyleCSS, /padding-right: 13px;/);
assert.match(webStyleCSS, /padding-top: 80px;/);
assert.match(webStyleCSS, /opacity: 140;/);
assert.match(webStyleCSS, /--kry-offset-y: 8px;/);
assert.match(webStyleCSS, /transform: translate\(var\(--kry-offset-x, 0px\), var\(--kry-offset-y, 0px\)\);/);
assert.match(webStyleCSS, /--kry-content-offset-y: 7px;/);
assert.match(webStyleCSS, /--kry-content-offset-x: 3px;/);
assert.match(webStyleCSS, /--kry-icon-size: 14px;/);
assert.match(webStyleCSS, /margin-left: 13px;/);
assert.match(webStyleCSS, /margin-right: 13px;/);
assert.match(webStyleCSS, /margin-top: 2px;/);
assert.match(webStyleCSS, /margin-bottom: 2px;/);
assert.match(webStyleCSS, /margin-left: 14px;/);
assert.match(webStyleCSS, /margin-right: 15px;/);
assert.match(webStyleCSS, /margin-top: 16px;/);
assert.match(webStyleCSS, /margin-bottom: 17px;/);
assert.match(webStyleCSS, /padding-left: 18px;/);
assert.match(webStyleCSS, /padding-right: 19px;/);
assert.match(webStyleCSS, /padding-top: 20px;/);
assert.match(webStyleCSS, /padding-bottom: 21px;/);
assert.match(webStyleCSS, /padding-inline: 22px;/);
assert.match(webStyleCSS, /padding-block: 23px;/);
assert.match(webStyleCSS, /padding-inline-start: 24px;/);
assert.match(webStyleCSS, /padding-inline-end: 25px;/);
assert.match(webStyleCSS, /padding-block-start: 26px;/);
assert.match(webStyleCSS, /padding-block-end: 27px;/);
assert.match(webStyleCSS, /margin-inline: 28px;/);
assert.match(webStyleCSS, /margin-block: 29px;/);
assert.match(webStyleCSS, /margin-inline-start: 30px;/);
assert.match(webStyleCSS, /margin-inline-end: 31px;/);
assert.match(webStyleCSS, /margin-block-start: 32px;/);
assert.match(webStyleCSS, /margin-block-end: 33px;/);
assert.match(webStyleCSS, /min-width: 44px;/);
assert.match(webStyleCSS, /max-height: 55px;/);
assert.match(webStyleCSS, /inline-size: 66px;/);
assert.match(webStyleCSS, /block-size: 77px;/);
assert.match(webStyleCSS, /min-inline-size: 88px;/);
assert.match(webStyleCSS, /max-inline-size: 99px;/);
assert.match(webStyleCSS, /min-block-size: 111px;/);
assert.match(webStyleCSS, /max-block-size: 122px;/);
assert.match(webStyleCSS, /inset: 1px;/);
assert.match(webStyleCSS, /top: 2px;/);
assert.match(webStyleCSS, /right: 3px;/);
assert.match(webStyleCSS, /bottom: 4px;/);
assert.match(webStyleCSS, /left: 5px;/);
assert.match(webStyleCSS, /inset-inline: 6px;/);
assert.match(webStyleCSS, /inset-block: 7px;/);
assert.match(webStyleCSS, /inset-inline-start: 8px;/);
assert.match(webStyleCSS, /inset-inline-end: 9px;/);
assert.match(webStyleCSS, /inset-block-start: 10px;/);
assert.match(webStyleCSS, /inset-block-end: 11px;/);
assert.match(webStyleCSS, /font-family: ui-sans-serif;/);
assert.match(webStyleCSS, /font-weight: 600;/);
assert.match(webStyleCSS, /font-style: italic;/);
assert.match(webStyleCSS, /font-variant: small-caps;/);
assert.match(webStyleCSS, /font-variant-alternates: historical-forms;/);
assert.match(webStyleCSS, /font-variant-caps: small-caps;/);
assert.match(webStyleCSS, /font-variant-east-asian: ruby;/);
assert.match(webStyleCSS, /font-variant-ligatures: common-ligatures;/);
assert.match(webStyleCSS, /font-variant-numeric: tabular-nums;/);
assert.match(webStyleCSS, /font-variant-position: sub;/);
assert.match(webStyleCSS, /font-language-override: "TRK";/);
assert.match(webStyleCSS, /font-palette: light;/);
assert.match(webStyleCSS, /font-stretch: condensed;/);
assert.match(webStyleCSS, /font-kerning: normal;/);
assert.match(webStyleCSS, /font-optical-sizing: auto;/);
assert.match(webStyleCSS, /font-feature-settings: "kern" 1;/);
assert.match(webStyleCSS, /font-variation-settings: "wght" 600;/);
assert.match(webStyleCSS, /font-size-adjust: 0.5;/);
assert.match(webStyleCSS, /font-synthesis: none;/);
assert.match(webStyleCSS, /font-synthesis-weight: none;/);
assert.match(webStyleCSS, /font-synthesis-style: none;/);
assert.match(webStyleCSS, /font-synthesis-small-caps: none;/);
assert.match(webStyleCSS, /font-synthesis-position: none;/);
assert.match(webStyleCSS, /line-height: 1.4;/);
assert.match(webStyleCSS, /letter-spacing: 1px;/);
assert.match(webStyleCSS, /text-indent: 12px;/);
assert.match(webStyleCSS, /text-align: center;/);
assert.match(webStyleCSS, /text-rendering: optimizeLegibility;/);
assert.match(webStyleCSS, /text-decoration: underline;/);
assert.match(webStyleCSS, /text-decoration-line: underline overline;/);
assert.match(webStyleCSS, /text-decoration-color: #667788;/);
assert.match(webStyleCSS, /text-decoration-style: wavy;/);
assert.match(webStyleCSS, /text-decoration-skip: spaces;/);
assert.match(webStyleCSS, /text-decoration-skip-ink: auto;/);
assert.match(webStyleCSS, /text-decoration-thickness: 2px;/);
assert.match(webStyleCSS, /text-underline-offset: 3px;/);
assert.match(webStyleCSS, /text-underline-position: under;/);
assert.match(webStyleCSS, /text-shadow: 0 1px 2px #0004;/);
assert.match(webStyleCSS, /text-emphasis: dot;/);
assert.match(webStyleCSS, /text-emphasis-color: #223344;/);
assert.match(webStyleCSS, /text-emphasis-style: filled sesame;/);
assert.match(webStyleCSS, /text-emphasis-position: over right;/);
assert.match(webStyleCSS, /text-transform: uppercase;/);
assert.match(webStyleCSS, /text-overflow: ellipsis;/);
assert.match(webStyleCSS, /white-space: nowrap;/);
assert.match(webStyleCSS, /text-size-adjust: none;/);
assert.match(webStyleCSS, /text-orientation: mixed;/);
assert.match(webStyleCSS, /text-wrap: balance;/);
assert.match(webStyleCSS, /text-wrap-mode: wrap;/);
assert.match(webStyleCSS, /text-wrap-style: pretty;/);
assert.match(webStyleCSS, /text-justify: inter-word;/);
assert.match(webStyleCSS, /text-combine-upright: digits 2;/);
assert.match(webStyleCSS, /ruby-align: center;/);
assert.match(webStyleCSS, /ruby-position: over;/);
assert.match(webStyleCSS, /word-break: keep-all;/);
assert.match(webStyleCSS, /overflow-wrap: anywhere;/);
assert.match(webStyleCSS, /word-wrap: break-word;/);
assert.match(webStyleCSS, /line-break: strict;/);
assert.match(webStyleCSS, /hanging-punctuation: first;/);
assert.match(webStyleCSS, /vertical-align: 5px;/);
assert.match(webStyleCSS, /list-style: square inside;/);
assert.match(webStyleCSS, /list-style-type: square;/);
assert.match(webStyleCSS, /list-style-position: inside;/);
assert.match(webStyleCSS, /list-style-image: none;/);
assert.match(webStyleCSS, /counter-reset: section 2;/);
assert.match(webStyleCSS, /counter-increment: section;/);
assert.match(webStyleCSS, /counter-set: item 4;/);
assert.match(webStyleCSS, /quotes: "<<" ">>";/);
assert.match(webStyleCSS, /marker-side: match-parent;/);
assert.match(webStyleCSS, /marker-start: open;/);
assert.match(webStyleCSS, /marker-end: close;/);
assert.match(webStyleCSS, /orphans: 3;/);
assert.match(webStyleCSS, /widows: 4;/);
assert.match(webStyleCSS, /box-decoration-break: clone;/);
assert.match(webStyleCSS, /border-collapse: collapse;/);
assert.match(webStyleCSS, /border-spacing: 3px;/);
assert.match(webStyleCSS, /table-layout: fixed;/);
assert.match(webStyleCSS, /caption-side: bottom;/);
assert.match(webStyleCSS, /empty-cells: hide;/);
assert.match(webStyleCSS, /display: flex;/);
assert.match(webStyleCSS, /position: relative;/);
assert.match(webStyleCSS, /z-index: 3;/);
assert.match(webStyleCSS, /overflow-inline: auto;/);
assert.match(webStyleCSS, /overflow-block: hidden;/);
assert.match(webStyleCSS, /overflow-x: auto;/);
assert.match(webStyleCSS, /overflow-y: hidden;/);
assert.match(webStyleCSS, /scroll-behavior: smooth;/);
assert.match(webStyleCSS, /overscroll-behavior: contain;/);
assert.match(webStyleCSS, /overscroll-behavior-x: none;/);
assert.match(webStyleCSS, /overscroll-behavior-y: auto;/);
assert.match(webStyleCSS, /overscroll-behavior-inline: contain;/);
assert.match(webStyleCSS, /overscroll-behavior-block: none;/);
assert.match(webStyleCSS, /scroll-snap-type: x mandatory;/);
assert.match(webStyleCSS, /scroll-snap-align: start center;/);
assert.match(webStyleCSS, /scroll-snap-stop: always;/);
assert.match(webStyleCSS, /scrollbar-color: #223344 #ddeeff;/);
assert.match(webStyleCSS, /scrollbar-width: thin;/);
assert.match(webStyleCSS, /scrollbar-gutter: stable both-edges;/);
assert.match(webStyleCSS, /scroll-margin: 12px;/);
assert.match(webStyleCSS, /scroll-margin-top: 13px;/);
assert.match(webStyleCSS, /scroll-margin-inline-start: 19px;/);
assert.match(webStyleCSS, /scroll-margin-block-end: 22px;/);
assert.match(webStyleCSS, /scroll-padding: 23px;/);
assert.match(webStyleCSS, /scroll-padding-left: 27px;/);
assert.match(webStyleCSS, /scroll-padding-inline: 28px;/);
assert.match(webStyleCSS, /scroll-padding-block-end: 33px;/);
assert.match(webStyleCSS, /touch-action: manipulation;/);
assert.match(webStyleCSS, /box-sizing: border-box;/);
assert.match(webStyleCSS, /align-items: center;/);
assert.match(webStyleCSS, /justify-content: space-between;/);
assert.match(webStyleCSS, /align-self: stretch;/);
assert.match(webStyleCSS, /justify-self: center;/);
assert.match(webStyleCSS, /flex-direction: column;/);
assert.match(webStyleCSS, /flex-wrap: wrap;/);
assert.match(webStyleCSS, /flex: 1 1 auto;/);
assert.match(webStyleCSS, /flex-grow: 2;/);
assert.match(webStyleCSS, /flex-shrink: 0;/);
assert.match(webStyleCSS, /flex-basis: 13px;/);
assert.match(webStyleCSS, /grid-template-columns: repeat\(2, minmax\(0, 1fr\)\);/);
assert.match(webStyleCSS, /grid-template-rows: auto 1fr;/);
assert.match(webStyleCSS, /grid-template-areas: "header header" "nav main";/);
assert.match(webStyleCSS, /grid-auto-columns: minmax\(8px, auto\);/);
assert.match(webStyleCSS, /grid-auto-rows: 24px;/);
assert.match(webStyleCSS, /grid-auto-flow: row dense;/);
assert.match(webStyleCSS, /grid-column: 1 \/ span 2;/);
assert.match(webStyleCSS, /grid-column-start: 1;/);
assert.match(webStyleCSS, /grid-column-end: 3;/);
assert.match(webStyleCSS, /grid-area: main;/);
assert.match(webStyleCSS, /grid-row: 2 \/ span 1;/);
assert.match(webStyleCSS, /grid-row-start: 2;/);
assert.match(webStyleCSS, /grid-row-end: 4;/);
assert.match(webStyleCSS, /row-gap: 4px;/);
assert.match(webStyleCSS, /column-gap: 6px;/);
assert.match(webStyleCSS, /align-content: stretch;/);
assert.match(webStyleCSS, /justify-items: center;/);
assert.match(webStyleCSS, /place-items: center;/);
assert.match(webStyleCSS, /place-content: stretch;/);
assert.match(webStyleCSS, /place-self: center;/);
assert.match(webStyleCSS, /object-fit: contain;/);
assert.match(webStyleCSS, /object-position: center top;/);
assert.match(webStyleCSS, /object-view-box: inset\(10% 20% 30% 40%\);/);
assert.match(webStyleCSS, /aspect-ratio: 16 \/ 9;/);
assert.match(webStyleCSS, /image-rendering: pixelated;/);
assert.match(webStyleCSS, /image-orientation: from-image;/);
assert.match(webStyleCSS, /image-resolution: 300dpi;/);
assert.match(webStyleCSS, /background-size: cover;/);
assert.match(webStyleCSS, /background-position: center;/);
assert.match(webStyleCSS, /background-position-x: left;/);
assert.match(webStyleCSS, /background-position-y: top;/);
assert.match(webStyleCSS, /background-repeat: no-repeat;/);
assert.match(webStyleCSS, /background-repeat-x: repeat;/);
assert.match(webStyleCSS, /background-repeat-y: no-repeat;/);
assert.match(webStyleCSS, /background-clip: padding-box;/);
assert.match(webStyleCSS, /background-origin: border-box;/);
assert.match(webStyleCSS, /background-attachment: fixed;/);
assert.match(webStyleCSS, /background-blend-mode: multiply;/);
assert.match(webStyleCSS, /visibility: visible;/);
assert.match(webStyleCSS, /transition: opacity 120ms ease;/);
assert.match(webStyleCSS, /transition-property: opacity, transform;/);
assert.match(webStyleCSS, /transition-duration: 120ms;/);
assert.match(webStyleCSS, /transition-timing-function: ease-in-out;/);
assert.match(webStyleCSS, /transition-delay: 20ms;/);
assert.match(webStyleCSS, /animation: fade-in 200ms ease both;/);
assert.match(webStyleCSS, /animation-name: fade-in;/);
assert.match(webStyleCSS, /animation-duration: 200ms;/);
assert.match(webStyleCSS, /animation-timing-function: ease;/);
assert.match(webStyleCSS, /animation-delay: 10ms;/);
assert.match(webStyleCSS, /animation-iteration-count: 2;/);
assert.match(webStyleCSS, /animation-direction: alternate;/);
assert.match(webStyleCSS, /animation-fill-mode: both;/);
assert.match(webStyleCSS, /animation-play-state: running;/);
assert.match(webStyleCSS, /transform: translate\(var\(--kry-offset-x, 0px\), var\(--kry-offset-y, 0px\)\) scale\(1\.1\);/);
assert.match(webStyleCSS, /transform-origin: center;/);
assert.match(webStyleCSS, /offset-path: path\("M 0 0 L 10 10"\);/);
assert.match(webStyleCSS, /offset-distance: 50%;/);
assert.match(webStyleCSS, /offset-rotate: auto 45deg;/);
assert.match(webStyleCSS, /offset-anchor: center;/);
assert.match(webStyleCSS, /offset-position: normal;/);
assert.match(webStyleCSS, /filter: contrast\(1\.1\);/);
assert.match(webStyleCSS, /backdrop-filter: blur\(2px\);/);
assert.match(webStyleCSS, /clip-path: inset\(0 round 4px\);/);
assert.match(webStyleCSS, /mask-image: linear-gradient\(#000, transparent\);/);
assert.match(webStyleCSS, /mask-size: cover;/);
assert.match(webStyleCSS, /mask-position: center;/);
assert.match(webStyleCSS, /mask-repeat: no-repeat;/);
assert.match(webStyleCSS, /mask-origin: border-box;/);
assert.match(webStyleCSS, /mask-clip: padding-box;/);
assert.match(webStyleCSS, /mask-composite: exclude;/);
assert.match(webStyleCSS, /mask-mode: alpha;/);
assert.match(webStyleCSS, /border-style: dashed;/);
assert.match(webStyleCSS, /border-top-style: solid;/);
assert.match(webStyleCSS, /border-right-style: dotted;/);
assert.match(webStyleCSS, /border-bottom-style: double;/);
assert.match(webStyleCSS, /border-left-style: groove;/);
assert.match(webStyleCSS, /border-inline-style: ridge;/);
assert.match(webStyleCSS, /border-block-style: inset;/);
assert.match(webStyleCSS, /border-inline-start-style: outset;/);
assert.match(webStyleCSS, /border-inline-end-style: hidden;/);
assert.match(webStyleCSS, /border-block-start-style: none;/);
assert.match(webStyleCSS, /border-block-end-style: solid;/);
assert.match(webStyleCSS, /border-color: #101112;/);
assert.match(webStyleCSS, /border-top-color: #111213;/);
assert.match(webStyleCSS, /border-right-color: #121314;/);
assert.match(webStyleCSS, /border-bottom-color: #131415;/);
assert.match(webStyleCSS, /border-left-color: #141516;/);
assert.match(webStyleCSS, /border-inline-color: #151617;/);
assert.match(webStyleCSS, /border-block-color: #161718;/);
assert.match(webStyleCSS, /border-inline-start-color: #171819;/);
assert.match(webStyleCSS, /border-inline-end-color: #18191a;/);
assert.match(webStyleCSS, /border-block-start-color: #191a1b;/);
assert.match(webStyleCSS, /border-block-end-color: #1a1b1c;/);
assert.match(webStyleCSS, /border-top-width: 22px;/);
assert.match(webStyleCSS, /border-right-width: 23px;/);
assert.match(webStyleCSS, /border-bottom-width: 24px;/);
assert.match(webStyleCSS, /border-left-width: 25px;/);
assert.match(webStyleCSS, /border-inline-width: 30px;/);
assert.match(webStyleCSS, /border-block-width: 31px;/);
assert.match(webStyleCSS, /border-inline-start-width: 32px;/);
assert.match(webStyleCSS, /border-inline-end-width: 33px;/);
assert.match(webStyleCSS, /border-block-start-width: 34px;/);
assert.match(webStyleCSS, /border-block-end-width: 35px;/);
assert.match(webStyleCSS, /border-top-left-radius: 26px;/);
assert.match(webStyleCSS, /border-top-right-radius: 27px;/);
assert.match(webStyleCSS, /border-bottom-right-radius: 28px;/);
assert.match(webStyleCSS, /border-bottom-left-radius: 29px;/);
assert.match(webStyleCSS, /border-start-start-radius: 36px;/);
assert.match(webStyleCSS, /border-start-end-radius: 37px;/);
assert.match(webStyleCSS, /border-end-start-radius: 38px;/);
assert.match(webStyleCSS, /border-end-end-radius: 39px;/);
assert.match(webStyleCSS, /cursor: pointer;/);
assert.match(webStyleCSS, /pointer-events: auto;/);
assert.match(webStyleCSS, /outline: 2px solid #445566;/);
assert.match(webStyleCSS, /outline-width: 2px;/);
assert.match(webStyleCSS, /outline-offset: 3px;/);
assert.match(webStyleCSS, /outline-style: solid;/);
assert.match(webStyleCSS, /outline-color: #445566;/);
assert.match(webStyleCSS, /box-shadow: 0 1px 2px #0004;/);
assert.match(webStyleCSS, /color-scheme: light dark;/);
assert.match(webStyleCSS, /contain: layout paint;/);
assert.match(webStyleCSS, /container: search \/ inline-size;/);
assert.match(webStyleCSS, /container-type: inline-size;/);
assert.match(webStyleCSS, /container-name: search;/);
assert.match(webStyleCSS, /will-change: transform;/);
assert.match(webStyleCSS, /isolation: isolate;/);
assert.match(webStyleCSS, /mix-blend-mode: multiply;/);
assert.match(webStyleCSS, /columns: 2 auto;/);
assert.match(webStyleCSS, /column-count: 2;/);
assert.match(webStyleCSS, /column-width: 180px;/);
assert.match(webStyleCSS, /column-fill: balance;/);
assert.match(webStyleCSS, /column-span: all;/);
assert.match(webStyleCSS, /column-rule: 1px solid #ccc;/);
assert.match(webStyleCSS, /column-rule-color: #ccddee;/);
assert.match(webStyleCSS, /column-rule-style: dashed;/);
assert.match(webStyleCSS, /column-rule-width: 4px;/);
assert.match(webStyleCSS, /break-before: avoid;/);
assert.match(webStyleCSS, /break-after: auto;/);
assert.match(webStyleCSS, /break-inside: avoid;/);
assert.match(webStyleCSS, /float: inline-start;/);
assert.match(webStyleCSS, /clear: both;/);
assert.match(webStyleCSS, /order: 2;/);
assert.match(webStyleCSS,
  /\[data-kry-kind="Button"\]:is\(#tap-button,\[data-kry-name="tap-button"\],\[data-kry-key="tap-button"\]\):is\(:hover,\[data-kry-state~="hover"\]\)/);
assert.match(webStyleCSS,
  /\[data-kry-kind="Button"\]:is\(#tap-button,\[data-kry-name="tap-button"\],\[data-kry-key="tap-button"\]\):is\(:hover,\[data-kry-state~="hover"\]\):is\(:active,\[aria-pressed="true"\],\[data-kry-state~="pressed"\]\)/);
assert.match(webStyleCSS,
  /\[data-kry-kind="Button"\]:is\(#tap-button,\[data-kry-name="tap-button"\],\[data-kry-key="tap-button"\]\):is\(:active,\[aria-pressed="true"\],\[data-kry-state~="pressed"\]\)/);
assert.match(webStyleCSS,
  /\[data-kry-kind="Button"\]:is\(#tap-button,\[data-kry-name="tap-button"\],\[data-kry-key="tap-button"\]\):is\(:focus,:focus-visible,\[data-kry-state~="focus"\]\)/);
assert.match(webStyleCSS,
  /\[data-kry-kind="Button"\]:is\(#tap-button,\[data-kry-name="tap-button"\],\[data-kry-key="tap-button"\]\):not\(:is\(:hover,:focus,:active,:disabled,:checked,:invalid,:read-only,:required,\[readonly\],\[required\],\[open\],\[selected\],\[aria-pressed="true"\],\[aria-disabled="true"\],\[aria-busy="true"\],\[aria-checked="true"\],\[aria-selected="true"\],\[aria-current\],\[aria-invalid="true"\],\[aria-expanded="true"\],\[data-kry-state\]\)\)/);
assert.match(webStyleCSS,
  /\[data-kry-kind="Button"\]\[data-kry-state~="hover"\]/);
const stateSelectorCSS = runtime.webStyleSheetToCSS(runtime.parseWebStyleSheet(`
  Button:disabled { opacity: 0.5; }
  Button:enabled { cursor: pointer; }
  Button:active { border-color: #121212; }
  Button:focus-visible { outline-color: #232323; }
  Screen:focus-within { outline-width: 6; }
  Button:target { text-decoration-line: underline; }
  Screen:has(> Button) { padding: 4; }
  Text:has(+ Button) { padding-x: 5; }
  Text:has(~ TextField) { padding-y: 6; }
  TextField:placeholder-shown { opacity: 0.61; }
  Toggle:indeterminate { opacity: 0.62; }
  Radio:default { opacity: 0.63; }
  TextField:autofill { opacity: 0.64; }
  Selectable:selected { opacity: 0.6; }
  Toggle:checked { opacity: 0.7; }
  TextField:invalid { opacity: 0.8; }
  TextField:valid { text-shadow: none; }
  Section:expanded { opacity: 0.9; }
  Section:loading { cursor: progress; }
  TextField:readonly { color: #111111; }
  TextField:read-only { caret-color: #222222; }
  TextField:required { outline-color: #333333; }
  Input:optional { outline-style: dotted; }
`));
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Button"\]:is\(:disabled,\[aria-disabled="true"\],\[data-kry-state~="disabled"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Button"\]:is\(:enabled,\[data-kry-state~="enabled"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Button"\]:is\(:active,\[aria-pressed="true"\],\[data-kry-state~="pressed"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Button"\]:is\(:focus,:focus-visible,\[data-kry-state~="focus"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Screen"\]:focus-within/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Button"\]:target/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Screen"\]:has\(> \[data-kry-kind="Button"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Text"\]:has\(\+ \[data-kry-kind="Button"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Text"\]:has\(~ \[data-kry-kind="TextField"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="TextField"\]:is\(:placeholder-shown,\[data-kry-state~="placeholder-shown"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Toggle"\]:is\(:indeterminate,\[aria-checked="mixed"\],\[data-kry-state~="indeterminate"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Radio"\]:is\(:default,\[data-kry-state~="default"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="TextField"\]:is\(:autofill,:-webkit-autofill,\[data-kry-state~="autofill"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Selectable"\]:is\(:checked,\[selected\],\[aria-selected="true"\],\[aria-current\],\[data-kry-state~="selected"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Toggle"\]:is\(:checked,\[aria-checked="true"\],\[data-kry-state~="checked"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="TextField"\]:is\(:invalid,\[aria-invalid="true"\],\[data-kry-state~="invalid"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="TextField"\]:is\(:valid,\[aria-invalid="false"\],\[data-kry-state~="valid"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Section"\]:is\(\[aria-expanded="true"\],\[data-kry-state~="expanded"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Section"\]:is\(\[aria-busy="true"\],\[data-kry-state~="loading"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="TextField"\]:is\(:read-only,\[readonly\],\[data-kry-state~="readonly"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="TextField"\]:is\(:required,\[required\],\[data-kry-state~="required"\]\)/);
assert.match(stateSelectorCSS,
  /\[data-kry-kind="Input"\]:is\(:optional,\[data-kry-state~="optional"\]\)/);
assert.match(webStyleCSS, /\[data-kry-kind="TextField"\]\[data-role="search"\]/);
for (const legacyAlias of [
  "focus-color",
  "background_end", "border_width", "padding_x", "padding_y",
  "font_size", "icon_size", "offset_x", "offset_y", "font_family",
  "text_align", "pointer_events", "outline_width"
]) {
  assert.throws(() => runtime.parseWebStyleSheet(`Button { ${legacyAlias}: #111111; }`),
    /unknown KSS property/);
}
assert.throws(() => runtime.parseWebStyleSheet("@theme dark; Button { background: #111111; }"),
  /unknown KSS directive @theme/);
assert.throws(() => runtime.parseWebStyleSheet("@layer legacy; Button { background: #111111; }"),
  /unknown KSS layer legacy/);
assert.throws(() => runtime.parseWebStyleSheet("tokens { colors { face: #111111; } } Button { background: #111111; }"),
  /unknown KSS token group colors/);
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
assert.equal(snap.frame.length, 8);
assert.equal(snap.frame[0].name, "Screen");
assert.equal(snap.frame[1].name, "Text");
assert.equal(snap.frame[2].name, "Button");
assert.equal(typeof snap.frame[2].args, "object");
assert.deepEqual(rectangle(snap.frame[2].args.bounds), { x: 10, y: 50, width: 120, height: 28 });
assert.equal(snap.frame[2].args.label, "Tap");
assert.equal(typeof snap.frame[2].args.class_name, "number");
assert.equal("style" in snap.frame[2].args, false);
assert.equal(snap.frame[5].name, "Selectable");
assert.equal(snap.frame[6].name, "Input");
assert.equal(snap.frame[7].name, "Input");
const webDoc = runtime.webDocumentFrame(rt);
assert.deepEqual(webDoc.nodes.map((node) => [node.kind, node.tag]), [
  ["Screen", "main"],
  ["Text", "span"],
  ["Button", "button"],
  ["TextField", "input"],
  ["Text", "label"],
  ["Selectable", "div"],
  ["Input", "input"],
  ["Input", "input"]
]);
assert.equal(webDoc.nodes[6].inputType, "number");
assert.equal(webDoc.nodes[7].inputType, "number");
assert.equal(webDoc.nodes[2].text, "Tap");
assert.deepEqual(webDoc.nodes[2].bounds, { x: 10, y: 50, width: 120, height: 28 });
assert.equal(webDoc.nodes[2].key, "tap");
assert.equal(webDoc.nodes[2].name, "tap");
assert.equal(webDoc.nodes[2].path, "Scene/root/tap");
assert.equal(webDoc.nodes[2].parentPath, "Scene/root");
assert.equal(webDoc.nodes[2].webRef, "primary-action");
assert.equal(runtime.webNodeQuery(rt, `[webRef^="primary"]`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `[webRef$="action"]`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `[webRef*="ary-act"]`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `[data.tracking_id|="tap"]`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `[aria.controls~="search-box"]`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Button[webRef="primary-action"]`).path,
  webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Screen TextField`).path, webDoc.nodes[3].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Text:first-child`).path, webDoc.nodes[1].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Input:last-child`).path, webDoc.nodes[7].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Button:nth-child(2)`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Button:nth-last-child(6)`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Text:nth-child(odd)`).path, webDoc.nodes[1].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Input:nth-child(even)`).path, webDoc.nodes[6].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Text:nth-child(2n+1)`).path, webDoc.nodes[1].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Text:nth-last-child(-n+4)`).path, webDoc.nodes[4].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Text:first-of-type`).path, webDoc.nodes[1].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Text:last-of-type`).path, webDoc.nodes[4].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Button:only-of-type`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Input:nth-of-type(1)`).path, webDoc.nodes[6].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Input:nth-of-type(n+2)`).path, webDoc.nodes[7].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Input:nth-last-of-type(2n)`).path, webDoc.nodes[6].path);
assert.equal(runtime.webNodeQuery(rt, `Screen > Input:nth-last-of-type(1)`).path, webDoc.nodes[7].path);
assert.equal(runtime.webNodeQuery(rt, `Button:not(.secondary)`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `:is(Button, TextField)[webRef^="primary"]`).path,
  webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `:where(TextField, Button)[data.tracking_id|="tap"]`).path,
  webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Screen:root`).path, "Scene/root");
assert.equal(runtime.webNodeQueryWithin(rt, "Scene/root", `:scope > Button`).path,
  webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Text + Button`).path, webDoc.nodes[2].path);
assert.equal(runtime.webNodeQuery(rt, `Button ~ Input`).path, webDoc.nodes[6].path);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet).cursor, "pointer");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["pointer-events"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet).appearance, "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["user-select"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet).resize, "vertical");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["outline-style"], "solid");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], operatorStyleSheet)["border-style"], "dotted");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[1], operatorStyleSheet).visibility, "hidden");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[7], operatorStyleSheet)["user-select"], "text");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["outline-width"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["outline-offset"], 6);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[1], operatorStyleSheet)["line-height"], 1.2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[6], operatorStyleSheet).appearance, "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[1], operatorStyleSheet)["white-space"], "pre-wrap");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[4], operatorStyleSheet)["text-align-last"], "start");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[1], operatorStyleSheet)["overflow-wrap"], "break-word");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[4], operatorStyleSheet)["text-transform"], "lowercase");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["text-decoration-style"], "dotted");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[6], operatorStyleSheet)["scroll-margin-top"], 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[7], operatorStyleSheet)["scroll-margin-bottom"], 5);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[6], operatorStyleSheet)["scroll-padding-top"], 6);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[7], operatorStyleSheet)["scroll-padding-bottom"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["caret-color"], "#112233");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["accent-color"], "#223344");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet).resize, "vertical");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], operatorStyleSheet)["background-size"], "contain");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[6], operatorStyleSheet)["background-repeat"], "repeat-x");
const soloRt = runtime.createRuntime();
runtime.beginFrame(soloRt);
runtime.widget(soloRt, "Screen", {}, null, { nodeName: "root", path: "Solo/root" });
runtime.widget(soloRt, "Text", { text: "Only" }, null,
  { nodeName: "only", path: "Solo/root/only", parentPath: "Solo/root" });
runtime.endFrame(soloRt);
const soloDoc = runtime.webDocumentFrame(soloRt);
const onlyChildStyleSheet = runtime.parseWebStyleSheet(`
  Screen > Text:only-child { visibility: hidden; }
`);
assert.equal(runtime.webNodeQuery(soloRt, `Screen > Text:only-child`).path,
  soloDoc.nodes[1].path);
assert.equal(runtime.resolveWebStyle(soloDoc.nodes[1], onlyChildStyleSheet).visibility,
  "hidden");
const emptyRt = runtime.createRuntime();
runtime.beginFrame(emptyRt);
runtime.widget(emptyRt, "Screen", {}, null, { nodeName: "root", path: "Empty/root" });
runtime.widget(emptyRt, "Section", {}, null,
  { nodeName: "panel", path: "Empty/root/panel", parentPath: "Empty/root" });
runtime.endFrame(emptyRt);
const emptyDoc = runtime.webDocumentFrame(emptyRt);
assert.equal(runtime.webNodeQuery(emptyRt, `Section:empty`).path,
  emptyDoc.nodes[1].path);
assert.equal(runtime.resolveWebStyle(emptyDoc.nodes[1], operatorStyleSheet)["field-sizing"],
  "content");
const loadingRt = runtime.createRuntime();
runtime.beginFrame(loadingRt);
runtime.widget(loadingRt, "Screen", {}, null, { nodeName: "root", path: "Loading/root" });
runtime.widget(loadingRt, "Section", { loading: true }, null,
  { nodeName: "panel", path: "Loading/root/panel", parentPath: "Loading/root" });
runtime.endFrame(loadingRt);
const loadingDoc = runtime.webDocumentFrame(loadingRt);
const loadingSheet = runtime.parseWebStyleSheet(`Section:loading { cursor: progress; }`);
assert.equal(runtime.webNodeQuery(loadingRt, `Section:loading`).path,
  loadingDoc.nodes[1].path);
assert.equal(runtime.resolveWebStyle(loadingDoc.nodes[1], loadingSheet).cursor, "progress");
const currentRt = runtime.createRuntime();
runtime.beginFrame(currentRt);
runtime.widget(currentRt, "Screen", {}, null, { nodeName: "root", path: "Current/root" });
runtime.widget(currentRt, "Link", { href: "/now", text: "Now", selected: true }, null,
  { nodeName: "now", path: "Current/root/now", parentPath: "Current/root" });
runtime.endFrame(currentRt);
const currentDoc = runtime.webDocumentFrame(currentRt);
const currentSheet = runtime.parseWebStyleSheet(`Link:selected { color: #abcdef; }`);
assert.equal(runtime.webNodeQuery(currentRt, `Link:selected`).path,
  currentDoc.nodes[1].path);
assert.equal(runtime.resolveWebStyle(currentDoc.nodes[1], currentSheet).color, "#abcdef");
assert.equal(webDoc.nodes[2].sourcePath, "src/valid.kry");
assert.ok(webDoc.nodes[2].sourceLine > 0);
assert.ok(webDoc.nodes[2].sourceColumn > 0);
assert.equal(webDoc.nodes[2].sourceColumn, 9);
assert.ok(webDoc.nodes[2].sourceEndLine > 0);
assert.ok(webDoc.nodes[2].sourceEndColumn > 0);
assert.ok(webDoc.nodes[2].sourceEndLine > webDoc.nodes[2].sourceLine);
assert.equal(webDoc.nodes[6].key, webDoc.nodes[6].path);
assert.equal(webDoc.nodes[7].key, webDoc.nodes[7].path);
const tapSourceRef = `${webDoc.nodes[2].sourcePath}:${webDoc.nodes[2].sourceLine}`;
const tapSourceColumnRef = `${tapSourceRef}:${webDoc.nodes[2].sourceColumn}`;
const tapSourceRangeRef = `${tapSourceColumnRef}-${webDoc.nodes[2].sourceEndLine}:${webDoc.nodes[2].sourceEndColumn}`;
assert.equal(runtime.webSourceRef("src/valid.kry", webDoc.nodes[2].sourceLine), tapSourceRef);
assert.equal(runtime.webSourceRef("src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn), tapSourceColumnRef);
assert.equal(runtime.webSourceRangeRef("src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn, webDoc.nodes[2].sourceEndLine,
  webDoc.nodes[2].sourceEndColumn), tapSourceRangeRef);
assert.equal(runtime.webSourceRangeRef("", 0, 0, 0, 0), "");
assert.equal(runtime.findWebNode(rt, tapSourceRef).path, webDoc.nodes[2].path);
assert.equal(runtime.findWebNode(rt, tapSourceColumnRef).path, webDoc.nodes[2].path);
assert.equal(runtime.webSourceMap(rt)
  .some((entry) => entry.ref === "primary-action" &&
    entry.sourceColumnRef === tapSourceColumnRef &&
    entry.sourceRangeRef === tapSourceRangeRef), true);
assert.equal(runtime.webNodeAtSource(rt, "src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn).path, webDoc.nodes[2].path);
assert.deepEqual(runtime.webNodesAtSource(rt, "src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn).map((node) => node.path), [webDoc.nodes[2].path]);
assert.equal(runtime.webNodeAtSourceRange(rt, "src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn + 1).path, webDoc.nodes[2].path);
assert.deepEqual(runtime.webNodesAtSourceRange(rt, "src/valid.kry", webDoc.nodes[2].sourceLine,
  webDoc.nodes[2].sourceColumn + 1).map((node) => node.path),
  [webDoc.nodes[2].path, webDoc.nodes[0].path]);
assert.deepEqual(runtime.webNodesAtSourceRange(rt, "src/valid.kry", webDoc.nodes[2].sourceEndLine,
  webDoc.nodes[2].sourceEndColumn + 1).map((node) => node.path), [webDoc.nodes[0].path]);
assert.equal(runtime.webNodeOverlappingSourceRange(rt, "src/valid.kry",
  webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn + 1,
  webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn - 1).path,
  webDoc.nodes[2].path);
assert.deepEqual(runtime.webNodesOverlappingSourceRange(rt, "src/valid.kry",
  webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn + 1,
  webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn - 1)
  .map((node) => node.path), [webDoc.nodes[2].path, webDoc.nodes[0].path]);
assert.deepEqual(runtime.webNodesOverlappingSourceRange(rt, "src/valid.kry",
  webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn + 1,
  webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn + 4)
  .map((node) => node.path), [webDoc.nodes[0].path]);
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
assert.equal(webDoc.nodes[2].ariaControls, "search-box");
assert.equal(webDoc.nodes[2].ariaOwns, "search-box");
assert.equal(webDoc.nodes[2].ariaDetails, "Scene/root/search_label");
assert.equal(webDoc.nodes[2].ariaErrorMessage, "Scene/root/search_label");
assert.equal(webDoc.nodes[2].ariaFlowTo, "search-box");
assert.equal(webDoc.nodes[2].popoverTarget, "Scene/root/search_label");
assert.equal(webDoc.nodes[2].popoverTargetAction, "toggle");
assert.deepEqual(webDoc.nodes[2].extraAttrs, {
  fetchpriority: "high",
  part: "primary-action"
});
assert.equal(webDoc.nodes[2].onClick, "call_host");
assert.equal(runtime.webNodeEventRefs(webDoc.nodes[2]).click, "call_host");
assert.equal(runtime.webNodeEventRefs(webDoc.nodes[2]).keyUp, "");
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
  sourceEndLine: webDoc.nodes[2].sourceEndLine,
  sourceEndColumn: webDoc.nodes[2].sourceEndColumn,
  sourceRef: tapSourceRef,
  sourceColumnRef: tapSourceColumnRef,
  sourceRangeRef: tapSourceRangeRef,
  id: "tap-button",
  domName: "",
  title: "Tap details",
  lang: "",
  dir: "",
  translate: "",
  dirname: "",
  placeholder: "",
  tabIndex: 3,
  domValue: "tap-value",
  href: "",
  target: "",
  rel: "",
  alt: "",
  asset: "",
  src: "",
  htmlFor: "",
  dataList: "",
  useMap: "",
  part: "",
  slot: "",
  inputType: "",
  formOwner: "",
  formAction: "",
  formMethod: "",
  formEncType: "",
  autoComplete: "",
  hidden: false,
  draggable: "",
  spellCheck: "",
  contentEditable: "",
  autoFocus: false,
  inert: false,
  autoCapitalize: "",
  enterKeyHint: "",
  download: "",
  formNoValidate: false,
  noValidate: false,
  clickable: false,
  popover: "",
  popoverTarget: "Scene/root/search_label",
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
  headers: "",
  scope: "",
  colSpan: "",
  rowSpan: "",
  classes: ["primary", "action"],
  dataAttrs: { "tracking-id": "tap-1" },
  ariaAttrs: { current: "page", pressed: "false" },
  extraAttrs: { fetchpriority: "high", part: "primary-action" },
  role: "button",
  ariaLabel: "Tap the action",
  ariaDescription: "Runs the host action",
  ariaDescribedBy: "",
  ariaDetails: "Scene/root/search_label",
  ariaErrorMessage: "Scene/root/search_label",
  ariaFlowTo: "search-box",
  ariaLabelledBy: "",
  ariaActiveDescendant: "",
  ariaControls: "search-box",
  ariaOwns: "search-box",
  ariaSort: "",
  ariaOrientation: "",
  ariaLevel: "",
  ariaPosInSet: "",
  ariaSetSize: "",
  ariaHasPopup: "",
  ariaMultiSelectable: "",
  ariaRowIndex: "",
  ariaColIndex: "",
  ariaRowCount: "",
  ariaColCount: "",
  ariaLive: "",
  state: {
    disabled: false,
    loading: false,
    selected: false,
    checked: false,
    invalid: false,
    valid: false,
    indeterminate: false,
    default: false,
    autofill: false,
    "placeholder-shown": false,
    expanded: false,
    open: false,
    hover: false,
    pressed: false,
    focus: false
  }
});
assert.deepEqual(runtime.webNodeStyleFacts(webDoc.nodes[2]), webDoc.nodes[2].styleFacts);
const buttonStyleTrace = runtime.traceWebStyle(webDoc.nodes[2], webStyleSheet);
assert.equal(buttonStyleTrace.facts.ref, "primary-action");
assert.equal(buttonStyleTrace.resolved.background, "#203040");
assert.equal(buttonStyleTrace.resolved.foreground, "#f0f0f0");
assert.equal(buttonStyleTrace.winners.background.value, "#203040");
assert.equal(buttonStyleTrace.winners.background.selector.includes("#tap-button"), true);
assert.equal(buttonStyleTrace.matchedRules.some((rule) =>
  rule.selector.includes("Button") && rule.style.foreground === "#f0f0f0"), true);
assert.equal(Object.keys(buttonStyleTrace).includes("matchedRules"), true);
assert.deepEqual(runtime.webNodeIdentity(webDoc.nodes[2]), {
  ref: "primary-action",
  aliases: [
    "primary-action",
    "Scene/root/tap",
    "tap",
    "tap-button",
    tapSourceRef,
    tapSourceColumnRef,
    tapSourceRangeRef
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
  sourceEndLine: webDoc.nodes[2].sourceEndLine,
  sourceEndColumn: webDoc.nodes[2].sourceEndColumn,
  sourceRef: tapSourceRef,
  sourceColumnRef: tapSourceColumnRef,
  sourceRangeRef: tapSourceRangeRef
});
const preMountButtonSnapshot = runtime.webNodeSnapshot(rt, "tap-button");
assert.equal(preMountButtonSnapshot.ref, "primary-action");
assert.equal(preMountButtonSnapshot.identity.domId, "tap-button");
assert.equal(preMountButtonSnapshot.parentRef, "Scene/root");
assert.deepEqual(preMountButtonSnapshot.childRefs, []);
assert.deepEqual(preMountButtonSnapshot.relationRefs.controls, ["search-box"]);
assert.deepEqual(runtime.webNodeRelationRefs(rt, "Scene/root/search_label").activeDescendantOf,
  ["search-box"]);
assert.equal(preMountButtonSnapshot.eventRefs.click, "call_host");
assert.equal(preMountButtonSnapshot.styleFacts.kind, "Button");
assert.equal(preMountButtonSnapshot.element, undefined);
assert.deepEqual(preMountButtonSnapshot.attrs, {});
assert.equal(preMountButtonSnapshot.rect, null);
assert.deepEqual(runtime.webNodeSnapshots(rt, "Button.primary").map((snapshot) => snapshot.ref),
  ["primary-action"]);
assert.deepEqual(runtime.resolveWebStyle(webDoc.nodes[2], webStyleSheet), {
  background: "#203040",
  "background-end": "#203850",
  foreground: "#f0f0f0",
  radius: 9,
  "padding-x": 13,
  "offset-y": 8,
  opacity: 0.75,
  "outline-width": 5
});
assert.equal(runtime.resolveWebStyle({
  ...webDoc.nodes[2],
  state: { ...webDoc.nodes[2].state, hover: true }
}, webStyleSheet)["outline-color"], "#607080");
assert.equal(runtime.resolveWebStyle({
  ...webDoc.nodes[2],
  state: { ...webDoc.nodes[2].state, hover: true, pressed: true }
}, webStyleSheet)["border-width"], 7);
assert.equal(runtime.resolveWebStyle({
  ...webDoc.nodes[2],
  state: { ...webDoc.nodes[2].state, pressed: true }
}, webStyleSheet)["outline-color"], "#708090");
assert.equal(runtime.resolveWebStyle({
  ...webDoc.nodes[2],
  state: { ...webDoc.nodes[2].state, focus: true }
}, webStyleSheet)["caret-color"], "#8090a0");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], runtime.parseWebStyleSheet(`
  TextField:valid {
    opacity: 0.66;
  }
`)).opacity, 0.66);
assert.equal(runtime.resolveWebStyle({
  ...webDoc.nodes[3],
  domValue: "",
  value: "",
  state: { ...webDoc.nodes[3].state, "placeholder-shown": false }
}, runtime.parseWebStyleSheet(`
  TextField:placeholder-shown {
    opacity: 0.61;
  }
`)).opacity, 0.61);
assert.equal(runtime.resolveWebStyle({
  kind: "Toggle",
  tag: "input",
  inputType: "checkbox",
  state: { indeterminate: true }
}, runtime.parseWebStyleSheet(`
  Toggle:indeterminate {
    opacity: 0.62;
  }
`)).opacity, 0.62);
assert.equal(runtime.resolveWebStyle({
  kind: "Radio",
  tag: "input",
  inputType: "radio",
  state: { default: true }
}, runtime.parseWebStyleSheet(`
  Radio:default {
    opacity: 0.63;
  }
`)).opacity, 0.63);
assert.equal(runtime.resolveWebStyle({
  ...webDoc.nodes[3],
  state: { ...webDoc.nodes[3].state, autofill: true }
}, runtime.parseWebStyleSheet(`
  TextField:autofill {
    opacity: 0.64;
  }
`)).opacity, 0.64);
assert.equal(runtime.webNodeQuery(rt, "Screen:has(> Button)").path, "Scene/root");
assert.equal(runtime.webNodeQuery(rt, "Text:has(+ Button)").path, webDoc.nodes[1].path);
assert.equal(runtime.webNodeQuery(rt, "Text:has(~ TextField)").path, webDoc.nodes[1].path);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[0], runtime.parseWebStyleSheet(`
  Screen:has(> Button) {
    opacity: 0.65;
  }
`)).opacity, 0.65);
const focusWithinRoot = { ...webDoc.nodes[0] };
const focusWithinButton = {
  ...webDoc.nodes[2],
  state: { ...webDoc.nodes[2].state, focus: true }
};
const focusWithinNodes = [focusWithinRoot, focusWithinButton];
focusWithinRoot.__kryFrameNodes = focusWithinNodes;
focusWithinButton.__kryFrameNodes = focusWithinNodes;
assert.equal(runtime.resolveWebStyle(focusWithinRoot, runtime.parseWebStyleSheet(`
  Screen:focus-within {
    opacity: 0.67;
  }
`)).opacity, 0.67);
runtime.ReplaceRoute("/#tap-button");
assert.equal(runtime.webNodeQuery(rt, "Button:target").path, "Scene/root/tap");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[2], runtime.parseWebStyleSheet(`
  Button:target {
    opacity: 0.68;
  }
`)).opacity, 0.68);
runtime.ReplaceRoute("/");
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
assert.equal(runtime.webNodeQuery(rt, "TextField:readonly").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "TextField:read-only").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "TextField:required").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "Button:enabled").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "Input:optional").path, webDoc.nodes[6].path);
assert.equal(runtime.webNodeQuery(rt, "TextField:valid").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "Screen:valid"), null);
assert.equal(runtime.webNodeQuery(rt, "[placeholder=\"Search terms\"]").path, "Scene/root/search");
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
assert.equal(runtime.webNodeQuery(rt, "[inert=true]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[autocapitalize=words]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[enterkeyhint=search]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[data-role]").path, "Scene/root/search");
assert.equal(runtime.webNodeQuery(rt, "[for=\"search-box\"]").path, "Scene/root/search_label");
assert.equal(runtime.webNodeQuery(rt, "[htmlFor=\"search-box\"]").path, "Scene/root/search_label");
assert.equal(runtime.webNodeQuery(rt, "[popover=manual]").path, "Scene/root/search_label");
assert.equal(runtime.webNodeQuery(rt, "[popoverTarget=\"Scene/root/search_label\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[fetchpriority=high]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[part=\"primary-action\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt,
  `[source="src/valid.kry"][line=${webDoc.nodes[2].sourceLine}][column=${webDoc.nodes[2].sourceColumn}]`).path,
  "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, `[sourceRef="${tapSourceRef}"]`).path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, `[sourceColumnRef="${tapSourceColumnRef}"]`).path,
  "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, `[sourceRangeRef="${tapSourceRangeRef}"]`).path,
  "Scene/root/tap");
assert.equal(runtime.webNodeMatches(rt, "primary-action", "Button.primary"), true);
assert.equal(runtime.webNodeQuery(rt, "[ref=\"primary-action\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[webRef=\"primary-action\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[aria-label=\"Tap the action\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[aria-description=\"Runs the host action\"]").path,
  "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[aria-controls=\"search-box\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[aria-owns=\"search-box\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[aria-details=\"Scene/root/search_label\"]").path,
  "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[aria-errormessage=\"Scene/root/search_label\"]").path,
  "Scene/root/tap");
assert.equal(runtime.webNodeQuery(rt, "[aria-flowto=\"search-box\"]").path, "Scene/root/tap");
assert.equal(runtime.webNodeMatches(rt, "Scene/root/tap", "Button.primary"), true);
assert.equal(runtime.webNodeMatches(rt, "Scene/root/tap", "TextField"), false);
assert.equal(webDoc.nodes.every((node) => !!node.path), true);
const selectablePath = webDoc.nodes[5].path;
assert.match(selectablePath, /^Scene\/root\/Selectable@\d+(?:-\d+)?$/);
assert.equal(runtime.webNodeQuery(rt, selectablePath).kind, "Selectable");
const inputPaths = webDoc.nodes
  .filter((node) => node.kind === "Input")
  .map((node) => node.path);
assert.deepEqual(runtime.webNodeQueryAll(rt, "[data.role=search]").map((node) => node.path), [
  "Scene/root/search"
]);
assert.equal(runtime.webNodeParent(rt, "Scene/root/tap").path, "Scene/root");
assert.deepEqual(runtime.webNodeChildren(rt, "Scene/root").map((node) => node.path), [
  webDoc.nodes[1].path,
  "Scene/root/tap",
  "Scene/root/search",
  "Scene/root/search_label",
  selectablePath,
  ...inputPaths
]);
assert.deepEqual(runtime.webNodeChildren(rt).map((node) => node.path), ["Scene/root"]);
assert.deepEqual(runtime.webNodeDescendants(rt, "Scene/root").map((node) => node.path), [
  webDoc.nodes[1].path,
  "Scene/root/tap",
  "Scene/root/search",
  "Scene/root/search_label",
  selectablePath,
  ...inputPaths
]);
assert.equal(runtime.webNodeQueryWithin(rt, "Scene/root", "Button.primary").path,
  "Scene/root/tap");
assert.deepEqual(runtime.webNodeQueryAllWithin(rt, "Scene/root", "Input")
  .map((node) => node.path), inputPaths);
assert.equal(runtime.webNodeQueryWithin(rt, "Scene/root/tap", "TextField"), null);
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
assert.equal(webDoc.nodes[3].inert, true);
assert.equal(webDoc.nodes[3].autoCapitalize, "words");
assert.equal(webDoc.nodes[3].enterKeyHint, "search");
assert.equal(webDoc.nodes[3].formNoValidate, true);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).gap, 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-x"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-x"], 13);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-y"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-left"], 14);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-right"], 15);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-top"], 16);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-bottom"], 17);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-left"], 18);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-right"], 19);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-top"], 20);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-bottom"], 21);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-inline"], 22);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-block"], 23);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-inline-start"], 24);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-inline-end"], 25);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-block-start"], 26);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["padding-block-end"], 27);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-inline"], 28);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-block"], 29);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-inline-start"], 30);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-inline-end"], 31);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-block-start"], 32);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["margin-block-end"], 33);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["min-width"], 44);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["max-height"], 55);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["inline-size"], 66);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["block-size"], 77);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["min-inline-size"], 88);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["max-inline-size"], 99);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["min-block-size"], 111);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["max-block-size"], 122);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).inset, 1);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).top, 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).right, 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).bottom, 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).left, 5);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["inset-inline"], 6);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["inset-block"], 7);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["inset-inline-start"], 8);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["inset-inline-end"], 9);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["inset-block-start"], 10);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["inset-block-end"], 11);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-y"], 6);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["content-offset-y"], 7);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).radius, 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["content-offset-x"], 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-size"], 11);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).typeface, "ui-sans-serif");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-weight"], 600);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-style"], "italic");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variant"], "small-caps");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variant-alternates"], "historical-forms");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variant-caps"], "small-caps");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variant-east-asian"], "ruby");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variant-ligatures"], "common-ligatures");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variant-numeric"], "tabular-nums");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variant-position"], "sub");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-language-override"], "\"TRK\"");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-palette"], "light");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-stretch"], "condensed");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-kerning"], "normal");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-optical-sizing"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-feature-settings"], "\"kern\" 1");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-variation-settings"], "\"wght\" 600");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-size-adjust"], 0.5);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-synthesis"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-synthesis-weight"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-synthesis-style"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-synthesis-small-caps"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["font-synthesis-position"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["line-height"], 1.4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["letter-spacing"], 1);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-indent"], 12);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-align"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-align-last"], "end");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-rendering"], "optimizeLegibility");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-decoration"], "underline");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-decoration-line"], "underline overline");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-decoration-color"], "#667788");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-decoration-style"], "wavy");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-decoration-skip"], "spaces");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-decoration-skip-ink"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-decoration-thickness"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-underline-offset"], 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-underline-position"], "under");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-shadow"], "0 1px 2px #0004");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-emphasis"], "dot");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-emphasis-color"], "#223344");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-emphasis-style"], "filled sesame");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-emphasis-position"], "over right");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-transform"], "uppercase");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-overflow"], "ellipsis");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["white-space"], "nowrap");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-size-adjust"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-orientation"], "mixed");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-wrap"], "balance");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-wrap-mode"], "wrap");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-wrap-style"], "pretty");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-justify"], "inter-word");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["text-combine-upright"], "digits 2");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["ruby-align"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["ruby-position"], "over");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["word-break"], "keep-all");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overflow-wrap"], "anywhere");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["word-wrap"], "break-word");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["line-break"], "strict");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["hanging-punctuation"], "first");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["vertical-align"], 5);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).direction, "rtl");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["writing-mode"], "vertical-rl");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["tab-size"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).hyphens, "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["line-clamp"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["list-style"], "square inside");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["list-style-type"], "square");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["list-style-position"], "inside");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["list-style-image"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["counter-reset"], "section 2");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["counter-increment"], "section");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["counter-set"], "item 4");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).quotes, "\"<<\" \">>\"");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["marker-side"], "match-parent");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["marker-start"], "open");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["marker-end"], "close");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).orphans, 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).widows, 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["box-decoration-break"], "clone");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-collapse"], "collapse");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-spacing"], 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["table-layout"], "fixed");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["caption-side"], "bottom");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["empty-cells"], "hide");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).display, "flex");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).position, "relative");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["z-index"], 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overflow-inline"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overflow-block"], "hidden");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overflow-x"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overflow-y"], "hidden");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-behavior"], "smooth");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overscroll-behavior"], "contain");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overscroll-behavior-x"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overscroll-behavior-y"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overscroll-behavior-inline"], "contain");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["overscroll-behavior-block"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-snap-type"], "x mandatory");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-snap-align"], "start center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-snap-stop"], "always");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scrollbar-color"], "#223344 #ddeeff");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scrollbar-width"], "thin");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scrollbar-gutter"], "stable both-edges");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-margin"], 12);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-margin-top"], 13);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-margin-inline-start"], 19);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-margin-block-end"], 22);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-padding"], 23);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-padding-left"], 27);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-padding-inline"], 28);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["scroll-padding-block-end"], 33);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["touch-action"], "manipulation");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["box-sizing"], "border-box");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["align-items"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["justify-content"], "space-between");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["align-self"], "stretch");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["justify-self"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["flex-direction"], "column");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["flex-wrap"], "wrap");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).flex, "1 1 auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["flex-grow"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["flex-shrink"], 0);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["flex-basis"], 13);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-template-columns"], "repeat(2, minmax(0, 1fr))");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-template-rows"], "auto 1fr");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-template-areas"], "\"header header\" \"nav main\"");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-auto-columns"], "minmax(8px, auto)");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-auto-rows"], 24);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-auto-flow"], "row dense");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-column"], "1 / span 2");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-column-start"], 1);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-column-end"], 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-area"], "main");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-row"], "2 / span 1");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-row-start"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["grid-row-end"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["row-gap"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-gap"], 6);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["align-content"], "stretch");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["justify-items"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["place-items"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["place-content"], "stretch");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["place-self"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["object-fit"], "contain");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["object-position"], "center top");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["object-view-box"], "inset(10% 20% 30% 40%)");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["aspect-ratio"], "16 / 9");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["image-rendering"], "pixelated");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["image-orientation"], "from-image");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["image-resolution"], "300dpi");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-size"], "cover");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-position"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-position-x"], "left");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-position-y"], "top");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-repeat"], "no-repeat");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-repeat-x"], "repeat");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-repeat-y"], "no-repeat");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-clip"], "padding-box");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-origin"], "border-box");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-attachment"], "fixed");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["background-blend-mode"], "multiply");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).visibility, "visible");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).transition, "opacity 120ms ease");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["transition-property"], "opacity, transform");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["transition-duration"], "120ms");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["transition-timing-function"], "ease-in-out");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["transition-delay"], "20ms");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).animation, "fade-in 200ms ease both");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-name"], "fade-in");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-duration"], "200ms");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-timing-function"], "ease");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-delay"], "10ms");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-iteration-count"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-direction"], "alternate");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-fill-mode"], "both");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["animation-play-state"], "running");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).transform, "scale(1.1)");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["transform-origin"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-path"], "path(\"M 0 0 L 10 10\")");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-distance"], "50%");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-rotate"], "auto 45deg");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-anchor"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["offset-position"], "normal");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).filter, "contrast(1.1)");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["backdrop-filter"], "blur(2px)");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["clip-path"], "inset(0 round 4px)");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-image"], "linear-gradient(#000, transparent)");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-size"], "cover");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-position"], "center");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-repeat"], "no-repeat");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-origin"], "border-box");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-clip"], "padding-box");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-composite"], "exclude");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mask-mode"], "alpha");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-style"], "dashed");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-top-style"], "solid");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-right-style"], "dotted");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-bottom-style"], "double");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-left-style"], "groove");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-style"], "ridge");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-style"], "inset");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-start-style"], "outset");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-end-style"], "hidden");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-start-style"], "none");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-end-style"], "solid");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-color"], "#101112");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-top-color"], "#111213");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-right-color"], "#121314");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-bottom-color"], "#131415");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-left-color"], "#141516");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-color"], "#151617");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-color"], "#161718");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-start-color"], "#171819");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-end-color"], "#18191a");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-start-color"], "#191a1b");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-end-color"], "#1a1b1c");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-top-width"], 22);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-right-width"], 23);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-bottom-width"], 24);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-left-width"], 25);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-width"], 30);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-width"], 31);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-start-width"], 32);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-inline-end-width"], 33);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-start-width"], 34);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-block-end-width"], 35);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-top-left-radius"], 26);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-top-right-radius"], 27);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-bottom-right-radius"], 28);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-bottom-left-radius"], 29);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-start-start-radius"], 36);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-start-end-radius"], 37);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-end-start-radius"], 38);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["border-end-end-radius"], 39);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).cursor, "pointer");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["pointer-events"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).outline, "2px solid #445566");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["outline-width"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["outline-offset"], 3);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["outline-style"], "solid");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["outline-color"], "#445566");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["box-shadow"], "0 1px 2px #0004");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["color-scheme"], "light dark");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).contain, "layout paint");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).container, "search / inline-size");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["container-type"], "inline-size");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["container-name"], "search");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["will-change"], "transform");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).isolation, "isolate");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["mix-blend-mode"], "multiply");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).columns, "2 auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-count"], 2);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-width"], 180);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-fill"], "balance");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-span"], "all");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-rule"], "1px solid #ccc");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-rule-color"], "#ccddee");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-rule-style"], "dashed");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["column-rule-width"], 4);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["break-before"], "avoid");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["break-after"], "auto");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet)["break-inside"], "avoid");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).float, "inline-start");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).clear, "both");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], webStyleSheet).order, 2);
const readonlyRequiredSheet = runtime.parseWebStyleSheet(`
  TextField:readonly { color: #111111; }
  TextField:required { outline-color: #333333; }
`);
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], readonlyRequiredSheet).color, "#111111");
assert.equal(runtime.resolveWebStyle(webDoc.nodes[3], readonlyRequiredSheet)["outline-color"], "#333333");
assert.deepEqual(webDoc.nodes[3].classes, ["field"]);
assert.equal(webDoc.nodes[3].placeholder, "Search terms");
assert.equal(webDoc.nodes[3].ariaLabel, "Search");
assert.equal(webDoc.nodes[3].ariaLabelledBy, "Scene/root/search_label");
assert.equal(webDoc.nodes[3].ariaActiveDescendant, "Scene/root/search_label");
assert.equal(webDoc.nodes[3].ariaDescribedBy, "primary-action");
assert.equal(webDoc.nodes[3].ariaDetails, "Scene/root/search_label");
assert.equal(webDoc.nodes[3].ariaErrorMessage, "Scene/root/search_label");
assert.equal(webDoc.nodes[3].ariaFlowTo, "primary-action");
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
      indeterminate: false,
      open: false,
      inert: false,
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
      querySelector(selector) {
        const styleMatch = String(selector || "").match(/^style\[data-kry-style="([^"]+)"\]$/);
        if (styleMatch)
          return this.children.find((child) =>
            child.tagName === "STYLE" && child.attributes["data-kry-style"] === styleMatch[1]) || null;
        return null;
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
      dblclick() { if (this.ondblclick) this.ondblclick(); },
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
      keyup(key) { if (this.onkeyup) this.onkeyup({ key }); },
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
      pointerenter() { if (this.onpointerenter) this.onpointerenter(); },
      pointerleave() { if (this.onpointerleave) this.onpointerleave(); },
      pointermove() { if (this.onpointermove) this.onpointermove(); },
      pointerdown() { if (this.onpointerdown) this.onpointerdown(); },
      pointerup() { if (this.onpointerup) this.onpointerup(); },
      pointercancel() { if (this.onpointercancel) this.onpointercancel(); },
      wheel(deltaY) { if (this.onwheel) this.onwheel({ deltaY }); },
      contextmenu() {
        const event = { defaultPrevented: false, preventDefault() { this.defaultPrevented = true; } };
        if (this.oncontextmenu)
          this.oncontextmenu(event);
        return event;
      },
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
      const styleMatch = String(selector || "").match(/^style\[data-kry-style="([^"]+)"\]$/);
      if (styleMatch)
        return head.children.find((child) =>
          child.tagName === "STYLE" && child.attributes["data-kry-style"] === styleMatch[1]) || null;
      return null;
    }
  };
}

{
  const previousDocument = globalThis.document;
  globalThis.document = fakeDocument();
  try {
    const removeInstalledStyle = runtime.installWebStyleSheet(webStyleSheet, null, "smoke");
    assert.equal(typeof removeInstalledStyle, "function");
    assert.equal(document.head.children.length, 1);
    assert.equal(document.head.children[0].tagName, "STYLE");
    assert.equal(document.head.children[0].attributes["data-kry-style"], "smoke");
    assert.match(document.head.children[0].textContent, /background: #102030;/);
    const replaceInstalledStyle = runtime.installWebStyleSheet(`
      Button.primary { foreground: #abcdef; }
    `, null, "smoke");
    assert.equal(typeof replaceInstalledStyle, "function");
    assert.equal(document.head.children.length, 1);
    assert.match(document.head.children[0].textContent, /color: #abcdef;/);
    replaceInstalledStyle();
    assert.equal(document.head.children.length, 0);
    removeInstalledStyle();
    assert.equal(document.head.children.length, 0);
    const appSheets = runtime.loadAppWebStyleSheets(generated.app);
    assert.equal(appSheets.length, 4);
    assert.deepEqual(appSheets.map((sheet) => sheet.pack),
      ["material", "local.brand", "brand", "acme.dark"]);
    const removeAppStyles = runtime.installAppWebStyleSheets(generated.app, null, "valid-app");
    assert.equal(typeof removeAppStyles, "function");
    assert.equal(document.head.children.length, 1);
    assert.equal(document.head.children[0].attributes["data-kry-style"], "valid-app");
    assert.match(document.head.children[0].textContent, /\[data-kry-kind="Button"\]\.primary/);
    removeAppStyles();
    assert.equal(document.head.children.length, 0);
    const removeRichAppStyles = runtime.installAppWebStyleSheets({
      title: "Rich App Styles",
      styles: [{
        source: `
          @keyframes app-fade { from { opacity: 0; } to { opacity: 1; } }
          @media (min-width: 600px) { Button.primary { display: flex; } }
          @supports (display: grid) { Screen { display: grid; } }
          @container (min-width: 300px) { TextField.field { padding-inline: 8; } }
        `
      }]
    }, null, "rich-app");
    assert.equal(typeof removeRichAppStyles, "function");
    assert.match(document.head.children[0].textContent, /@keyframes app-fade \{/);
    assert.match(document.head.children[0].textContent, /@media \(min-width: 600px\)/);
    assert.match(document.head.children[0].textContent, /@supports \(display: grid\)/);
    assert.match(document.head.children[0].textContent, /@container \(min-width: 300px\)/);
    removeRichAppStyles();
    assert.equal(document.head.children.length, 0);

    const ariaRt = runtime.createRuntime();
    runtime.beginFrame(ariaRt);
    runtime.widget(ariaRt, "Heading", { level: 2, text: "Welcome" }, null,
      { nodeName: "welcome", path: "Page/welcome" });
    runtime.widget(ariaRt, "Link", { href: "/docs", text: "Docs", selected: true }, null,
      { nodeName: "docs", path: "Page/docs", href: "/reference", target: "_blank", rel: "noopener" });
    runtime.widget(ariaRt, "Checkbox", { checked: true, disabled: true, loading: true }, null,
      { nodeName: "accept", path: "Page/accept" });
    runtime.widget(ariaRt, "Progress", { min: 0, max: 100, value: 42, label: "Loading" }, null,
      { nodeName: "load", path: "Page/load" });
    runtime.widget(ariaRt, "Separator", {}, null,
      { nodeName: "break", path: "Page/break" });
    runtime.endFrame(ariaRt);
    const snapshot = runtime.webAccessibilitySnapshot(ariaRt);
    assert.equal(snapshot.nodes[0].role, "heading");
    assert.equal(snapshot.nodes[0].level, 2);
    assert.equal(snapshot.nodes[1].role, "link");
    assert.equal(snapshot.nodes[1].href, "/reference");
    assert.equal(snapshot.nodes[2].role, "checkbox");
    assert.equal(snapshot.nodes[2].inputType, "checkbox");
    assert.equal(snapshot.nodes[2].state.checked, true);
    assert.equal(snapshot.nodes[3].role, "progressbar");
    assert.equal(snapshot.nodes[3].value, "42");
    assert.equal(snapshot.nodes[3].min, "0");
    assert.equal(snapshot.nodes[3].max, "100");
    assert.equal(snapshot.nodes[3].valueNow, "42");
    const target = document.createElement("div");
    runtime.renderWebDocument(ariaRt, target);
    const root = target.children[0];
    const link = runtime.findWebElement(target, "Page/docs");
    const checkbox = runtime.findWebElement(target, "Page/accept");
    const progress = runtime.findWebElement(target, "Page/load");
    assert.equal(link.attributes["aria-current"], "page");
    assert.equal(link.attributes.href, "/reference");
    assert.equal(link.attributes.target, "_blank");
    assert.equal(link.attributes.rel, "noopener");
    assert.equal(checkbox.attributes["aria-checked"], "true");
    assert.equal(checkbox.attributes.checked, "");
    assert.equal(checkbox.attributes["aria-disabled"], "true");
    assert.equal(checkbox.attributes["aria-busy"], "true");
    assert.equal(progress.tagName, "PROGRESS");
    assert.equal(progress.attributes.min, "0");
    assert.equal(progress.attributes.max, "100");
    assert.equal(progress.attributes.value, "42");
    assert.equal(progress.textContent, "Loading");
    assert.equal(runtime.webFormValue(target, "accept"), true);
    const separator = runtime.findWebElement(target, "Page/break");
    assert.equal(separator.tagName, "HR");
    assert.equal(root.children.length, 5);

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
    runtime.widget(submitRt, "TextField", { text: "outside@example.test" }, null,
      { nodeName: "externalEmail", path: "Page/externalEmail", domName: "external_email", formOwner: "contact" });
    runtime.widget(submitRt, "TextField", { text: "loose@example.test" }, null,
      { nodeName: "looseEmail", path: "Page/looseEmail", parentPath: "Page", domName: "loose_email" });
    runtime.widget(submitRt, "Column", {}, null,
      { nodeName: "newsletterLabel", path: "Page/newsletterLabel", tag: "label", webRef: "newsletterLabel" });
    runtime.widget(submitRt, "Checkbox", { checked: true }, null,
      { nodeName: "newsletterOptIn", path: "Page/newsletterLabel/optIn", parentPath: "Page/newsletterLabel" });
    runtime.endFrame(submitRt);
    assert.equal(runtime.webNodeQuery(submitRt, "[form=contact]").path, "Page/externalEmail");
    assert.equal(runtime.webNodeRelations(submitRt, "newsletterLabel").labelFor.path,
      "Page/newsletterLabel/optIn");
    assert.deepEqual(runtime.webNodeRelationRefs(submitRt, "newsletterOptIn").labelledBy,
      ["Page/newsletterLabel"]);
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
    const externalEmail = runtime.findWebElement(submitTarget, "external_email");
    assert.equal(externalEmail.attributes.form, "kry-Page-contact");
    assert.equal(runtime.webDOMQuery(submitTarget, "[form=contact]").element, externalEmail);
    assert.equal(runtime.webDOMRelations(submitTarget, "external_email").formOwner.ref, "Page/contact");
    assert.equal(runtime.webDOMSnapshot(submitTarget, "external_email").relationRefs.formOwner, "Page/contact");
    assert.deepEqual(runtime.webDOMRelations(submitTarget, "contact").formControls
      .map((object) => object.ref), ["Page/externalEmail", "Page/contact/email"]);
    assert.deepEqual(runtime.webDOMSnapshot(submitTarget, "contact").relationRefs.formControls,
      ["Page/externalEmail", "Page/contact/email"]);
    assert.deepEqual(runtime.webNodeRelations(submitRt, "contact").formControls
      .map((node) => node.path), ["Page/externalEmail", "Page/contact/email"]);
    assert.deepEqual(runtime.webNodeRelationRefs(submitRt, "contact").formControls,
      ["Page/externalEmail", "Page/contact/email"]);
    assert.equal(runtime.webDOMRelations(submitTarget, "newsletterLabel").labelFor.ref,
      "Page/newsletterLabel/optIn");
    assert.deepEqual(runtime.webDOMSnapshot(submitTarget, "newsletterOptIn").relationRefs.labelledBy,
      ["Page/newsletterLabel"]);
    assert.equal(runtime.webFormValues(submitTarget, "contact").external_email, "outside@example.test");
    assert.equal(runtime.webFormValues(submitTarget, "contact").loose_email, undefined);
    assert.equal(runtime.webDOMQuery(submitTarget, "[action=\"/contact\"]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[method=post]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[enctype=\"multipart/form-data\"]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[autocomplete=off]").element, submitForm);
    assert.equal(runtime.webDOMQuery(submitTarget, "[novalidate]").element, submitForm);
    assert.equal(submitForm.dataset.kryOnReset, "clear_contact");
    assert.equal(runtime.webDOMSubmit(submitTarget, "contact"), true);
    assert.equal(submitValues.email, "hello@example.test");
    assert.equal(submitValues.external_email, "outside@example.test");
    assert.equal(submitValues.loose_email, undefined);
    assert.equal(submitValues["Page/contact/email"], "hello@example.test");
    assert.equal(submitForm.krySubmit(), true);
    assert.equal(submitValues.email, "hello@example.test");
    assert.equal(submitValues.external_email, "outside@example.test");
    assert.equal(runtime.webDOMReset(submitTarget, "Page/contact"), true);
    assert.equal(resetValues.email, "hello@example.test");
    assert.equal(resetValues.external_email, "outside@example.test");
    assert.equal(resetValues.loose_email, undefined);
    assert.equal(resetValues["Page/contact/email"], "hello@example.test");
    assert.equal(submitForm.kryReset(), true);
    assert.equal(resetValues.email, "hello@example.test");
    assert.equal(resetValues.external_email, "outside@example.test");

    const tableRt = runtime.createRuntime();
    runtime.beginFrame(tableRt);
    runtime.widget(tableRt, "TableView", {}, null,
      { nodeName: "prices", path: "Page/prices" });
    runtime.widget(tableRt, "TableCell", { text: "Price", scope: "col" }, null,
      {
        nodeName: "priceHeader",
        path: "Page/prices/priceHeader",
        parentPath: "Page/prices",
        id: "price-header",
        ariaSort: "ascending",
        ariaColIndex: 1,
        ariaColCount: 2
      });
    runtime.widget(tableRt, "TableCell", { text: "Product" }, null,
      {
        nodeName: "productHeader",
        path: "Page/prices/productHeader",
        parentPath: "Page/prices",
        id: "product-header",
        scope: "row",
        ariaRowIndex: 2
      });
    runtime.widget(tableRt, "TableCell", { text: "Quarter", scope: "colgroup" }, null,
      {
        nodeName: "quarterHeader",
        path: "Page/prices/quarterHeader",
        parentPath: "Page/prices",
        id: "quarter-header"
      });
    runtime.widget(tableRt, "TableCell", { text: "Region", scope: "rowgroup" }, null,
      {
        nodeName: "regionHeader",
        path: "Page/prices/regionHeader",
        parentPath: "Page/prices",
        id: "region-header"
      });
    runtime.widget(tableRt, "Text", { text: "$12" }, null,
      {
        nodeName: "priceCell",
        path: "Page/prices/priceCell",
        parentPath: "Page/prices",
        tag: "td",
        headers: "priceHeader productHeader quarterHeader regionHeader",
        colSpan: "2",
        rowSpan: "1",
        ariaRowIndex: 2,
        ariaColIndex: 1,
        ariaRowCount: 4
      });
    runtime.endFrame(tableRt);
    assert.equal(runtime.webNodeQuery(tableRt, "[scope=col]").path,
      "Page/prices/priceHeader");
    assert.equal(runtime.webNodeQuery(tableRt, "[aria-sort=ascending]").path,
      "Page/prices/priceHeader");
    assert.equal(runtime.webNodeQuery(tableRt, "[aria-colindex=1]").path,
      "Page/prices/priceHeader");
    assert.equal(runtime.webNodeQuery(tableRt, "[aria-colcount=2]").path,
      "Page/prices/priceHeader");
    assert.equal(runtime.webNodeQuery(tableRt, "[headers~=priceHeader]").path,
      "Page/prices/priceCell");
    assert.equal(runtime.webNodeQuery(tableRt, "[colspan=2]").path,
      "Page/prices/priceCell");
    assert.equal(runtime.webNodeQuery(tableRt, "[rowspan=1]").path,
      "Page/prices/priceCell");
    assert.equal(runtime.webNodeQuery(tableRt, "[headers~=priceHeader][aria-rowindex=2]").path,
      "Page/prices/priceCell");
    assert.equal(runtime.webNodeQuery(tableRt, "[aria-rowcount=4]").path,
      "Page/prices/priceCell");
    const tableTarget = document.createElement("div");
    runtime.renderWebDocument(tableRt, tableTarget);
    const priceHeader = runtime.findWebElement(tableTarget, "priceHeader");
    const productHeader = runtime.findWebElement(tableTarget, "productHeader");
    const priceCell = runtime.findWebElement(tableTarget, "priceCell");
    assert.equal(runtime.webNodeQuery(tableRt, "Page/prices/priceHeader").tag, "th");
    assert.equal(runtime.webNodeQuery(tableRt, "Page/prices/priceHeader").text, "Price");
    assert.equal(runtime.webNodeQuery(tableRt, "Page/prices/productHeader").tag, "th");
    assert.equal(runtime.webNodeQuery(tableRt, "Page/prices/productHeader").text, "Product");
    assert.equal(priceHeader.tagName, "TH");
    assert.equal(priceHeader.textContent, "Price");
    assert.equal(priceHeader.attributes.scope, "col");
    assert.equal(priceHeader.attributes["aria-sort"], "ascending");
    assert.equal(priceHeader.attributes["aria-colindex"], "1");
    assert.equal(priceHeader.attributes["aria-colcount"], "2");
    assert.equal(productHeader.tagName, "TH");
    assert.equal(productHeader.textContent, "Product");
    assert.equal(productHeader.attributes.scope, "row");
    assert.equal(productHeader.attributes["aria-rowindex"], "2");
    assert.equal(priceCell.attributes.headers,
      "price-header product-header quarter-header region-header");
    assert.equal(priceCell.attributes.colspan, "2");
    assert.equal(priceCell.attributes.rowspan, "1");
    assert.equal(priceCell.attributes["aria-rowindex"], "2");
    assert.equal(priceCell.attributes["aria-colindex"], "1");
    assert.equal(priceCell.attributes["aria-rowcount"], "4");
    assert.equal(runtime.webAccessibilitySnapshot(tableRt).nodes
      .find((node) => node.path === "Page/prices/priceHeader")?.role,
      "columnheader");
    assert.equal(runtime.webAccessibilitySnapshot(tableRt).nodes
      .find((node) => node.path === "Page/prices/priceHeader")?.label,
      "Price");
    assert.equal(runtime.webAccessibilitySnapshot(tableRt).nodes
      .find((node) => node.path === "Page/prices/productHeader")?.role,
      "rowheader");
    assert.equal(runtime.webAccessibilitySnapshot(tableRt).nodes
      .find((node) => node.path === "Page/prices/productHeader")?.label,
      "Product");
    assert.deepEqual(runtime.webAccessibilitySnapshot(tableRt).nodes
      .find((node) => node.kind === "Text" && node.path === "Page/prices/priceCell"),
      {
        path: "Page/prices/priceCell",
        sourcePath: "",
        sourceLine: 0,
        sourceColumn: 0,
        sourceEndLine: 0,
        sourceEndColumn: 0,
        name: "priceCell",
        kind: "Text",
        tag: "td",
        id: "",
        classes: [],
        role: "",
        label: "$12",
        description: "",
        text: "$12",
        value: "",
        min: "",
        max: "",
        valueNow: "",
        href: "",
        alt: "",
        asset: "",
        src: "",
        inputType: "",
        level: 0,
        rowIndex: "2",
        colIndex: "1",
        rowCount: "4",
        colCount: "",
        state: {
          disabled: false,
          loading: false,
          selected: false,
          checked: false,
          invalid: false,
          valid: false,
          indeterminate: false,
          default: false,
          autofill: false,
          "placeholder-shown": false,
          expanded: false,
          open: false,
          hover: false,
          pressed: false,
          focus: false
        }
      });
    assert.equal(runtime.webDOMRelations(tableTarget, "priceCell").headers[0].ref,
      "Page/prices/priceHeader");
    assert.deepEqual(runtime.webDOMRelations(tableTarget, "priceHeader").headerFor
      .map((object) => object.ref), ["Page/prices/priceCell"]);
    assert.equal(runtime.webDOMRelations(tableTarget, "priceCell").columnHeaders[0].ref,
      "Page/prices/priceHeader");
    assert.equal(runtime.webDOMRelations(tableTarget, "priceCell").rowHeaders[0].ref,
      "Page/prices/productHeader");
    assert.deepEqual(runtime.webDOMRelations(tableTarget, "priceCell").columnHeaders
      .map((object) => object.ref),
      ["Page/prices/priceHeader", "Page/prices/quarterHeader"]);
    assert.deepEqual(runtime.webDOMRelations(tableTarget, "priceCell").rowHeaders
      .map((object) => object.ref),
      ["Page/prices/productHeader", "Page/prices/regionHeader"]);
    assert.deepEqual(runtime.webDOMRelations(tableTarget, "priceCell").columnGroupHeaders
      .map((object) => object.ref),
      ["Page/prices/quarterHeader"]);
    assert.deepEqual(runtime.webDOMRelations(tableTarget, "priceCell").rowGroupHeaders
      .map((object) => object.ref),
      ["Page/prices/regionHeader"]);
    assert.deepEqual(runtime.webNodeRelationRefs(tableRt, "priceCell").headers,
      ["Page/prices/priceHeader", "Page/prices/productHeader",
       "Page/prices/quarterHeader", "Page/prices/regionHeader"]);
    assert.deepEqual(runtime.webNodeRelations(tableRt, "priceCell").headers
      .map((node) => node.path),
      ["Page/prices/priceHeader", "Page/prices/productHeader",
       "Page/prices/quarterHeader", "Page/prices/regionHeader"]);
    assert.deepEqual(runtime.webNodeRelations(tableRt, "priceHeader").headerFor
      .map((node) => node.path), ["Page/prices/priceCell"]);
    assert.deepEqual(runtime.webNodeRelationRefs(tableRt, "priceHeader").headerFor,
      ["Page/prices/priceCell"]);
    assert.deepEqual(runtime.webNodeRelationRefs(tableRt, "priceCell").columnGroupHeaders,
      ["Page/prices/quarterHeader"]);
    assert.deepEqual(runtime.webDOMSnapshot(tableTarget, "priceCell").relationRefs.headers,
      ["Page/prices/priceHeader", "Page/prices/productHeader",
       "Page/prices/quarterHeader", "Page/prices/regionHeader"]);
    assert.deepEqual(runtime.webDOMSnapshot(tableTarget, "priceHeader").relationRefs.headerFor,
      ["Page/prices/priceCell"]);
    assert.deepEqual(runtime.webDOMSnapshot(tableTarget, "priceCell").relationRefs.columnHeaders,
      ["Page/prices/priceHeader", "Page/prices/quarterHeader"]);
    assert.deepEqual(runtime.webDOMSnapshot(tableTarget, "priceCell").relationRefs.rowHeaders,
      ["Page/prices/productHeader", "Page/prices/regionHeader"]);
    assert.deepEqual(runtime.webDOMSnapshot(tableTarget, "priceCell").relationRefs.columnGroupHeaders,
      ["Page/prices/quarterHeader"]);
    assert.deepEqual(runtime.webDOMSnapshot(tableTarget, "priceCell").relationRefs.rowGroupHeaders,
      ["Page/prices/regionHeader"]);
    assert.deepEqual(runtime.webDOMRelationRefs(tableTarget, "priceCell").headers,
      ["Page/prices/priceHeader", "Page/prices/productHeader",
       "Page/prices/quarterHeader", "Page/prices/regionHeader"]);

    const menuRt = runtime.createRuntime();
    runtime.beginFrame(menuRt);
    runtime.widget(menuRt, "Menu", {}, null,
      {
        nodeName: "choices",
        path: "Page/choices",
        ariaOrientation: "vertical",
        ariaMultiSelectable: true
      });
    runtime.widget(menuRt, "Button", { label: "One" }, null,
      {
        nodeName: "choiceOne",
        path: "Page/choices/choiceOne",
        parentPath: "Page/choices",
        role: "menuitem",
        ariaLevel: 2,
        ariaPosInSet: 1,
        ariaSetSize: 3,
        ariaHasPopup: "menu"
      });
    runtime.widget(menuRt, "Button", { label: "Two" }, null,
      {
        nodeName: "choiceTwo",
        path: "Page/choices/choiceTwo",
        parentPath: "Page/choices"
      });
    runtime.widget(menuRt, "Selectable", { label: "Three" }, null,
      {
        nodeName: "choiceThree",
        path: "Page/choices/choiceThree",
        parentPath: "Page/choices"
      });
    runtime.endFrame(menuRt);
    assert.equal(runtime.webNodeQuery(menuRt, "[aria-orientation=vertical]").path,
      "Page/choices");
    assert.equal(runtime.webNodeQuery(menuRt, "[aria-multiselectable=true]").path,
      "Page/choices");
    assert.equal(runtime.webNodeQuery(menuRt, "[aria-level=2]").path,
      "Page/choices/choiceOne");
    assert.equal(runtime.webNodeQuery(menuRt, "[aria-posinset=1]").path,
      "Page/choices/choiceOne");
    assert.equal(runtime.webNodeQuery(menuRt, "[aria-setsize=3]").path,
      "Page/choices/choiceOne");
    assert.equal(runtime.webNodeQuery(menuRt, "[aria-haspopup=menu]").path,
      "Page/choices/choiceOne");
    assert.equal(runtime.webNodeQuery(menuRt, "Page/choices/choiceTwo").role,
      "menuitem");
    assert.equal(runtime.webNodeStyleFacts(runtime.webNodeQuery(menuRt,
      "Page/choices/choiceTwo")).role, "menuitem");
    assert.equal(runtime.webNodeQuery(menuRt, "Page/choices/choiceThree").tag,
      "button");
    assert.equal(runtime.webNodeQuery(menuRt, "Page/choices/choiceThree").role,
      "menuitem");
    assert.deepEqual(runtime.webNodeRelationRefs(menuRt, "Page/choices").collectionItems,
      ["Page/choices/choiceOne", "Page/choices/choiceTwo", "Page/choices/choiceThree"]);
    assert.equal(runtime.webNodeRelationRefs(menuRt, "Page/choices/choiceTwo").collectionOwner,
      "Page/choices");
    assert.equal(runtime.webNodeRelationRefs(menuRt, "Page/choices/choiceTwo").activeCollectionOwner,
      "");

    const treeItemRt = runtime.createRuntime();
    runtime.beginFrame(treeItemRt);
    runtime.widget(treeItemRt, "TreeView", {}, null,
      { nodeName: "tree", path: "Page/tree", ariaActiveDescendant: "Page/tree/branch" });
    runtime.widget(treeItemRt, "Selectable", { label: "Branch" }, null,
      { nodeName: "branch", path: "Page/tree/branch", parentPath: "Page/tree" });
    runtime.widget(treeItemRt, "Button", { label: "Leaf" }, null,
      { nodeName: "leaf", path: "Page/tree/leaf", parentPath: "Page/tree" });
    runtime.widget(treeItemRt, "Button", { label: "Custom" }, null,
      { nodeName: "custom", path: "Page/tree/custom", parentPath: "Page/tree", role: "button" });
    runtime.endFrame(treeItemRt);
    assert.equal(runtime.webNodeQuery(treeItemRt, "Page/tree/branch").role,
      "treeitem");
    assert.equal(runtime.webNodeQuery(treeItemRt, "Page/tree/branch").tag,
      "button");
    assert.equal(runtime.webNodeSnapshot(treeItemRt, "Page/tree/leaf").role,
      "treeitem");
    assert.equal(runtime.webNodeQuery(treeItemRt, "Page/tree/custom").role,
      "button");
    assert.deepEqual(runtime.webNodeRelationRefs(treeItemRt, "Page/tree").collectionItems,
      ["Page/tree/branch", "Page/tree/leaf"]);
    assert.deepEqual(runtime.webNodeRelationRefs(treeItemRt, "Page/tree").activeCollectionItems,
      ["Page/tree/branch"]);
    assert.equal(runtime.webNodeRelationRefs(treeItemRt, "Page/tree/branch").collectionOwner,
      "Page/tree");
    assert.equal(runtime.webNodeRelationRefs(treeItemRt, "Page/tree/branch").activeCollectionOwner,
      "Page/tree");
    const menuTarget = document.createElement("div");
    runtime.renderWebDocument(menuRt, menuTarget);
    const choices = runtime.findWebElement(menuTarget, "choices");
    const choiceOne = runtime.findWebElement(menuTarget, "choiceOne");
    const choiceThree = runtime.findWebElement(menuTarget, "choiceThree");
    assert.equal(choices.attributes["aria-orientation"], "vertical");
    assert.equal(choices.attributes["aria-multiselectable"], "true");
    assert.equal(choiceOne.attributes["aria-level"], "2");
    assert.equal(choiceOne.attributes["aria-posinset"], "1");
    assert.equal(choiceOne.attributes["aria-setsize"], "3");
    assert.equal(choiceOne.attributes["aria-haspopup"], "menu");
    assert.equal(choiceThree.tagName, "BUTTON");
    assert.equal(choiceThree.attributes.role, "menuitem");
    assert.deepEqual(runtime.webDOMRelationRefs(menuTarget, "Page/choices").collectionItems,
      ["Page/choices/choiceOne", "Page/choices/choiceTwo", "Page/choices/choiceThree"]);
    assert.equal(runtime.webDOMSnapshot(menuTarget, "Page/choices/choiceThree")
      .relationRefs.collectionOwner, "Page/choices");
    assert.equal(runtime.webDOMSnapshot(menuTarget, "Page/choices/choiceThree")
      .relationRefs.activeCollectionOwner, "");
    assert.equal(runtime.webDOMQuery(menuTarget, "[aria-level=2]").element,
      choiceOne);

    const treeTarget = document.createElement("div");
    runtime.renderWebDocument(treeItemRt, treeTarget);
    assert.deepEqual(runtime.webDOMRelationRefs(treeTarget, "Page/tree").activeCollectionItems,
      ["Page/tree/branch"]);
    assert.equal(runtime.webDOMSnapshot(treeTarget, "Page/tree/branch")
      .relationRefs.activeCollectionOwner, "Page/tree");

    const nativeRt = runtime.createRuntime();
    runtime.beginFrame(nativeRt);
    const nativeEvents = [];
    runtime.widget(nativeRt, "NavigationBar", {}, null,
      { nodeName: "nav", path: "Page/nav" });
    runtime.widget(nativeRt, "Button", { label: "Home" }, null,
      { nodeName: "homeLink", path: "Page/nav/home", parentPath: "Page/nav" });
    runtime.widget(nativeRt, "TitleBar", {}, null,
      { nodeName: "title", path: "Page/title" });
    runtime.widget(nativeRt, "HGroup", {}, null,
      { nodeName: "headingGroup", path: "Page/headingGroup" });
    runtime.widget(nativeRt, "Heading", { text: "Native DOM" }, null,
      { nodeName: "headingGroupTitle", path: "Page/headingGroup/title", parentPath: "Page/headingGroup" });
    runtime.widget(nativeRt, "Search", {}, null,
      { nodeName: "siteSearch", path: "Page/search" });
    runtime.widget(nativeRt, "TextField", { placeholder: "Search" }, null,
      { nodeName: "siteSearchField", path: "Page/search/field", parentPath: "Page/search" });
    runtime.widget(nativeRt, "Article", { text: "Release notes" }, null,
      { nodeName: "article", path: "Page/article" });
    runtime.widget(nativeRt, "Aside", {}, null,
      { nodeName: "aside", path: "Page/aside" });
    runtime.widget(nativeRt, "Text", { text: "Related" }, null,
      { nodeName: "asideText", path: "Page/aside/related", parentPath: "Page/aside" });
    runtime.widget(nativeRt, "Footer", {}, null,
      { nodeName: "footer", path: "Page/footer" });
    runtime.widget(nativeRt, "Text", { text: "Legal" }, null,
      { nodeName: "footerText", path: "Page/footer/legal", parentPath: "Page/footer" });
    runtime.widget(nativeRt, "Figure", {}, null,
      { nodeName: "figure", path: "Page/figure" });
    runtime.widget(nativeRt, "Figcaption", { text: "Launch chart" }, null,
      { nodeName: "caption", path: "Page/figure/caption", parentPath: "Page/figure" });
    runtime.widget(nativeRt, "UnorderedList", {}, null,
      { nodeName: "list", path: "Page/list" });
    runtime.widget(nativeRt, "ListItem", { text: "First" }, null,
      { nodeName: "listItem", path: "Page/list/first", parentPath: "Page/list" });
    runtime.widget(nativeRt, "OrderedList", { start: 3, reversed: true, type: "A" }, null,
      { nodeName: "orderedList", path: "Page/ordered" });
    runtime.widget(nativeRt, "ListItem", { text: "Third", value: 3 }, null,
      { nodeName: "orderedItem", path: "Page/ordered/third", parentPath: "Page/ordered" });
    runtime.widget(nativeRt, "BlockQuote", { text: "Native DOM first.", cite: "/notes/native-dom" }, null,
      { nodeName: "quote", path: "Page/quote" });
    runtime.widget(nativeRt, "Quote", { text: "Inline quote", cite: "/notes/inline" }, null,
      { nodeName: "inlineQuote", path: "Page/inlineQuote" });
    runtime.widget(nativeRt, "CodeBlock", { text: "Button.primary {}" }, null,
      { nodeName: "codeBlock", path: "Page/codeBlock" });
    runtime.widget(nativeRt, "Code", { text: "dom_ref" }, null,
      { nodeName: "inlineCode", path: "Page/inlineCode" });
    runtime.widget(nativeRt, "Strong", { text: "Important" }, null,
      { nodeName: "strongText", path: "Page/strong" });
    runtime.widget(nativeRt, "Emphasis", { text: "Emphasis" }, null,
      { nodeName: "emText", path: "Page/em" });
    runtime.widget(nativeRt, "Abbreviation", { text: "DOM", title: "Document Object Model" }, null,
      { nodeName: "abbrText", path: "Page/abbr" });
    runtime.widget(nativeRt, "Data", { text: "Forty two", value: "42" }, null,
      { nodeName: "dataText", path: "Page/data" });
    runtime.widget(nativeRt, "Deleted", { text: "Old", cite: "/changes/1", datetime: "2026-09-12" }, null,
      { nodeName: "deletedText", path: "Page/deleted" });
    runtime.widget(nativeRt, "Inserted", { text: "New", cite: "/changes/2", datetime: "2026-09-13" }, null,
      { nodeName: "insertedText", path: "Page/inserted" });
    runtime.widget(nativeRt, "Subscript", { text: "2" }, null,
      { nodeName: "subText", path: "Page/sub" });
    runtime.widget(nativeRt, "Superscript", { text: "n" }, null,
      { nodeName: "supText", path: "Page/sup" });
    runtime.widget(nativeRt, "Keyboard", { text: "Ctrl+K" }, null,
      { nodeName: "kbdText", path: "Page/kbd" });
    runtime.widget(nativeRt, "Sample", { text: "ok" }, null,
      { nodeName: "sampleText", path: "Page/sample" });
    runtime.widget(nativeRt, "Variable", { text: "x" }, null,
      { nodeName: "varText", path: "Page/var" });
    runtime.widget(nativeRt, "Cite", { text: "Kryon Notes" }, null,
      { nodeName: "citeText", path: "Page/cite" });
    runtime.widget(nativeRt, "Mark", { text: "highlight" }, null,
      { nodeName: "mark", path: "Page/mark" });
    runtime.widget(nativeRt, "Time", { text: "2026-09-13", datetime: "2026-09-13" }, null,
      { nodeName: "time", path: "Page/time" });
    runtime.widget(nativeRt, "Address", { text: "hello@example.test" }, null,
      { nodeName: "nativeAddress", path: "Page/address" });
    runtime.widget(nativeRt, "Small", { text: "Fine print" }, null,
      { nodeName: "nativeSmall", path: "Page/small" });
    runtime.widget(nativeRt, "Ruby", { text: "\u6f22" }, null,
      { nodeName: "nativeRuby", path: "Page/ruby" });
    runtime.widget(nativeRt, "RubyText", { text: "kan" }, null,
      { nodeName: "nativeRubyText", path: "Page/ruby/text", parentPath: "Page/ruby" });
    runtime.widget(nativeRt, "RubyParenthesis", { text: "(" }, null,
      { nodeName: "nativeRubyParenthesis", path: "Page/ruby/open", parentPath: "Page/ruby" });
    runtime.widget(nativeRt, "BidirectionalIsolate", { text: "\u0645\u0631\u062d\u0628\u0627" }, null,
      { nodeName: "nativeBdi", path: "Page/bdi" });
    runtime.widget(nativeRt, "BidirectionalOverride", { text: "abc", dir: "rtl", lang: "ar", translate: "no" }, null,
      { nodeName: "nativeBdo", path: "Page/bdo" });
    runtime.widget(nativeRt, "LineBreak", {}, null,
      { nodeName: "nativeBr", path: "Page/break" });
    runtime.widget(nativeRt, "WordBreakOpportunity", {}, null,
      { nodeName: "nativeWbr", path: "Page/wbr" });
    runtime.widget(nativeRt, "DescriptionList", {}, null,
      { nodeName: "nativeDescriptionList", path: "Page/descriptions" });
    runtime.widget(nativeRt, "DescriptionTerm", { text: "DOM" }, null,
      { nodeName: "nativeDescriptionTerm", path: "Page/descriptions/dom", parentPath: "Page/descriptions" });
    runtime.widget(nativeRt, "DescriptionDetails", { text: "Document Object Model" }, null,
      { nodeName: "nativeDescriptionDetails", path: "Page/descriptions/dom/details", parentPath: "Page/descriptions" });
    runtime.widget(nativeRt, "Base", { href: "https://example.test/", target: "_blank" }, null,
      { nodeName: "nativeBase", path: "Page/base" });
    runtime.widget(nativeRt, "Meta", { name: "description", content: "Kry DOM", charset: "utf-8" }, null,
      { nodeName: "nativeMeta", path: "Page/meta" });
    runtime.widget(nativeRt, "Title", { text: "Kry document title" }, null,
      { nodeName: "nativeTitle", path: "Page/title" });
    runtime.widget(nativeRt, "StyleElement", {
      text: ".kry-style-probe { color: rgb(1, 2, 3); }",
      media: "screen",
      nonce: "style-nonce"
    }, null,
      { nodeName: "nativeStyleElement", path: "Page/styleElement" });
    runtime.widget(nativeRt, "Video", { src: "intro.mp4", poster: "intro.jpg", controls: true, preload: "metadata" }, null,
      { nodeName: "nativeVideo", path: "Page/video" });
    runtime.widget(nativeRt, "Source", { src: "intro.webm", type: "video/webm" }, null,
      { nodeName: "nativeVideoSource", path: "Page/video/webm", parentPath: "Page/video" });
    runtime.widget(nativeRt, "Track", { src: "captions.vtt", kind: "captions", srclang: "en", label: "English", default: true }, null,
      { nodeName: "nativeVideoTrack", path: "Page/video/captions", parentPath: "Page/video" });
    runtime.widget(nativeRt, "Audio", { src: "theme.mp3", controls: true, loop: true }, null,
      { nodeName: "nativeAudio", path: "Page/audio" });
    runtime.widget(nativeRt, "IFrame", {
      src: "/embed",
      loading: "lazy",
      allow: "fullscreen",
      allow_fullscreen: true,
      sandbox: "allow-scripts",
      referrer_policy: "strict-origin",
      credentialless: true,
      frame_name: "preview",
      width: 640,
      height: 360,
      text: "Embedded content"
    }, null,
      { nodeName: "nativeFrame", path: "Page/frame" });
    runtime.widget(nativeRt, "Embed", { src: "chart.svg", type: "image/svg+xml" }, null,
      { nodeName: "nativeEmbed", path: "Page/embed" });
    runtime.widget(nativeRt, "EmbeddedObject", {
      object_data: "document.pdf",
      type: "application/pdf",
      width: 800,
      height: 600
    }, null,
      { nodeName: "nativeObject", path: "Page/object" });
    runtime.widget(nativeRt, "Param", { name: "page", value: "2" }, null,
      { nodeName: "nativeParam", path: "Page/object/page", parentPath: "Page/object" });
    runtime.widget(nativeRt, "Script", {
      type: "application/json",
      text: "{\"enabled\":true}",
      nonce: "nonce-1",
      integrity: "sha256-demo",
      referrer_policy: "no-referrer",
      crossorigin: "anonymous",
      no_module: true
    }, null,
      { nodeName: "nativeScript", path: "Page/script" });
    runtime.widget(nativeRt, "NoScript", { text: "Enable JavaScript" }, null,
      { nodeName: "nativeNoScript", path: "Page/noscript" });
    runtime.widget(nativeRt, "Source", { srcset: "hero.webp 1x, hero@2x.webp 2x", type: "image/webp" }, null,
      { nodeName: "nativePictureSource", path: "Page/sourceSet" });
    runtime.widget(nativeRt, "Template", { text: "Deferred content" }, null,
      { nodeName: "nativeTemplate", path: "Page/template" });
    runtime.widget(nativeRt, "Slot", { dom_name: "actions" }, null,
      { nodeName: "nativeSlot", path: "Page/slot" });
    runtime.widget(nativeRt, "Table", {}, null,
      { nodeName: "nativeTable", path: "Page/nativeTable" });
    runtime.widget(nativeRt, "TableCaption", { text: "Totals" }, null,
      { nodeName: "nativeTableCaption", path: "Page/nativeTable/caption", parentPath: "Page/nativeTable" });
    runtime.widget(nativeRt, "TableColumnGroup", { span: 2 }, null,
      { nodeName: "nativeColumns", path: "Page/nativeTable/columns", parentPath: "Page/nativeTable" });
    runtime.widget(nativeRt, "TableColumn", { span: 1 }, null,
      { nodeName: "nativeColumn", path: "Page/nativeTable/columns/first", parentPath: "Page/nativeTable/columns" });
    runtime.widget(nativeRt, "TableHead", {}, null,
      { nodeName: "nativeTableHead", path: "Page/nativeTable/head", parentPath: "Page/nativeTable" });
    runtime.widget(nativeRt, "TableRow", {}, null,
      { nodeName: "nativeTableHeadRow", path: "Page/nativeTable/head/row", parentPath: "Page/nativeTable/head" });
    runtime.widget(nativeRt, "TableCell", { text: "Name", scope: "col" }, null,
      { nodeName: "nativeHeaderCell", path: "Page/nativeTable/head/row/name", parentPath: "Page/nativeTable/head/row" });
    runtime.widget(nativeRt, "TableBody", {}, null,
      { nodeName: "nativeTableBody", path: "Page/nativeTable/body", parentPath: "Page/nativeTable" });
    runtime.widget(nativeRt, "TableRow", {}, null,
      { nodeName: "nativeTableBodyRow", path: "Page/nativeTable/body/row", parentPath: "Page/nativeTable/body" });
    runtime.widget(nativeRt, "TableCell", { text: "Kryon", headers: "nativeHeaderCell" }, null,
      { nodeName: "nativeBodyCell", path: "Page/nativeTable/body/row/name", parentPath: "Page/nativeTable/body/row" });
    runtime.widget(nativeRt, "Form", { form_action: "/signup", form_method: "post" }, null,
      { nodeName: "nativeForm", path: "Page/nativeForm" });
    runtime.widget(nativeRt, "Label", { text: "Email", for: "form-email" }, null,
      { nodeName: "nativeLabel", path: "Page/nativeForm/label", parentPath: "Page/nativeForm" });
    runtime.widget(nativeRt, "TextField", { text: "hello@example.test" }, null,
      { nodeName: "nativeEmail", path: "Page/nativeForm/email", parentPath: "Page/nativeForm", id: "form-email" });
    runtime.widget(nativeRt, "Select", {}, null,
      { nodeName: "nativeSelect", path: "Page/nativeSelect" });
    runtime.widget(nativeRt, "OptionGroup", { label: "Numbers" }, null,
      { nodeName: "nativeOptionGroup", path: "Page/nativeSelect/numbers", parentPath: "Page/nativeSelect" });
    runtime.widget(nativeRt, "Option", { text: "One", value: "1", selected: true }, null,
      { nodeName: "nativeOption", path: "Page/nativeSelect/numbers/one", parentPath: "Page/nativeSelect/numbers" });
    runtime.widget(nativeRt, "DataList", {}, null,
      { nodeName: "suggestions", path: "Page/suggestions" });
    runtime.widget(nativeRt, "Option", { value: "hello@example.test" }, null,
      { nodeName: "suggestedEmail", path: "Page/suggestions/email", parentPath: "Page/suggestions" });
    runtime.widget(nativeRt, "Details", { open: true }, null,
      { nodeName: "nativeDetails", path: "Page/nativeDetails" });
    runtime.widget(nativeRt, "Summary", { text: "More" }, null,
      { nodeName: "nativeSummary", path: "Page/nativeDetails/summary", parentPath: "Page/nativeDetails" });
    runtime.widget(nativeRt, "Dialog", { open: true }, null,
      { nodeName: "nativeDialog", path: "Page/nativeDialog" });
    runtime.widget(nativeRt, "Output", { value: "Ready", for: "nativeEmail email" }, null,
      { nodeName: "nativeOutput", path: "Page/nativeOutput" });
    runtime.widget(nativeRt, "Card", {}, null,
      { nodeName: "plainCard", path: "Page/plainCard" });
    runtime.widget(nativeRt, "Card", { clickable: true }, null,
      { nodeName: "actionCard", path: "Page/actionCard" });
    runtime.widget(nativeRt, "Fieldset", { title: "Preferences" }, null,
      { nodeName: "fieldset", path: "Page/fieldset" });
    runtime.widget(nativeRt, "Legend", { text: "Preferences" }, null,
      { nodeName: "fieldsetLegend", path: "Page/fieldset/legend", parentPath: "Page/fieldset" });
    runtime.widget(nativeRt, "Checkbox", { checked: true, label: "Agree" }, null,
      { nodeName: "fieldsetAgree", path: "Page/fieldset/agree", parentPath: "Page/fieldset" });
    runtime.widget(nativeRt, "Fieldset", { title: "Locked", disabled: true }, null,
      { nodeName: "lockedFieldset", path: "Page/locked" });
    runtime.widget(nativeRt, "TextField", { label: "Locked field" }, null,
      { nodeName: "lockedField", path: "Page/locked/field", parentPath: "Page/locked" });
    runtime.widget(nativeRt, "Collapsible", {}, null,
      { nodeName: "autoDetails", path: "Page/autoDetails" });
    runtime.widget(nativeRt, "Modal", {}, null,
      { nodeName: "autoDialog", path: "Page/autoDialog" });
    runtime.widget(nativeRt, "TextField", {
      input_type: "email",
      placeholder: "Email",
      autocomplete: "email",
      required: true,
      min_length: 3,
      max_length: 254,
      pattern: ".+@.+",
      input_mode: "email",
      enter_key_hint: "send",
      dirname: "email.dir",
      list: "suggestions"
    }, null,
      { nodeName: "email", path: "Page/email" });
    runtime.widget(nativeRt, "Slider", { min: 0, max: 10, value: 4, label: "Volume" }, null,
      { nodeName: "volume", path: "Page/volume" });
    runtime.widget(nativeRt, "Spinbox", { min: 1, max: 8, value: 3 }, null,
      { nodeName: "copies", path: "Page/copies" });
    runtime.widget(nativeRt, "Input", { min: -10, max: 10, step: 0.5, value: 2.5, label: "Amount" }, null,
      { nodeName: "amount", path: "Page/amount" });
    runtime.widget(nativeRt, "Dropdown", {}, null,
      { nodeName: "choice", path: "Page/choice" });
    runtime.widget(nativeRt, "Selectable", { label: "Alpha", value: "a" }, null,
      { nodeName: "choiceAlpha", path: "Page/choice/alpha", parentPath: "Page/choice" });
    runtime.widget(nativeRt, "ListBox", {}, null,
      { nodeName: "items", path: "Page/items" });
    runtime.widget(nativeRt, "Selectable", { text: "Beta", value: "b", selected: true }, null,
      { nodeName: "itemBeta", path: "Page/items/beta", parentPath: "Page/items" });
    runtime.widget(nativeRt, "ColorPicker", { value: "#336699", label: "Accent" }, null,
      { nodeName: "accent", path: "Page/accent" });
    runtime.widget(nativeRt, "Checkbox", { checked: true }, null,
      { nodeName: "agree", path: "Page/agree" });
    runtime.widget(nativeRt, "Toggle", { checked: false }, null,
      { nodeName: "enabled", path: "Page/enabled" });
    runtime.widget(nativeRt, "Radio", { checked: true }, null,
      { nodeName: "choiceRadio", path: "Page/choiceRadio" });
    runtime.widget(nativeRt, "Progress", { value: 42, max: 100 }, null,
      { nodeName: "upload", path: "Page/upload" });
    runtime.widget(nativeRt, "Meter", { min: 0, max: 1, value: 0.75, label: "Storage" }, null,
      { nodeName: "storage", path: "Page/storage" });
    runtime.widget(nativeRt, "Line", {}, null,
      { nodeName: "line", path: "Page/line" });
    runtime.widget(nativeRt, "Separator", {}, null,
      { nodeName: "rule", path: "Page/rule" });
    runtime.widget(nativeRt, "TableView", {}, null,
      { nodeName: "table", path: "Page/table" });
    runtime.widget(nativeRt, "Image", {
      asset_path: "hero.png",
      alt_text: "Hero",
      use_map: "heroMap",
      srcset: "hero-small.png 480w, hero.png 960w",
      sizes: "(max-width: 600px) 480px, 960px",
      loading: "lazy",
      decoding: "async",
      fetch_priority: "high",
      referrer_policy: "no-referrer",
      crossorigin: "anonymous",
      width: 960,
      height: 540
    }, null,
      { nodeName: "hero", path: "Page/hero" });
    runtime.widget(nativeRt, "ImageMap", { dom_name: "hero-map" }, null,
      { nodeName: "heroMap", path: "Page/heroMap" });
    runtime.widget(nativeRt, "Area", {
      alt: "Primary region",
      href: "/hero",
      target: "_self",
      rel: "bookmark",
      download: "hero.txt",
      ping: "/hero-audit",
      href_lang: "en",
      referrer_policy: "same-origin",
      shape: "rect",
      coords: "0,0,100,80"
    }, null,
      { nodeName: "heroArea", path: "Page/heroMap/primary", parentPath: "Page/heroMap" });
    runtime.widget(nativeRt, "Icon", {}, null,
      { nodeName: "glyph", path: "Page/glyph" });
    runtime.widget(nativeRt, "Bullet", {}, null,
      { nodeName: "bullet", path: "Page/bullet" });
    runtime.widget(nativeRt, "CanvasGrid", {}, null,
      { nodeName: "grid", path: "Page/grid" });
    runtime.widget(nativeRt, "Toolbar", {}, null,
      { nodeName: "toolbar", path: "Page/toolbar" });
    runtime.widget(nativeRt, "SegmentedControl", {}, null,
      { nodeName: "segments", path: "Page/segments" });
    runtime.widget(nativeRt, "TabBar", {}, null,
      { nodeName: "tabs", path: "Page/tabs" });
    runtime.widget(nativeRt, "Button", { label: "Details" }, null,
      { nodeName: "detailsTab", path: "Page/tabs/details", parentPath: "Page/tabs" });
    runtime.widget(nativeRt, "Selectable", { label: "Logs" }, null,
      { nodeName: "logsTab", path: "Page/tabs/logs", parentPath: "Page/tabs" });
    runtime.widget(nativeRt, "TreeView", {}, null,
      { nodeName: "tree", path: "Page/tree" });
    runtime.widget(nativeRt, "Menu", {}, null,
      { nodeName: "menu", path: "Page/menu" });
    runtime.widget(nativeRt, "Toast", {}, null,
      { nodeName: "toast", path: "Page/toast" });
    runtime.widget(nativeRt, "Plot", {}, null,
      { nodeName: "plot", path: "Page/plot" });
    runtime.widget(nativeRt, "Popup", { flags: 3 }, null,
      { nodeName: "modalPopup", path: "Page/modalPopup" });
    runtime.widget(nativeRt, "Popup", { flags: 1 }, null,
      { nodeName: "tipPopup", path: "Page/tipPopup" });
    runtime.widget(nativeRt, "Popup", { flags: 4 }, null,
      { nodeName: "contextPopup", path: "Page/contextPopup" });
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
    assert.equal(runtime.webNodeQuery(nativeRt, "NavigationBar").tag, "nav");
    assert.equal(runtime.webNodeQuery(nativeRt, "NavigationBar").role, "");
    assert.equal(runtime.webNodeQuery(nativeRt, "TitleBar").tag, "header");
    assert.equal(runtime.webNodeQuery(nativeRt, "TitleBar").role, "");
    assert.equal(runtime.webNodeQuery(nativeRt, "HGroup").tag, "hgroup");
    assert.equal(runtime.webNodeQuery(nativeRt, "Search").tag, "search");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/search/field").landmarkOwner.path,
      "Page/search");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/search").landmarkMembers,
      ["Page/search/field"]);
    assert.equal(runtime.webNodeQuery(nativeRt, "Card").tag, "div");
    assert.equal(runtime.webNodeQuery(nativeRt, "Card[clickable=true]").tag, "button");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/actionCard").clickable, true);
    assert.equal(runtime.webNodeStyleFacts(runtime.webNodeQuery(nativeRt, "Page/actionCard")).clickable, true);
    assert.equal(runtime.webNodeQuery(nativeRt, "Fieldset").tag, "fieldset");
    assert.equal(runtime.webNodeQuery(nativeRt, "Legend").tag, "legend");
    assert.equal(runtime.webNodeQuery(nativeRt, "Legend").text, "Preferences");
    assert.equal(runtime.webNodeQuery(nativeRt, "Fieldset").ariaLabel, "Preferences");
    assert.equal(runtime.webNodeQuery(nativeRt, "fieldsetAgree").ariaLabel, "Agree");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/volume").ariaLabel, "Volume");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/amount").ariaLabel, "Amount");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/accent").ariaLabel, "Accent");
    assert.equal(runtime.webNodeRelations(nativeRt, "fieldsetAgree").groupOwner.path,
      "Page/fieldset");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Fieldset").groupMembers,
      ["Page/fieldset/agree"]);
    assert.equal(runtime.webNodeRelations(nativeRt, "fieldsetLegend").legendOwner.path,
      "Page/fieldset");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Fieldset").legendItems,
      ["Page/fieldset/legend"]);
    assert.equal(runtime.webNodeRelations(nativeRt, "lockedField").disabledOwner.path,
      "Page/locked");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "lockedFieldset").disabledMembers,
      ["Page/locked/field"]);
    assert.equal(runtime.webNodeSnapshot(nativeRt, "lockedField")
      .relationRefs.disabledOwner, "Page/locked");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "NavigationBar")?.role, "navigation");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "TitleBar")?.role, "banner");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Search")?.role, "search");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Aside")?.role, "complementary");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Footer")?.role, "contentinfo");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Figure")?.role, "figure");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "UnorderedList")?.role, "list");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Form")?.role, "form");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Summary")?.role, "button");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Output")?.role, "status");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "TableHead")?.role, "rowgroup");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.path === "Page/nativeTable/head/row")?.role, "row");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.path === "Page/nativeTable/head/row/name")?.role,
      "columnheader");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/nav/home").landmarkOwner.path,
      "Page/nav");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/nav").landmarkMembers,
      ["Page/nav/home"]);
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/nav/home")
      .relationRefs.landmarkOwner, "Page/nav");
    assert.equal(runtime.webNodeQuery(nativeRt, "Article").tag, "article");
    assert.equal(runtime.webNodeQuery(nativeRt, "Aside").tag, "aside");
    assert.equal(runtime.webNodeQuery(nativeRt, "Footer").tag, "footer");
    assert.equal(runtime.webNodeQuery(nativeRt, "Figure").tag, "figure");
    assert.equal(runtime.webNodeQuery(nativeRt, "Figcaption").tag, "figcaption");
    assert.equal(runtime.webNodeQuery(nativeRt, "UnorderedList").tag, "ul");
    assert.equal(runtime.webNodeQuery(nativeRt, "ListItem").tag, "li");
    assert.equal(runtime.webNodeQuery(nativeRt, "OrderedList").tag, "ol");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/ordered").extraAttrs.start, "3");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/ordered").extraAttrs.reversed, true);
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/ordered/third").extraAttrs.value, "3");
    assert.equal(runtime.webNodeQuery(nativeRt, "BlockQuote").tag, "blockquote");
    assert.equal(runtime.webNodeQuery(nativeRt, "BlockQuote").extraAttrs.cite, "/notes/native-dom");
    assert.equal(runtime.webNodeQuery(nativeRt, "Quote").tag, "q");
    assert.equal(runtime.webNodeQuery(nativeRt, "Quote").extraAttrs.cite, "/notes/inline");
    assert.equal(runtime.webNodeQuery(nativeRt, "CodeBlock").tag, "pre");
    assert.equal(runtime.webNodeQuery(nativeRt, "Code").tag, "code");
    assert.equal(runtime.webNodeQuery(nativeRt, "Strong").tag, "strong");
    assert.equal(runtime.webNodeQuery(nativeRt, "Emphasis").tag, "em");
    assert.equal(runtime.webNodeQuery(nativeRt, "Abbreviation").tag, "abbr");
    assert.equal(runtime.webNodeQuery(nativeRt, "Abbreviation").title, "Document Object Model");
    assert.equal(runtime.webNodeQuery(nativeRt, "Data").tag, "data");
    assert.equal(runtime.webNodeQuery(nativeRt, "Data").extraAttrs.value, "42");
    assert.equal(runtime.webNodeQuery(nativeRt, "Deleted").tag, "del");
    assert.equal(runtime.webNodeQuery(nativeRt, "Deleted").extraAttrs.datetime, "2026-09-12");
    assert.equal(runtime.webNodeQuery(nativeRt, "Inserted").tag, "ins");
    assert.equal(runtime.webNodeQuery(nativeRt, "Inserted").extraAttrs.cite, "/changes/2");
    assert.equal(runtime.webNodeQuery(nativeRt, "Subscript").tag, "sub");
    assert.equal(runtime.webNodeQuery(nativeRt, "Superscript").tag, "sup");
    assert.equal(runtime.webNodeQuery(nativeRt, "Keyboard").tag, "kbd");
    assert.equal(runtime.webNodeQuery(nativeRt, "Sample").tag, "samp");
    assert.equal(runtime.webNodeQuery(nativeRt, "Variable").tag, "var");
    assert.equal(runtime.webNodeQuery(nativeRt, "Cite").tag, "cite");
    assert.equal(runtime.webNodeQuery(nativeRt, "Mark").tag, "mark");
    assert.equal(runtime.webNodeQuery(nativeRt, "Time").tag, "time");
    assert.equal(runtime.webNodeQuery(nativeRt, "Time").extraAttrs.datetime, "2026-09-13");
    assert.equal(runtime.webNodeQuery(nativeRt, "Address").tag, "address");
    assert.equal(runtime.webNodeQuery(nativeRt, "Small").tag, "small");
    assert.equal(runtime.webNodeQuery(nativeRt, "Ruby").tag, "ruby");
    assert.equal(runtime.webNodeQuery(nativeRt, "Ruby").text, "\u6f22");
    assert.equal(runtime.webNodeQuery(nativeRt, "RubyText").tag, "rt");
    assert.equal(runtime.webNodeQuery(nativeRt, "RubyParenthesis").tag, "rp");
    assert.equal(runtime.webNodeQuery(nativeRt, "BidirectionalIsolate").tag, "bdi");
    assert.equal(runtime.webNodeQuery(nativeRt, "BidirectionalOverride").tag, "bdo");
    assert.equal(runtime.webNodeQuery(nativeRt, "BidirectionalOverride").dir, "rtl");
    assert.equal(runtime.webNodeQuery(nativeRt, "BidirectionalOverride").lang, "ar");
    assert.equal(runtime.webNodeQuery(nativeRt, "BidirectionalOverride").translate, "no");
    assert.equal(runtime.webNodeQuery(nativeRt, "[dir=rtl]").path, "Page/bdo");
    assert.equal(runtime.webNodeQuery(nativeRt, "LineBreak").tag, "br");
    assert.equal(runtime.webNodeQuery(nativeRt, "WordBreakOpportunity").tag, "wbr");
    assert.equal(runtime.webNodeQuery(nativeRt, "DescriptionList").tag, "dl");
    assert.equal(runtime.webNodeQuery(nativeRt, "DescriptionTerm").tag, "dt");
    assert.equal(runtime.webNodeQuery(nativeRt, "DescriptionTerm").text, "DOM");
    assert.equal(runtime.webNodeQuery(nativeRt, "DescriptionDetails").tag, "dd");
    assert.equal(runtime.webNodeQuery(nativeRt, "DescriptionDetails").text, "Document Object Model");
    assert.equal(runtime.webNodeRelations(nativeRt, "DescriptionTerm").descriptionListOwner.path,
      "Page/descriptions");
    assert.equal(runtime.webNodeRelations(nativeRt, "DescriptionDetails").descriptionListOwner.path,
      "Page/descriptions");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "DescriptionList").descriptionListItems,
      ["Page/descriptions/dom", "Page/descriptions/dom/details"]);
    assert.equal(runtime.webNodeQuery(nativeRt, "Base").tag, "base");
    assert.equal(runtime.webNodeQuery(nativeRt, "Base").extraAttrs.href, "https://example.test/");
    assert.equal(runtime.webNodeQuery(nativeRt, "Base").extraAttrs.target, "_blank");
    assert.equal(runtime.webNodeQuery(nativeRt, "Meta").tag, "meta");
    assert.equal(runtime.webNodeQuery(nativeRt, "Meta").extraAttrs.name, "description");
    assert.equal(runtime.webNodeQuery(nativeRt, "Meta").extraAttrs.content, "Kry DOM");
    assert.equal(runtime.webNodeQuery(nativeRt, "Meta").extraAttrs.charset, "utf-8");
    assert.equal(runtime.webNodeQuery(nativeRt, "Title").tag, "title");
    assert.equal(runtime.webNodeQuery(nativeRt, "Title").text, "Kry document title");
    assert.equal(runtime.webNodeQuery(nativeRt, "[content=\"Kry DOM\"]").path, "Page/meta");
    assert.equal(runtime.webNodeQuery(nativeRt, "StyleElement").tag, "style");
    assert.equal(runtime.webNodeQuery(nativeRt, "StyleElement").text,
      ".kry-style-probe { color: rgb(1, 2, 3); }");
    assert.equal(runtime.webNodeQuery(nativeRt, "StyleElement").extraAttrs.media, "screen");
    assert.equal(runtime.webNodeQuery(nativeRt, "StyleElement").extraAttrs.nonce, "style-nonce");
    assert.equal(runtime.webNodeQuery(nativeRt, "[media=screen]").path, "Page/styleElement");
    assert.equal(runtime.webNodeQuery(nativeRt, "Video").tag, "video");
    assert.equal(runtime.webNodeQuery(nativeRt, "Video").extraAttrs.src, "intro.mp4");
    assert.equal(runtime.webNodeQuery(nativeRt, "Video").extraAttrs.controls, true);
    assert.equal(runtime.webNodeQuery(nativeRt, "Source").tag, "source");
    assert.equal(runtime.webNodeQuery(nativeRt, "Source").extraAttrs.type, "video/webm");
    assert.equal(runtime.webNodeQuery(nativeRt, "Track").tag, "track");
    assert.equal(runtime.webNodeQuery(nativeRt, "Track").extraAttrs.srclang, "en");
    assert.equal(runtime.webNodeQuery(nativeRt, "Audio").tag, "audio");
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").tag, "iframe");
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").extraAttrs.loading, "lazy");
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").extraAttrs.sandbox, "allow-scripts");
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").extraAttrs.referrerpolicy, "strict-origin");
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").extraAttrs.credentialless, true);
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").extraAttrs.name, "preview");
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").extraAttrs.width, "640");
    assert.equal(runtime.webNodeQuery(nativeRt, "IFrame").extraAttrs.height, "360");
    assert.equal(runtime.webNodeQuery(nativeRt, "[sandbox=\"allow-scripts\"]").path, "Page/frame");
    assert.equal(runtime.webNodeQuery(nativeRt, "[credentialless]").path, "Page/frame");
    assert.equal(runtime.webNodeQuery(nativeRt, "Embed").tag, "embed");
    assert.equal(runtime.webNodeQuery(nativeRt, "EmbeddedObject").tag, "object");
    assert.equal(runtime.webNodeQuery(nativeRt, "EmbeddedObject").extraAttrs.data, "document.pdf");
    assert.equal(runtime.webNodeQuery(nativeRt, "EmbeddedObject").extraAttrs.type, "application/pdf");
    assert.equal(runtime.webNodeQuery(nativeRt, "EmbeddedObject").extraAttrs.width, "800");
    assert.equal(runtime.webNodeQuery(nativeRt, "EmbeddedObject").extraAttrs.height, "600");
    assert.equal(runtime.webNodeQuery(nativeRt, "Param").tag, "param");
    assert.equal(runtime.webNodeQuery(nativeRt, "Param").extraAttrs.name, "page");
    assert.equal(runtime.webNodeQuery(nativeRt, "Param").extraAttrs.value, "2");
    assert.equal(runtime.webNodeQuery(nativeRt, "[data=\"document.pdf\"]").path, "Page/object");
    assert.equal(runtime.webNodeQuery(nativeRt, "[value=\"2\"]").path, "Page/object/page");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").tag, "script");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").text, "{\"enabled\":true}");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").extraAttrs.type, "application/json");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").extraAttrs.nonce, "nonce-1");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").extraAttrs.integrity, "sha256-demo");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").extraAttrs.referrerpolicy, "no-referrer");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").extraAttrs.crossorigin, "anonymous");
    assert.equal(runtime.webNodeQuery(nativeRt, "Script").extraAttrs.nomodule, true);
    assert.equal(runtime.webNodeQuery(nativeRt, "NoScript").tag, "noscript");
    assert.equal(runtime.webNodeQuery(nativeRt, "NoScript").text, "Enable JavaScript");
    assert.equal(runtime.webNodeQuery(nativeRt, "[nonce=\"nonce-1\"]").path, "Page/script");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/sourceSet").tag, "source");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/sourceSet").extraAttrs.srcset,
      "hero.webp 1x, hero@2x.webp 2x");
    assert.equal(runtime.webNodeQuery(nativeRt, "Template").tag, "template");
    assert.equal(runtime.webNodeQuery(nativeRt, "Slot").tag, "slot");
    assert.equal(runtime.webNodeQuery(nativeRt, "Slot").domName, "actions");
    assert.equal(runtime.webNodeQuery(nativeRt, "[name=actions]").path, "Page/slot");
    assert.equal(runtime.webNodeQuery(nativeRt, "Table").tag, "table");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableCaption").tag, "caption");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableCaption").text, "Totals");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableColumnGroup").tag, "colgroup");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableColumnGroup").extraAttrs.span, "2");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableColumn").tag, "col");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableColumn").extraAttrs.span, "1");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableHead").tag, "thead");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableBody").tag, "tbody");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/nativeTable/head/row").tag, "tr");
    assert.equal(runtime.webNodeQuery(nativeRt, "nativeHeaderCell").tag, "th");
    assert.equal(runtime.webNodeQuery(nativeRt, "nativeBodyCell").tag, "td");
    assert.equal(runtime.webNodeRelations(nativeRt, "nativeBodyCell").headers[0].path,
      "Page/nativeTable/head/row/name");
    assert.equal(runtime.webNodeQuery(nativeRt, "[src=\"intro.mp4\"]").path, "Page/video");
    assert.equal(runtime.webNodeQuery(nativeRt, "Form").tag, "form");
    assert.equal(runtime.webNodeQuery(nativeRt, "Label").tag, "label");
    assert.equal(runtime.webNodeQuery(nativeRt, "Select").tag, "select");
    assert.equal(runtime.webNodeQuery(nativeRt, "OptionGroup").tag, "optgroup");
    assert.equal(runtime.webNodeQuery(nativeRt, "OptionGroup").extraAttrs.label, "Numbers");
    assert.equal(runtime.webNodeQuery(nativeRt, "Option").tag, "option");
    assert.equal(runtime.webNodeQuery(nativeRt, "DataList").tag, "datalist");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/suggestions/email").collectionOwner.path,
      "Page/suggestions");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/suggestions").collectionItems,
      ["Page/suggestions/email"]);
    assert.equal(runtime.webNodeQuery(nativeRt, "Details").tag, "details");
    assert.equal(runtime.webNodeQuery(nativeRt, "Summary").tag, "summary");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/nativeDetails/summary").summaryOwner.path,
      "Page/nativeDetails");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/nativeDetails").summaryItems,
      ["Page/nativeDetails/summary"]);
    assert.equal(runtime.webNodeQuery(nativeRt, "Dialog").tag, "dialog");
    assert.equal(runtime.webNodeQuery(nativeRt, "Output").tag, "output");
    assert.equal(runtime.webNodeQuery(nativeRt, "Output").htmlFor, "nativeEmail email");
    assert.equal(runtime.webNodeQuery(nativeRt, "Output").text, "Ready");
    assert.equal(runtime.webNodeQuery(nativeRt, "Output").domValue, "Ready");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Output").outputFor,
      ["Page/nativeForm/email", "Page/email"]);
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/email").outputBy,
      ["Page/nativeOutput"]);
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/email").labelledBy, []);
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/nativeForm/label").labelFor.path,
      "Page/nativeForm/email");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/nativeForm").formControls,
      ["Page/nativeForm/email"]);
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/nativeSelect/numbers/one").collectionOwner.path,
      "Page/nativeSelect");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/nativeSelect/numbers/one").groupOwner.path,
      "Page/nativeSelect/numbers");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/nativeSelect/numbers").groupMembers,
      ["Page/nativeSelect/numbers/one"]);
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/figure/caption").captionOwner.path,
      "Page/figure");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/figure").captionItems,
      ["Page/figure/caption"]);
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/nativeTable/caption").captionOwner.path,
      "Page/nativeTable");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/nativeTable").captionItems,
      ["Page/nativeTable/caption"]);
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/aside/related").landmarkOwner.path,
      "Page/aside");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/footer/legal").landmarkOwner.path,
      "Page/footer");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/list/first").collectionOwner.path,
      "Page/list");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/list").collectionItems,
      ["Page/list/first"]);
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/ordered/third").collectionOwner.path,
      "Page/ordered");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Fieldset")?.role, "group");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.path === "Page/fieldset")?.label, "Preferences");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.path === "Page/amount")?.label, "Amount");
    assert.equal(runtime.webNodeQuery(nativeRt, "Collapsible").tag, "details");
    assert.equal(runtime.webNodeQuery(nativeRt, "Modal").tag, "dialog");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").inputType, "email");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").placeholder, "Email");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").autoComplete, "email");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").required, true);
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").minLength, "3");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").maxLength, "254");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").dataList, "suggestions");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").dirname, "email.dir");
    assert.equal(runtime.webNodeQuery(nativeRt, "[dirname=\"email.dir\"]").path, "Page/email");
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/email").dataList.path, "Page/suggestions");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/suggestions").listedBy,
      ["Page/email"]);
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").pattern, ".+@.+");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").inputMode, "email");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/email").enterKeyHint, "send");
    assert.equal(runtime.webNodeQuery(nativeRt, "[type=email]").path, "Page/email");
    assert.equal(runtime.webNodeQuery(nativeRt, "[autocomplete=email]").path, "Page/email");
    assert.equal(runtime.webNodeQuery(nativeRt, "[inputmode=email]").path, "Page/email");
    assert.equal(runtime.webNodeStyleFacts(runtime.webNodeQuery(nativeRt, "Page/email")).placeholder,
      "Email");
    assert.equal(runtime.webNodeQuery(nativeRt, "Menu").tag, "menu");
    assert.equal(runtime.webNodeQuery(nativeRt, "TableView").tag, "table");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/choice/alpha").tag, "option");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/choice/alpha").text, "Alpha");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/choice/alpha").domValue, "a");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/items/beta").tag, "option");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/items/beta").state.selected, true);
    assert.equal(runtime.webNodeRelations(nativeRt, "Page/items/beta").selectedCollectionOwner.path,
      "Page/items");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "Page/items").selectedCollectionItems,
      ["Page/items/beta"]);
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/items/beta")
      .relationRefs.selectedCollectionOwner, "Page/items");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/tabs/details").role, "tab");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/tabs/logs").tag, "button");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/tabs/logs").role, "tab");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/volume").valueNow, "4");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/volume").min, "0");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/volume").max, "10");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/amount").tag, "input");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/amount").inputType, "number");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/amount").domValue, "2.5");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/amount").valueNow, "2.5");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/amount").min, "-10");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/amount").max, "10");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Slider")?.valueNow, "4");
    assert.equal(runtime.webNodeQuery(nativeRt, "[alt=Hero]").path, "Page/hero");
    assert.equal(runtime.webNodeQuery(nativeRt, "[src=\"hero.png\"]").path, "Page/hero");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.srcset,
      "hero-small.png 480w, hero.png 960w");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.sizes,
      "(max-width: 600px) 480px, 960px");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.loading, "lazy");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.decoding, "async");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.fetchpriority, "high");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.referrerpolicy, "no-referrer");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.crossorigin, "anonymous");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.width, "960");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").extraAttrs.height, "540");
    assert.equal(runtime.webNodeQuery(nativeRt, "Image").useMap, "heroMap");
    assert.equal(runtime.webNodeRelations(nativeRt, "Image").imageMap.path, "Page/heroMap");
    assert.deepEqual(runtime.webNodeRelationRefs(nativeRt, "ImageMap").mappedImages,
      ["Page/hero"]);
    assert.equal(runtime.webNodeQuery(nativeRt, "ImageMap").tag, "map");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").tag, "area");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").href, "/hero");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").target, "_self");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").rel, "bookmark");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").download, "hero.txt");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").alt, "Primary region");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").extraAttrs.ping, "/hero-audit");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").extraAttrs.hreflang, "en");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").extraAttrs.referrerpolicy, "same-origin");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").extraAttrs.shape, "rect");
    assert.equal(runtime.webNodeQuery(nativeRt, "Area").extraAttrs.coords, "0,0,100,80");
    assert.equal(runtime.webNodeQuery(nativeRt, "[usemap=heroMap]").path, "Page/hero");
    assert.equal(runtime.webNodeStyleFacts(runtime.webNodeQuery(nativeRt, "Image")).asset, "hero.png");
    assert.equal(runtime.webNodeStyleFacts(runtime.webNodeQuery(nativeRt, "Image")).src, "hero.png");
    assert.equal(runtime.webNodeStyleFacts(runtime.webNodeQuery(nativeRt, "Image")).alt, "Hero");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Image").asset, "hero.png");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Image").src, "hero.png");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Image").alt, "Hero");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Image")?.label, "Hero");
    assert.equal(runtime.webAccessibilitySnapshot(nativeRt).nodes
      .find((node) => node.kind === "Image")?.src, "hero.png");
    assert.equal(runtime.webNodeQuery(nativeRt, "Icon").tag, "span");
    assert.equal(runtime.webNodeQuery(nativeRt, "Bullet").tag, "li");
    assert.equal(runtime.webNodeQuery(nativeRt, "Plot").tag, "canvas");
    assert.equal(runtime.webNodeQuery(nativeRt, "CanvasGrid").tag, "canvas");
    assert.deepEqual(
      ["Slider", "Spinbox", "Input", "Dropdown", "ListBox", "ColorPicker",
       "Checkbox", "Toggle", "Radio", "Progress", "Meter", "Line", "Separator"].map((kind) => {
        const node = runtime.webNodeQuery(nativeRt, kind);
        return [kind, node.tag, node.inputType, node.min, node.max, node.domValue];
      }),
      [
        ["Slider", "input", "range", "0", "10", "4"],
        ["Spinbox", "input", "number", "1", "8", "3"],
        ["Input", "input", "number", "-10", "10", "2.5"],
        ["Dropdown", "select", "", "", "", ""],
        ["ListBox", "select", "", "", "", ""],
        ["ColorPicker", "input", "color", "", "", "#336699"],
        ["Checkbox", "input", "checkbox", "", "", ""],
        ["Toggle", "input", "checkbox", "", "", ""],
        ["Radio", "input", "radio", "", "", ""],
        ["Progress", "progress", "", "", "100", "42"],
        ["Meter", "meter", "", "0", "1", "0.75"],
        ["Line", "hr", "", "", "", ""],
        ["Separator", "hr", "", "", "", ""]
      ]);
    assert.deepEqual(["Slider", "Spinbox", "Input", "Dropdown", "ListBox", "ColorPicker",
      "Checkbox", "Toggle", "Radio", "Progress", "Meter", "Line", "Separator", "TableView"]
      .map((kind) => runtime.webAccessibilitySnapshot(nativeRt).nodes
        .find((node) => node.kind === kind)?.role),
      ["slider", "spinbutton", "spinbutton", "combobox", "listbox", "", "checkbox",
       "switch", "radio", "progressbar", "meter", "separator", "separator", "table"]);
    assert.deepEqual(["Toolbar", "SegmentedControl", "TabBar", "TreeView", "Menu", "Toast", "Icon", "Bullet", "Selectable", "Plot", "CanvasGrid"]
      .map((kind) => runtime.webAccessibilitySnapshot(nativeRt).nodes
        .find((node) => node.kind === kind)?.role),
      ["toolbar", "group", "tablist", "tree", "menu", "status", "img", "listitem", "option", "img", "img"]);
    assert.equal(runtime.webNodeQuery(nativeRt, "Toast").ariaLive, "polite");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/modalPopup").tag, "dialog");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/modalPopup").state.open, true);
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/modalPopup").role, "dialog");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/tipPopup").tag, "div");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/tipPopup").role, "tooltip");
    assert.equal(runtime.webNodeQuery(nativeRt, "Page/contextPopup").tag, "div");
    assert.equal(runtime.webNodeSnapshot(nativeRt, "Page/contextPopup").role, "menu");
    assert.equal(runtime.webNodeQuery(nativeRt, "Section[open=true]").path, "Page/details");
    assert.equal(runtime.webNodeQuery(nativeRt, "Section[open]").path, "Page/details");
    const nativeTarget = document.createElement("div");
    runtime.renderWebDocument(nativeRt, nativeTarget);
    const nav = runtime.findWebElement(nativeTarget, "nav");
    const dropdownOption = runtime.findWebElement(nativeTarget, "Page/choice/alpha");
    const listOption = runtime.findWebElement(nativeTarget, "Page/items/beta");
    const tabButton = runtime.findWebElement(nativeTarget, "Page/tabs/details");
    const tabSelectable = runtime.findWebElement(nativeTarget, "Page/tabs/logs");
    assert.equal(dropdownOption.tagName, "OPTION");
    assert.equal(dropdownOption.textContent, "Alpha");
    assert.equal(dropdownOption.attributes.value, "a");
    assert.equal(listOption.tagName, "OPTION");
    assert.equal(listOption.attributes.selected, "");
    assert.equal(listOption.selected, true);
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/items/beta")
      .selectedCollectionOwner.ref, "Page/items");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/items").selectedCollectionItems,
      ["Page/items/beta"]);
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "Page/items/beta")
      .relationRefs.selectedCollectionOwner, "Page/items");
    assert.equal(tabButton.tagName, "BUTTON");
    assert.equal(tabButton.attributes.role, "tab");
    assert.equal(tabSelectable.tagName, "BUTTON");
    assert.equal(tabSelectable.attributes.role, "tab");
    const title = runtime.findWebElement(nativeTarget, "title");
    const nativeArticle = runtime.findWebElement(nativeTarget, "article");
    const nativeHeadingGroup = runtime.findWebElement(nativeTarget, "headingGroup");
    const nativeHeadingGroupTitle = runtime.findWebElement(nativeTarget, "headingGroupTitle");
    const nativeSearch = runtime.findWebElement(nativeTarget, "siteSearch");
    const nativeSearchField = runtime.findWebElement(nativeTarget, "siteSearchField");
    const nativeAside = runtime.findWebElement(nativeTarget, "aside");
    const nativeFooter = runtime.findWebElement(nativeTarget, "footer");
    const nativeFigure = runtime.findWebElement(nativeTarget, "figure");
    const nativeCaption = runtime.findWebElement(nativeTarget, "caption");
    const nativeList = runtime.findWebElement(nativeTarget, "list");
    const nativeListItem = runtime.findWebElement(nativeTarget, "listItem");
    const nativeOrderedList = runtime.findWebElement(nativeTarget, "orderedList");
    const nativeOrderedItem = runtime.findWebElement(nativeTarget, "orderedItem");
    const nativeQuote = runtime.findWebElement(nativeTarget, "quote");
    const nativeInlineQuote = runtime.findWebElement(nativeTarget, "inlineQuote");
    const nativeCodeBlock = runtime.findWebElement(nativeTarget, "codeBlock");
    const nativeInlineCode = runtime.findWebElement(nativeTarget, "inlineCode");
    const nativeStrong = runtime.findWebElement(nativeTarget, "strongText");
    const nativeEm = runtime.findWebElement(nativeTarget, "emText");
    const nativeAbbr = runtime.findWebElement(nativeTarget, "abbrText");
    const nativeData = runtime.findWebElement(nativeTarget, "dataText");
    const nativeDel = runtime.findWebElement(nativeTarget, "deletedText");
    const nativeIns = runtime.findWebElement(nativeTarget, "insertedText");
    const nativeSub = runtime.findWebElement(nativeTarget, "subText");
    const nativeSup = runtime.findWebElement(nativeTarget, "supText");
    const nativeKbd = runtime.findWebElement(nativeTarget, "kbdText");
    const nativeSamp = runtime.findWebElement(nativeTarget, "sampleText");
    const nativeVar = runtime.findWebElement(nativeTarget, "varText");
    const nativeCite = runtime.findWebElement(nativeTarget, "citeText");
    const nativeMark = runtime.findWebElement(nativeTarget, "mark");
    const nativeTime = runtime.findWebElement(nativeTarget, "time");
    const nativeAddress = runtime.findWebElement(nativeTarget, "nativeAddress");
    const nativeSmall = runtime.findWebElement(nativeTarget, "nativeSmall");
    const nativeRuby = runtime.findWebElement(nativeTarget, "nativeRuby");
    const nativeRubyText = runtime.findWebElement(nativeTarget, "nativeRubyText");
    const nativeRubyParenthesis = runtime.findWebElement(nativeTarget, "nativeRubyParenthesis");
    const nativeBdi = runtime.findWebElement(nativeTarget, "nativeBdi");
    const nativeBdo = runtime.findWebElement(nativeTarget, "nativeBdo");
    const nativeBr = runtime.findWebElement(nativeTarget, "nativeBr");
    const nativeWbr = runtime.findWebElement(nativeTarget, "nativeWbr");
    const nativeDescriptionList = runtime.findWebElement(nativeTarget, "nativeDescriptionList");
    const nativeDescriptionTerm = runtime.findWebElement(nativeTarget, "nativeDescriptionTerm");
    const nativeDescriptionDetails = runtime.findWebElement(nativeTarget, "nativeDescriptionDetails");
    const nativeBase = runtime.findWebElement(nativeTarget, "nativeBase");
    const nativeMeta = runtime.findWebElement(nativeTarget, "nativeMeta");
    const nativeTitle = runtime.findWebElement(nativeTarget, "nativeTitle");
    const nativeStyleElement = runtime.findWebElement(nativeTarget, "nativeStyleElement");
    const nativeVideo = runtime.findWebElement(nativeTarget, "nativeVideo");
    const nativeVideoSource = runtime.findWebElement(nativeTarget, "nativeVideoSource");
    const nativeVideoTrack = runtime.findWebElement(nativeTarget, "nativeVideoTrack");
    const nativeAudio = runtime.findWebElement(nativeTarget, "nativeAudio");
    const nativeFrame = runtime.findWebElement(nativeTarget, "nativeFrame");
    const nativeEmbed = runtime.findWebElement(nativeTarget, "nativeEmbed");
    const nativeObject = runtime.findWebElement(nativeTarget, "nativeObject");
    const nativeParam = runtime.findWebElement(nativeTarget, "nativeParam");
    const nativeScript = runtime.findWebElement(nativeTarget, "nativeScript");
    const nativeNoScript = runtime.findWebElement(nativeTarget, "nativeNoScript");
    const nativeTable = runtime.findWebElement(nativeTarget, "nativeTable");
    const nativeTableCaption = runtime.findWebElement(nativeTarget, "nativeTableCaption");
    const nativeColumns = runtime.findWebElement(nativeTarget, "nativeColumns");
    const nativeColumn = runtime.findWebElement(nativeTarget, "nativeColumn");
    const nativeTableHead = runtime.findWebElement(nativeTarget, "nativeTableHead");
    const nativeTableHeadRow = runtime.findWebElement(nativeTarget, "nativeTableHeadRow");
    const nativeHeaderCell = runtime.findWebElement(nativeTarget, "nativeHeaderCell");
    const nativeTableBody = runtime.findWebElement(nativeTarget, "nativeTableBody");
    const nativeTableBodyRow = runtime.findWebElement(nativeTarget, "nativeTableBodyRow");
    const nativeBodyCell = runtime.findWebElement(nativeTarget, "nativeBodyCell");
    const nativeForm = runtime.findWebElement(nativeTarget, "nativeForm");
    const nativeLabel = runtime.findWebElement(nativeTarget, "nativeLabel");
    const nativeEmail = runtime.findWebElement(nativeTarget, "nativeEmail");
    const nativeSelect = runtime.findWebElement(nativeTarget, "nativeSelect");
    const nativeOptionGroup = runtime.findWebElement(nativeTarget, "nativeOptionGroup");
    const nativeOption = runtime.findWebElement(nativeTarget, "nativeOption");
    const nativeSuggestions = runtime.findWebElement(nativeTarget, "suggestions");
    const nativeSuggestedEmail = runtime.findWebElement(nativeTarget, "suggestedEmail");
    const nativeDetailsElement = runtime.findWebElement(nativeTarget, "nativeDetails");
    const nativeSummary = runtime.findWebElement(nativeTarget, "nativeSummary");
    const nativeDialog = runtime.findWebElement(nativeTarget, "nativeDialog");
    const nativeOutput = runtime.findWebElement(nativeTarget, "nativeOutput");
    const plainCard = runtime.findWebElement(nativeTarget, "plainCard");
    const actionCard = runtime.findWebElement(nativeTarget, "actionCard");
    const fieldset = runtime.findWebElement(nativeTarget, "fieldset");
    const fieldsetAgree = runtime.findWebElement(nativeTarget, "fieldsetAgree");
    const lockedFieldset = runtime.findWebElement(nativeTarget, "lockedFieldset");
    const lockedField = runtime.findWebElement(nativeTarget, "lockedField");
    const autoDetails = runtime.findWebElement(nativeTarget, "autoDetails");
    const autoDialog = runtime.findWebElement(nativeTarget, "autoDialog");
    const email = runtime.findWebElement(nativeTarget, "email");
    const volume = runtime.findWebElement(nativeTarget, "volume");
    const copies = runtime.findWebElement(nativeTarget, "copies");
    const amount = runtime.findWebElement(nativeTarget, "amount");
    const choice = runtime.findWebElement(nativeTarget, "choice");
    const items = runtime.findWebElement(nativeTarget, "items");
    const accent = runtime.findWebElement(nativeTarget, "accent");
    const agree = runtime.findWebElement(nativeTarget, "agree");
    const enabled = runtime.findWebElement(nativeTarget, "enabled");
    const choiceRadio = runtime.findWebElement(nativeTarget, "choiceRadio");
    const upload = runtime.findWebElement(nativeTarget, "upload");
    const storage = runtime.findWebElement(nativeTarget, "storage");
    const rule = runtime.findWebElement(nativeTarget, "rule");
    const table = runtime.findWebElement(nativeTarget, "table");
    const hero = runtime.findWebElement(nativeTarget, "hero");
    const heroMap = runtime.findWebElement(nativeTarget, "heroMap");
    const heroArea = runtime.findWebElement(nativeTarget, "heroArea");
    const nativePictureSource = runtime.findWebElement(nativeTarget, "nativePictureSource");
    const nativeTemplate = runtime.findWebElement(nativeTarget, "nativeTemplate");
    const nativeSlot = runtime.findWebElement(nativeTarget, "nativeSlot");
    const glyph = runtime.findWebElement(nativeTarget, "glyph");
    const bullet = runtime.findWebElement(nativeTarget, "bullet");
    const grid = runtime.findWebElement(nativeTarget, "grid");
    const toolbar = runtime.findWebElement(nativeTarget, "toolbar");
    const segments = runtime.findWebElement(nativeTarget, "segments");
    const tabs = runtime.findWebElement(nativeTarget, "tabs");
    const tree = runtime.findWebElement(nativeTarget, "tree");
    const menu = runtime.findWebElement(nativeTarget, "menu");
    const toast = runtime.findWebElement(nativeTarget, "toast");
    const plot = runtime.findWebElement(nativeTarget, "plot");
    const modalPopup = runtime.findWebElement(nativeTarget, "modalPopup");
    const tipPopup = runtime.findWebElement(nativeTarget, "tipPopup");
    const contextPopup = runtime.findWebElement(nativeTarget, "contextPopup");
    const details = runtime.findWebElement(nativeTarget, "details");
    const dialog = runtime.findWebElement(nativeTarget, "dialog");
    const popover = runtime.findWebElement(nativeTarget, "popover");
    const popoverButton = runtime.findWebElement(nativeTarget, "popoverButton");
    assert.equal(nav.tagName, "NAV");
    assert.equal(title.tagName, "HEADER");
    assert.equal(nativeArticle.tagName, "ARTICLE");
    assert.equal(nativeArticle.textContent, "Release notes");
    assert.equal(nativeHeadingGroup.tagName, "HGROUP");
    assert.equal(nativeHeadingGroupTitle.tagName, "H1");
    assert.equal(nativeSearch.tagName, "SEARCH");
    assert.equal(nativeSearchField.tagName, "INPUT");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/search/field").landmarkOwner.ref,
      "Page/search");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/search").landmarkMembers,
      ["Page/search/field"]);
    assert.equal(nativeAside.tagName, "ASIDE");
    assert.equal(nativeFooter.tagName, "FOOTER");
    assert.equal(nativeFigure.tagName, "FIGURE");
    assert.equal(nativeCaption.tagName, "FIGCAPTION");
    assert.equal(nativeCaption.textContent, "Launch chart");
    assert.equal(nativeList.tagName, "UL");
    assert.equal(nativeListItem.tagName, "LI");
    assert.equal(nativeListItem.textContent, "First");
    assert.equal(nativeOrderedList.tagName, "OL");
    assert.equal(nativeOrderedList.attributes.start, "3");
    assert.equal(nativeOrderedList.attributes.reversed, "");
    assert.equal(nativeOrderedList.attributes.type, "A");
    assert.equal(nativeOrderedItem.tagName, "LI");
    assert.equal(nativeOrderedItem.attributes.value, "3");
    assert.equal(nativeQuote.tagName, "BLOCKQUOTE");
    assert.equal(nativeQuote.attributes.cite, "/notes/native-dom");
    assert.equal(nativeInlineQuote.tagName, "Q");
    assert.equal(nativeInlineQuote.attributes.cite, "/notes/inline");
    assert.equal(nativeInlineQuote.textContent, "Inline quote");
    assert.equal(nativeCodeBlock.tagName, "PRE");
    assert.equal(nativeInlineCode.tagName, "CODE");
    assert.equal(nativeStrong.tagName, "STRONG");
    assert.equal(nativeStrong.textContent, "Important");
    assert.equal(nativeEm.tagName, "EM");
    assert.equal(nativeAbbr.tagName, "ABBR");
    assert.equal(nativeAbbr.attributes.title, "Document Object Model");
    assert.equal(nativeData.tagName, "DATA");
    assert.equal(nativeData.attributes.value, "42");
    assert.equal(nativeDel.tagName, "DEL");
    assert.equal(nativeDel.attributes.cite, "/changes/1");
    assert.equal(nativeDel.attributes.datetime, "2026-09-12");
    assert.equal(nativeIns.tagName, "INS");
    assert.equal(nativeIns.attributes.cite, "/changes/2");
    assert.equal(nativeIns.attributes.datetime, "2026-09-13");
    assert.equal(nativeSub.tagName, "SUB");
    assert.equal(nativeSup.tagName, "SUP");
    assert.equal(nativeKbd.tagName, "KBD");
    assert.equal(nativeSamp.tagName, "SAMP");
    assert.equal(nativeVar.tagName, "VAR");
    assert.equal(nativeCite.tagName, "CITE");
    assert.equal(nativeMark.tagName, "MARK");
    assert.equal(nativeTime.tagName, "TIME");
    assert.equal(nativeTime.attributes.datetime, "2026-09-13");
    assert.equal(nativeAddress.tagName, "ADDRESS");
    assert.equal(nativeAddress.textContent, "hello@example.test");
    assert.equal(nativeSmall.tagName, "SMALL");
    assert.equal(nativeSmall.textContent, "Fine print");
    assert.equal(nativeRuby.tagName, "RUBY");
    assert.equal(nativeRuby.textContent, "\u6f22");
    assert.equal(nativeRubyText.tagName, "RT");
    assert.equal(nativeRubyText.textContent, "kan");
    assert.equal(nativeRubyParenthesis.tagName, "RP");
    assert.equal(nativeRubyParenthesis.textContent, "(");
    assert.equal(nativeBdi.tagName, "BDI");
    assert.equal(nativeBdi.textContent, "\u0645\u0631\u062d\u0628\u0627");
    assert.equal(nativeBdo.tagName, "BDO");
    assert.equal(nativeBdo.attributes.dir, "rtl");
    assert.equal(nativeBdo.attributes.lang, "ar");
    assert.equal(nativeBdo.attributes.translate, "no");
    assert.equal(nativeBr.tagName, "BR");
    assert.equal(nativeWbr.tagName, "WBR");
    assert.equal(nativeDescriptionList.tagName, "DL");
    assert.equal(nativeDescriptionTerm.tagName, "DT");
    assert.equal(nativeDescriptionTerm.textContent, "DOM");
    assert.equal(nativeDescriptionDetails.tagName, "DD");
    assert.equal(nativeDescriptionDetails.textContent, "Document Object Model");
    assert.equal(runtime.webDOMRelations(nativeTarget, "DescriptionTerm").descriptionListOwner.ref,
      "Page/descriptions");
    assert.deepEqual(runtime.webDOMSnapshot(nativeTarget, "DescriptionList")
      .relationRefs.descriptionListItems,
      ["Page/descriptions/dom", "Page/descriptions/dom/details"]);
    assert.equal(nativeBase.tagName, "BASE");
    assert.equal(nativeBase.attributes.href, "https://example.test/");
    assert.equal(nativeBase.attributes.target, "_blank");
    assert.equal(nativeMeta.tagName, "META");
    assert.equal(nativeMeta.attributes.name, "description");
    assert.equal(nativeMeta.attributes.content, "Kry DOM");
    assert.equal(nativeMeta.attributes.charset, "utf-8");
    assert.equal(nativeTitle.tagName, "TITLE");
    assert.equal(nativeTitle.textContent, "Kry document title");
    assert.equal(nativeStyleElement.tagName, "STYLE");
    assert.equal(nativeStyleElement.attributes.media, "screen");
    assert.equal(nativeStyleElement.attributes.nonce, "style-nonce");
    assert.equal(nativeStyleElement.textContent, ".kry-style-probe { color: rgb(1, 2, 3); }");
    assert.equal(nativeVideo.tagName, "VIDEO");
    assert.equal(nativeVideo.attributes.src, "intro.mp4");
    assert.equal(nativeVideo.attributes.poster, "intro.jpg");
    assert.equal(nativeVideo.attributes.controls, "");
    assert.equal(nativeVideo.attributes.preload, "metadata");
    assert.equal(nativeVideoSource.tagName, "SOURCE");
    assert.equal(nativeVideoSource.attributes.src, "intro.webm");
    assert.equal(nativeVideoSource.attributes.type, "video/webm");
    assert.equal(nativeVideoTrack.tagName, "TRACK");
    assert.equal(nativeVideoTrack.attributes.src, "captions.vtt");
    assert.equal(nativeVideoTrack.attributes.kind, "captions");
    assert.equal(nativeVideoTrack.attributes.srclang, "en");
    assert.equal(nativeVideoTrack.attributes.label, "English");
    assert.equal(nativeVideoTrack.attributes.default, "");
    assert.equal(nativeAudio.tagName, "AUDIO");
    assert.equal(nativeAudio.attributes.src, "theme.mp3");
    assert.equal(nativeAudio.attributes.controls, "");
    assert.equal(nativeAudio.attributes.loop, "");
    assert.equal(nativeFrame.tagName, "IFRAME");
    assert.equal(nativeFrame.attributes.src, "/embed");
    assert.equal(nativeFrame.attributes.loading, "lazy");
    assert.equal(nativeFrame.attributes.allow, "fullscreen");
    assert.equal(nativeFrame.attributes.allowfullscreen, "");
    assert.equal(nativeFrame.attributes.sandbox, "allow-scripts");
    assert.equal(nativeFrame.attributes.referrerpolicy, "strict-origin");
    assert.equal(nativeFrame.attributes.credentialless, "");
    assert.equal(nativeFrame.attributes.name, "preview");
    assert.equal(nativeFrame.attributes.width, "640");
    assert.equal(nativeFrame.attributes.height, "360");
    assert.equal(nativeFrame.textContent, "Embedded content");
    assert.equal(nativeEmbed.tagName, "EMBED");
    assert.equal(nativeEmbed.attributes.src, "chart.svg");
    assert.equal(nativeEmbed.attributes.type, "image/svg+xml");
    assert.equal(nativeObject.tagName, "OBJECT");
    assert.equal(nativeObject.attributes.data, "document.pdf");
    assert.equal(nativeObject.attributes.type, "application/pdf");
    assert.equal(nativeObject.attributes.width, "800");
    assert.equal(nativeObject.attributes.height, "600");
    assert.equal(nativeParam.tagName, "PARAM");
    assert.equal(nativeParam.attributes.name, "page");
    assert.equal(nativeParam.attributes.value, "2");
    assert.equal(nativeScript.tagName, "SCRIPT");
    assert.equal(nativeScript.attributes.type, "application/json");
    assert.equal(nativeScript.attributes.nonce, "nonce-1");
    assert.equal(nativeScript.attributes.integrity, "sha256-demo");
    assert.equal(nativeScript.attributes.referrerpolicy, "no-referrer");
    assert.equal(nativeScript.attributes.crossorigin, "anonymous");
    assert.equal(nativeScript.attributes.nomodule, "");
    assert.equal(nativeScript.textContent, "{\"enabled\":true}");
    assert.equal(nativeNoScript.tagName, "NOSCRIPT");
    assert.equal(nativeNoScript.textContent, "Enable JavaScript");
    assert.equal(nativeTable.tagName, "TABLE");
    assert.equal(nativeTableCaption.tagName, "CAPTION");
    assert.equal(nativeTableCaption.textContent, "Totals");
    assert.equal(nativeColumns.tagName, "COLGROUP");
    assert.equal(nativeColumns.attributes.span, "2");
    assert.equal(nativeColumn.tagName, "COL");
    assert.equal(nativeColumn.attributes.span, "1");
    assert.equal(nativeTableHead.tagName, "THEAD");
    assert.equal(nativeTableHeadRow.tagName, "TR");
    assert.equal(nativeHeaderCell.tagName, "TH");
    assert.equal(nativeHeaderCell.attributes.scope, "col");
    assert.equal(nativeHeaderCell.textContent, "Name");
    assert.equal(nativeTableBody.tagName, "TBODY");
    assert.equal(nativeTableBodyRow.tagName, "TR");
    assert.equal(nativeBodyCell.tagName, "TD");
    assert.equal(nativeBodyCell.textContent, "Kryon");
    assert.equal(runtime.webDOMRelations(nativeTarget, "nativeBodyCell").headers[0].ref,
      "Page/nativeTable/head/row/name");
    assert.equal(nativeForm.tagName, "FORM");
    assert.equal(nativeForm.attributes.action, "/signup");
    assert.equal(nativeForm.attributes.method, "post");
    assert.equal(nativeLabel.tagName, "LABEL");
    assert.equal(nativeLabel.attributes.for, "form-email");
    assert.equal(nativeLabel.textContent, "Email");
    assert.equal(nativeEmail.tagName, "INPUT");
    assert.equal(nativeEmail.attributes.id, "form-email");
    assert.equal(nativeSelect.tagName, "SELECT");
    assert.equal(nativeOptionGroup.tagName, "OPTGROUP");
    assert.equal(nativeOptionGroup.attributes.label, "Numbers");
    assert.equal(nativeOption.tagName, "OPTION");
    assert.equal(nativeOption.attributes.value, "1");
    assert.equal(nativeOption.attributes.selected, "");
    assert.equal(nativeSuggestions.tagName, "DATALIST");
    assert.equal(nativeSuggestedEmail.tagName, "OPTION");
    assert.equal(nativeSuggestedEmail.attributes.value, "hello@example.test");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/suggestions/email").collectionOwner.ref,
      "Page/suggestions");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/suggestions").collectionItems,
      ["Page/suggestions/email"]);
    assert.equal(nativeDetailsElement.tagName, "DETAILS");
    assert.equal(nativeDetailsElement.open, true);
    assert.equal(nativeSummary.tagName, "SUMMARY");
    assert.equal(nativeSummary.textContent, "More");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/nativeDetails/summary").summaryOwner.ref,
      "Page/nativeDetails");
    assert.deepEqual(runtime.webDOMSnapshot(nativeTarget, "Page/nativeDetails").relationRefs.summaryItems,
      ["Page/nativeDetails/summary"]);
    assert.equal(nativeDialog.tagName, "DIALOG");
    assert.equal(nativeDialog.open, true);
    assert.equal(nativeOutput.tagName, "OUTPUT");
    assert.equal(nativeOutput.attributes.value, "Ready");
    assert.deepEqual(nativeOutput.attributes.for.split(/\s+/), ["form-email", "kry-Page-email"]);
    assert.equal(nativeOutput.textContent, "Ready");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Output").outputFor,
      ["Page/nativeForm/email", "Page/email"]);
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/email").outputBy,
      ["Page/nativeOutput"]);
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/email").labelledBy, []);
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/list/first").collectionOwner.ref,
      "Page/list");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/ordered/third").collectionOwner.ref,
      "Page/ordered");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/aside/related").landmarkOwner.ref,
      "Page/aside");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/nativeForm/label").labelFor.ref,
      "Page/nativeForm/email");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/nativeForm").formControls,
      ["Page/nativeForm/email"]);
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/nativeSelect/numbers/one").collectionOwner.ref,
      "Page/nativeSelect");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/nativeSelect/numbers/one").groupOwner.ref,
      "Page/nativeSelect/numbers");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/nativeSelect/numbers").groupMembers,
      ["Page/nativeSelect/numbers/one"]);
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/figure/caption").captionOwner.ref,
      "Page/figure");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/figure").captionItems,
      ["Page/figure/caption"]);
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/nativeTable/caption").captionOwner.ref,
      "Page/nativeTable");
    assert.deepEqual(runtime.webDOMSnapshot(nativeTarget, "Page/nativeTable").relationRefs.captionItems,
      ["Page/nativeTable/caption"]);
    assert.equal(plainCard.tagName, "DIV");
    assert.equal(actionCard.tagName, "BUTTON");
    assert.equal(fieldset.tagName, "FIELDSET");
    assert.equal(fieldset.attributes["aria-label"], undefined);
    assert.equal(fieldset.children[0].tagName, "LEGEND");
    assert.equal(fieldset.children[0].textContent, "Preferences");
    assert.equal(runtime.findWebElement(nativeTarget, "fieldsetLegend").tagName, "LEGEND");
    assert.equal(fieldsetAgree.tagName, "INPUT");
    assert.equal(fieldsetAgree.attributes["aria-label"], "Agree");
    assert.equal(runtime.webDOMRelations(nativeTarget, "fieldsetAgree").groupOwner.ref,
      "Page/fieldset");
    assert.deepEqual(runtime.webDOMSnapshot(nativeTarget, "Fieldset").relationRefs.groupMembers,
      ["Page/fieldset/agree"]);
    assert.equal(runtime.webDOMRelations(nativeTarget, "fieldsetLegend").legendOwner.ref,
      "Page/fieldset");
    assert.deepEqual(runtime.webDOMSnapshot(nativeTarget, "Fieldset").relationRefs.legendItems,
      ["Page/fieldset/legend"]);
    assert.equal(lockedFieldset.tagName, "FIELDSET");
    assert.equal(lockedFieldset.attributes.disabled, "");
    assert.equal(lockedFieldset.children[0].tagName, "LEGEND");
    assert.equal(lockedFieldset.children[0].textContent, "Locked");
    assert.equal(lockedField.tagName, "INPUT");
    assert.equal(runtime.webDOMRelations(nativeTarget, "lockedField").disabledOwner.ref,
      "Page/locked");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "lockedFieldset").disabledMembers,
      ["Page/locked/field"]);
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "lockedField")
      .relationRefs.disabledOwner, "Page/locked");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/nav/home").landmarkOwner.ref,
      "Page/nav");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/nav").landmarkMembers,
      ["Page/nav/home"]);
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "Page/nav/home")
      .relationRefs.landmarkOwner, "Page/nav");
    assert.equal(autoDetails.tagName, "DETAILS");
    assert.equal(autoDetails.open, false);
    assert.equal(autoDialog.tagName, "DIALOG");
    assert.equal(email.tagName, "INPUT");
    assert.equal(email.attributes.type, "email");
    assert.equal(email.attributes.placeholder, "Email");
    assert.equal(email.attributes.autocomplete, "email");
    assert.equal(email.attributes.required, "");
    assert.equal(email.attributes.minlength, "3");
    assert.equal(email.attributes.maxlength, "254");
    assert.equal(email.attributes.pattern, ".+@.+");
    assert.equal(email.attributes.inputmode, "email");
    assert.equal(email.attributes.enterkeyhint, "send");
    assert.equal(email.attributes.dirname, "email.dir");
    assert.equal(email.attributes.list, nativeSuggestions.attributes.id);
    assert.equal(email.attributes.list, "kry-Page-suggestions");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Page/email").dataList.ref,
      "Page/suggestions");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "Page/suggestions").listedBy,
      ["Page/email"]);
    nativeBdo.setAttribute("lang", "he");
    assert.equal(runtime.webDOMSync(nativeTarget, "Page/bdo").node.lang, "he");
    assert.equal(runtime.webDOMQuery(nativeTarget, "[placeholder=Email]").element, email);
    assert.equal(runtime.webDOMSetState(nativeTarget, "enabled", "indeterminate", true), true);
    assert.equal(runtime.webDOMQuery(nativeTarget, "Toggle:indeterminate").node.path, "Page/enabled");
    assert.equal(enabled.indeterminate, true);
    assert.equal(enabled.attributes["aria-checked"], "mixed");
    assert.equal(volume.tagName, "INPUT");
    assert.equal(volume.attributes.type, "range");
    assert.equal(volume.attributes.min, "0");
    assert.equal(volume.attributes.max, "10");
    assert.equal(volume.attributes.value, "4");
    assert.equal(volume.attributes["aria-label"], "Volume");
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "volume").valueNow, "4");
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "volume").min, "0");
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "volume").max, "10");
    assert.equal(runtime.webDOMAccessibilitySnapshot(nativeTarget).nodes
      .find((node) => node.path === "Page/volume")?.valueNow, "4");
    assert.equal(runtime.webDOMAccessibilitySnapshot(nativeTarget, "Button[role=tab]").nodes[0]?.role,
      "tab");
    assert.equal(nativeTarget.children[0].kryAccessibilitySnapshot("Selectable").nodes
      .some((node) => node.role === "option"), true);
    assert.equal(copies.tagName, "INPUT");
    assert.equal(copies.attributes.type, "number");
    assert.equal(copies.attributes.min, "1");
    assert.equal(copies.attributes.max, "8");
    assert.equal(copies.attributes.value, "3");
    assert.equal(amount.tagName, "INPUT");
    assert.equal(amount.attributes.type, "number");
    assert.equal(amount.attributes.min, "-10");
    assert.equal(amount.attributes.max, "10");
    assert.equal(amount.attributes.step, "0.5");
    assert.equal(amount.attributes.value, "2.5");
    assert.equal(amount.attributes["aria-label"], "Amount");
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "amount").valueNow, "2.5");
    assert.equal(choice.tagName, "SELECT");
    assert.equal(items.tagName, "SELECT");
    assert.equal(accent.tagName, "INPUT");
    assert.equal(accent.attributes.type, "color");
    assert.equal(accent.attributes.value, "#336699");
    assert.equal(accent.attributes["aria-label"], "Accent");
    assert.equal(agree.tagName, "INPUT");
    assert.equal(agree.attributes.type, "checkbox");
    assert.equal(enabled.tagName, "INPUT");
    assert.equal(enabled.attributes.type, "checkbox");
    assert.equal(choiceRadio.tagName, "INPUT");
    assert.equal(choiceRadio.attributes.type, "radio");
    assert.equal(upload.tagName, "PROGRESS");
    assert.equal(upload.attributes.max, "100");
    assert.equal(upload.attributes.value, "42");
    assert.equal(storage.tagName, "METER");
    assert.equal(storage.attributes.min, "0");
    assert.equal(storage.attributes.max, "1");
    assert.equal(storage.attributes.value, "0.75");
    assert.equal(storage.attributes["aria-label"], "Storage");
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "storage").valueNow, "0.75");
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "storage").role, "meter");
    assert.equal(runtime.webDOMAccessibilitySnapshot(nativeTarget).nodes
      .find((node) => node.path === "Page/storage")?.role, "meter");
    assert.equal(rule.tagName, "HR");
    assert.equal(table.tagName, "TABLE");
    assert.equal(hero.tagName, "IMG");
    assert.equal(hero.attributes.src, "hero.png");
    assert.equal(hero.attributes.alt, "Hero");
    assert.equal(hero.attributes.usemap, "#hero-map");
    assert.equal(hero.attributes.srcset, "hero-small.png 480w, hero.png 960w");
    assert.equal(hero.attributes.sizes, "(max-width: 600px) 480px, 960px");
    assert.equal(hero.attributes.loading, "lazy");
    assert.equal(hero.attributes.decoding, "async");
    assert.equal(hero.attributes.fetchpriority, "high");
    assert.equal(hero.attributes.referrerpolicy, "no-referrer");
    assert.equal(hero.attributes.crossorigin, "anonymous");
    assert.equal(hero.attributes.width, "960");
    assert.equal(hero.attributes.height, "540");
    assert.equal(heroMap.tagName, "MAP");
    assert.equal(heroMap.attributes.name, "hero-map");
    assert.equal(heroArea.tagName, "AREA");
    assert.equal(heroArea.attributes.href, "/hero");
    assert.equal(heroArea.attributes.target, "_self");
    assert.equal(heroArea.attributes.rel, "bookmark");
    assert.equal(heroArea.attributes.download, "hero.txt");
    assert.equal(heroArea.attributes.alt, "Primary region");
    assert.equal(heroArea.attributes.ping, "/hero-audit");
    assert.equal(heroArea.attributes.hreflang, "en");
    assert.equal(heroArea.attributes.referrerpolicy, "same-origin");
    assert.equal(heroArea.attributes.shape, "rect");
    assert.equal(heroArea.attributes.coords, "0,0,100,80");
    assert.equal(runtime.webDOMRelations(nativeTarget, "Image").imageMap.ref,
      "Page/heroMap");
    assert.deepEqual(runtime.webDOMRelationRefs(nativeTarget, "ImageMap").mappedImages,
      ["Page/hero"]);
    assert.equal(nativePictureSource.tagName, "SOURCE");
    assert.equal(nativePictureSource.attributes.srcset, "hero.webp 1x, hero@2x.webp 2x");
    assert.equal(nativeTemplate.tagName, "TEMPLATE");
    assert.equal(nativeSlot.tagName, "SLOT");
    assert.equal(nativeSlot.attributes.name, "actions");
    assert.equal(runtime.webDOMQuery(nativeTarget, "[name=actions]").ref, "Page/slot");
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "hero").styleFacts.src, "hero.png");
    assert.equal(glyph.tagName, "SPAN");
    assert.equal(glyph.attributes.role, "img");
    assert.equal(bullet.tagName, "LI");
    assert.equal(bullet.attributes.role, undefined);
    assert.equal(runtime.webDOMSnapshot(nativeTarget, "bullet").role, "listitem");
    assert.equal(grid.tagName, "CANVAS");
    assert.equal(grid.attributes.role, "img");
    assert.equal(toolbar.attributes.role, "toolbar");
    assert.equal(segments.tagName, "DIV");
    assert.equal(segments.attributes.role, "group");
    assert.equal(tabs.attributes.role, "tablist");
    assert.equal(tree.attributes.role, "tree");
    assert.equal(menu.tagName, "MENU");
    assert.equal(menu.attributes.role, "menu");
    assert.equal(toast.tagName, "OUTPUT");
    assert.equal(toast.attributes["aria-live"], "polite");
    assert.equal(plot.tagName, "CANVAS");
    assert.equal(modalPopup.tagName, "DIALOG");
    assert.equal(modalPopup.open, true);
    assert.equal(modalPopup.attributes.open, "");
    assert.equal(tipPopup.tagName, "DIV");
    assert.equal(tipPopup.attributes.role, "tooltip");
    assert.equal(contextPopup.tagName, "DIV");
    assert.equal(contextPopup.attributes.role, "menu");
    assert.equal(toast.attributes.role, undefined);
    assert.equal(plot.attributes.role, "img");
    assert.equal(details.open, true);
    assert.equal(details.attributes.open, "");
    assert.equal(popover.attributes.popover, "auto");
    assert.equal(popover.id, "kry-Page-popover");
    assert.equal(popoverButton.attributes.popovertarget, popover.id);
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
    assert.equal(dialog.kryShowModal(), true);
    assert.equal(dialog.open, true);
    assert.equal(dialog.kryClose("method"), true);
    assert.equal(dialog.open, false);
    assert.equal(dialog.returnValue, "method");
    assert.deepEqual(nativeEvents.slice(1), ["dialog-cancel", "dialog-close", "dialog-close"]);
    assert.equal(runtime.webDOMShowPopover(nativeTarget, "[data-menu=main]"), true);
    assert.equal(popover.popoverOpen, true);
    assert.equal(runtime.webDOMGetState(nativeTarget, "popover", "open"), true);
    assert.equal(runtime.webDOMTogglePopover(nativeTarget, "popover"), true);
    assert.equal(popover.popoverOpen, false);
    assert.equal(popover.kryShowPopover(), true);
    assert.equal(popover.popoverOpen, true);
    assert.equal(popover.kryTogglePopover(), true);
    assert.equal(popover.popoverOpen, false);
    assert.equal(popover.kryShowPopover(), true);
    assert.equal(popover.kryHidePopover(), true);
    assert.equal(popover.popoverOpen, false);
    assert.equal(runtime.webDOMHidePopover(nativeTarget, "popover"), true);
    assert.equal(runtime.webDOMGetState(nativeTarget, "popover", "open"), false);
    assert.deepEqual(nativeEvents.slice(4), [
      "popover-toggle",
      "popover-toggle",
      "popover-toggle",
      "popover-toggle",
      "popover-toggle",
      "popover-toggle",
      "popover-toggle"
    ]);

    const pointerEvents = [];
    const pointerRt = runtime.createRuntime();
    runtime.beginFrame(pointerRt);
    runtime.widget(pointerRt, "Button", { label: "Hover" }, null,
      {
        nodeName: "hover",
        path: "Page/hover",
        onDoubleClick: "double",
        onMouseEnter: "enter",
        onMouseLeave: "leave",
        onMouseMove: "move",
        onMouseDown: "down",
        onMouseUp: "up",
        onPointerEnter: "p-enter",
        onPointerLeave: "p-leave",
        onPointerMove: "p-move",
        onPointerDown: "p-down",
        onPointerUp: "p-up",
        onPointerCancel: "p-cancel",
        onWheel: "wheel",
        onContextMenu: "context",
        doubleClickAction() { pointerEvents.push("double"); },
        mouseEnterAction() { pointerEvents.push("enter"); },
        mouseLeaveAction() { pointerEvents.push("leave"); },
        mouseMoveAction() { pointerEvents.push("move"); },
        mouseDownAction() { pointerEvents.push("down"); },
        mouseUpAction() { pointerEvents.push("up"); },
        pointerEnterAction() { pointerEvents.push("p-enter"); },
        pointerLeaveAction() { pointerEvents.push("p-leave"); },
        pointerMoveAction() { pointerEvents.push("p-move"); },
        pointerDownAction() { pointerEvents.push("p-down"); },
        pointerUpAction() { pointerEvents.push("p-up"); },
        pointerCancelAction() { pointerEvents.push("p-cancel"); },
        wheelAction(value) { pointerEvents.push("wheel:" + value); },
        contextMenuAction() { pointerEvents.push("context"); }
      });
    runtime.endFrame(pointerRt);
    const pointerTarget = document.createElement("div");
    runtime.renderWebDocument(pointerRt, pointerTarget);
    const pointerButton = runtime.findWebElement(pointerTarget, "hover");
    assert.equal(pointerButton.dataset.kryOnDoubleClick, "double");
    assert.equal(pointerButton.dataset.kryOnMouseEnter, "enter");
    assert.equal(pointerButton.dataset.kryOnMouseLeave, "leave");
    assert.equal(pointerButton.dataset.kryOnMouseMove, "move");
    assert.equal(pointerButton.dataset.kryOnMouseDown, "down");
    assert.equal(pointerButton.dataset.kryOnMouseUp, "up");
    assert.equal(pointerButton.dataset.kryOnPointerEnter, "p-enter");
    assert.equal(pointerButton.dataset.kryOnPointerLeave, "p-leave");
    assert.equal(pointerButton.dataset.kryOnPointerMove, "p-move");
    assert.equal(pointerButton.dataset.kryOnPointerDown, "p-down");
    assert.equal(pointerButton.dataset.kryOnPointerUp, "p-up");
    assert.equal(pointerButton.dataset.kryOnPointerCancel, "p-cancel");
    assert.equal(pointerButton.dataset.kryOnWheel, "wheel");
    assert.equal(pointerButton.dataset.kryOnContextMenu, "context");
    pointerButton.mouseenter();
    pointerButton.mousedown();
    pointerButton.mouseup();
    pointerButton.mouseleave();
    pointerButton.mousemove();
    pointerButton.wheel(12);
    const contextEvent = pointerButton.contextmenu();
    assert.equal(contextEvent.defaultPrevented, true);
    pointerButton.dblclick();
    pointerButton.pointerenter();
    pointerButton.pointerdown();
    pointerButton.pointermove();
    pointerButton.pointerup();
    pointerButton.pointerdown();
    pointerButton.pointercancel();
    pointerButton.pointerleave();
    assert.deepEqual(pointerEvents, [
      "enter", "down", "up", "leave", "move", "wheel:12", "context", "double",
      "p-enter", "p-down", "p-move", "p-up", "p-down", "p-cancel", "p-leave"
    ]);

    const keyEvents = [];
    const keyRt = runtime.createRuntime();
    runtime.beginFrame(keyRt);
    runtime.widget(keyRt, "TextField", {}, null,
      {
        nodeName: "keyField",
        path: "Page/keyField",
        onKeyUp: "release",
        keyUpAction(key) { keyEvents.push("up:" + key); }
      });
    runtime.endFrame(keyRt);
    assert.equal(runtime.webNodeQuery(keyRt, "TextField").onKeyUp, "release");
    const keyTarget = document.createElement("div");
    runtime.renderWebDocument(keyRt, keyTarget);
    const keyField = runtime.findWebElement(keyTarget, "keyField");
    assert.equal(keyField.dataset.kryOnKeyUp, "release");
    keyField.keyup("Enter");
    assert.deepEqual(keyEvents, ["up:Enter"]);

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
    runtime.widget(linkRt, "Link", {
      text: "Manual",
      link: "/manual.pdf",
      target: "_blank",
      rel: "noopener",
      download: "manual.pdf",
      ping: "/audit",
      href_lang: "en",
      referrer_policy: "origin",
      data_tracking_id: "manual-link",
      attr_itemprop: "url",
      dom_id: "manual-link",
      dom_name: "manual_resource",
      title: "Manual PDF",
      tab_index: 2,
      hidden: true,
      draggable: "false",
      contenteditable: "false",
      part: "manual-link",
      slot: "resource-link",
      role: "doc-biblioref",
      aria_label: "Open manual",
      aria_description: "Downloadable PDF",
      aria_details: "manual-details"
    }, null,
      {
        nodeName: "manual",
        path: "Page/manual"
      });
    runtime.endFrame(linkRt);
    assert.equal(runtime.webNodeQuery(linkRt, "Link").href, "/manual.pdf");
    assert.equal(runtime.webNodeQuery(linkRt, "Link").target, "_blank");
    assert.equal(runtime.webNodeQuery(linkRt, "Link").rel, "noopener");
    assert.equal(runtime.webNodeQuery(linkRt, "#manual-link").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[id=\"manual-link\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[name=\"manual_resource\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[title=\"Manual PDF\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[tabindex=\"2\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[download=\"manual.pdf\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[href=\"/manual.pdf\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[target=\"_blank\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[rel=\"noopener\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[ping=\"/audit\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[hreflang=en]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[referrerpolicy=origin]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[data-tracking-id=\"manual-link\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[itemprop=\"url\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[role=\"doc-biblioref\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[aria-label=\"Open manual\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[aria-description=\"Downloadable PDF\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[aria-details=\"manual-details\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[hidden]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[draggable=false]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[part=\"manual-link\"]").path,
      "Page/manual");
    assert.equal(runtime.webNodeQuery(linkRt, "[slot=\"resource-link\"]").path,
      "Page/manual");
    const linkTarget = document.createElement("div");
    runtime.renderWebDocument(linkRt, linkTarget);
    const manual = runtime.findWebElement(linkTarget, "manual");
    assert.equal(manual.attributes.href, "/manual.pdf");
    assert.equal(manual.attributes.target, "_blank");
    assert.equal(manual.attributes.rel, "noopener");
    assert.equal(manual.attributes.download, "manual.pdf");
    assert.equal(manual.attributes.ping, "/audit");
    assert.equal(manual.attributes.hreflang, "en");
    assert.equal(manual.attributes.referrerpolicy, "origin");
    assert.equal(manual.attributes.id, "manual-link");
    assert.equal(manual.attributes.name, "manual_resource");
    assert.equal(manual.attributes.title, "Manual PDF");
    assert.equal(manual.attributes.tabindex, "2");
    assert.equal(manual.attributes["data-tracking-id"], "manual-link");
    assert.equal(manual.attributes.itemprop, "url");
    assert.equal(manual.attributes.role, "doc-biblioref");
    assert.equal(manual.attributes["aria-label"], "Open manual");
    assert.equal(manual.attributes["aria-description"], "Downloadable PDF");
    assert.equal(manual.attributes["aria-details"], "manual-details");
    assert.equal(manual.attributes.hidden, "");
    assert.equal(manual.hidden, true);
    assert.equal(manual.attributes.draggable, "false");
    assert.equal(manual.draggable, false);
    assert.equal(manual.attributes.contenteditable, "false");
    assert.equal(manual.contentEditable, "false");
    assert.equal(manual.attributes.part, "manual-link");
    assert.equal(manual.attributes.slot, "resource-link");

    const tagRt = runtime.createRuntime();
    runtime.beginFrame(tagRt);
    runtime.widget(tagRt, "Section", {
      dom_tag: "article",
      web_ref: "article-ref",
      title: "Article region",
      live: "polite",
      on_click: "open_article",
      on_key_up: "article_key",
      on_beforeinput: "article_before",
      on_dragstart: "article_drag"
    }, null,
      {
        nodeName: "article",
        path: "Page/article"
      });
    runtime.endFrame(tagRt);
    assert.equal(runtime.webNodeQuery(tagRt, "Section").tag, "article");
    assert.equal(runtime.webNodeQuery(tagRt, "Section").ariaLive, "polite");
    assert.equal(runtime.webNodeQuery(tagRt, "Section").onClick, "open_article");
    assert.equal(runtime.webNodeQuery(tagRt, "Section").onKeyUp, "article_key");
    assert.equal(runtime.webNodeQuery(tagRt, "Section").onBeforeInput, "article_before");
    assert.equal(runtime.webNodeQuery(tagRt, "Section").onDragStart, "article_drag");
    assert.equal(runtime.findWebNode(tagRt, "article-ref").path, "Page/article");
    const tagTarget = document.createElement("div");
    runtime.renderWebDocument(tagRt, tagTarget);
    const article = runtime.findWebElement(tagTarget, "article-ref");
    assert.equal(article.tagName, "ARTICLE");
    assert.equal(article.dataset.kryWebRef, "article-ref");
    assert.equal(article.dataset.kryOnClick, "open_article");
    assert.equal(article.dataset.kryOnKeyUp, "article_key");
    assert.equal(article.dataset.kryOnBeforeInput, "article_before");
    assert.equal(article.dataset.kryOnDragStart, "article_drag");
    assert.equal(article.attributes.title, "Article region");
    assert.equal(article.attributes["aria-live"], "polite");

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
    const styledRt = runtime.createRuntime({
      webStyleSheets: `@pack page.meta;
Page { background: #0a141e; }`
    });
    runtime.beginFrame(styledRt);
    runtime.widget(styledRt, "Page", { title: "Styled page" });
    runtime.endFrame(styledRt);
    assert.equal(runtime.webDocumentFrame(styledRt).metadata.themeColor, "#0a141e");

    const domState = generated.createState();
    const domRt = runtime.createRuntime({ app: generated.app });
    runtime.setWebStyleSheets(domRt, webStyleSheet);
    generated.frame(domRt, domState, host);
    const target = document.createElement("div");
    const renderEvents = [];
    const lifecycleEvents = [];
    target.addEventListener("kry-render", (event) => renderEvents.push(event.detail));
    for (const type of ["kry-mount", "kry-update", "kry-unmount"]) {
      target.addEventListener(type, (event) => lifecycleEvents.push({
        type,
        ref: event.kryObject?.ref,
        path: event.kryPath,
        detailRef: event.detail?.object?.ref,
        detailRoot: event.detail?.root,
        detailElement: event.detail?.element
      }));
    }
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
    const emptyTarget = document.createElement("div");
    runtime.renderWebDocument(emptyRt, emptyTarget);
    assert.equal(runtime.webDOMQuery(emptyTarget, "Section:empty").node.path,
      emptyDoc.nodes[1].path);
    const loadingTarget = document.createElement("div");
    runtime.renderWebDocument(loadingRt, loadingTarget);
    assert.equal(runtime.webDOMQuery(loadingTarget, "Section:loading").node.path,
      loadingDoc.nodes[1].path);
    assert.equal(runtime.findWebElement(loadingTarget, "Loading/root/panel").attributes["aria-busy"],
      "true");
    const currentTarget = document.createElement("div");
    runtime.renderWebDocument(currentRt, currentTarget);
    assert.equal(runtime.webDOMQuery(currentTarget, "Link:selected").node.path,
      currentDoc.nodes[1].path);
    assert.equal(runtime.findWebElement(currentTarget, "Current/root/now").attributes["aria-current"],
      "page");
    assert.equal(runtime.webDOMRoot(target), root);
    assert.equal(runtime.webDOMRoot(root), root);
    assert.equal(runtime.webDOMFrame(target).nodes.length, runtime.webDocumentFrame(domRt).nodes.length);
    assert.equal(root.kryRuntime, domRt);
    assert.equal(root.kryFrame.nodes.length, runtime.webDocumentFrame(domRt).nodes.length);
    assert.ok(root.kryObjects.some((object) => object.ref === "primary-action"));
    assert.equal(root.kryObjectMap.get("primary-action").element.dataset.kryRef, "primary-action");
    assert.equal(root.kryObjectMap.get("Scene/root/tap").element.dataset.kryRef, "primary-action");
    assert.equal(root.kryObjectMap.get("tap-button").element.dataset.kryRef, "primary-action");
    assert.equal(root.kryObjectMap.get(tapSourceRef).element.dataset.kryRef, "primary-action");
    assert.equal(root.kryObjectMap.get(tapSourceColumnRef).element.dataset.kryRef, "primary-action");
    assert.equal(Object.keys(root).includes("kryObjects"), false);
    assert.equal(Object.keys(root).includes("kryObjectMap"), false);
    const expectedLifecycleRefs = runtime.webDOMObjects(target).map((object) => object.ref);
    assert.equal(lifecycleEvents.length, expectedLifecycleRefs.length);
    assert.deepEqual(lifecycleEvents.map((event) => event.type),
      expectedLifecycleRefs.map(() => "kry-mount"));
    assert.deepEqual(lifecycleEvents.map((event) => event.ref), expectedLifecycleRefs);
    assert.ok(expectedLifecycleRefs.includes("Scene/root"));
    assert.ok(expectedLifecycleRefs.includes(webDoc.nodes[1].path));
    assert.ok(expectedLifecycleRefs.includes("primary-action"));
    assert.ok(expectedLifecycleRefs.includes("search-box"));
    assert.ok(expectedLifecycleRefs.includes("Scene/root/search_label"));
    assert.equal(lifecycleEvents[2].path, "Scene/root/tap");
    assert.equal(lifecycleEvents[2].detailRef, "primary-action");
    assert.equal(lifecycleEvents[2].detailRoot, root);
    assert.equal(lifecycleEvents[2].detailElement.dataset.kryRef, "primary-action");
    const observedButtons = [];
    const removeButtonObserver = runtime.webDOMObserve(target, "Button.primary",
      (objects, detail) => observedButtons.push([
        objects.map((object) => object.ref),
        detail.root === root,
        detail.frame?.nodes.length || 0,
        detail.event?.type || ""
      ]));
    assert.equal(typeof removeButtonObserver, "function");
    assert.deepEqual(observedButtons, [[["primary-action"], true, webDoc.nodes.length, ""]]);
    const observedFields = [];
    const removeFieldObserver = root.kryObserve("TextField.field",
      (objects, detail) => observedFields.push([objects.map((object) => object.ref), detail.event?.type || ""]),
      { immediate: false });
    assert.equal(typeof removeFieldObserver, "function");
    assert.deepEqual(observedFields, []);
    const lifecycleCount = lifecycleEvents.length;
    runtime.renderWebDocument(domRt, target);
    assert.equal(lifecycleEvents.length, lifecycleCount + expectedLifecycleRefs.length);
    assert.deepEqual(lifecycleEvents.slice(lifecycleCount).map((event) => event.type),
      expectedLifecycleRefs.map(() => "kry-update"));
    assert.deepEqual(observedButtons, [
      [["primary-action"], true, webDoc.nodes.length, ""],
      [["primary-action"], true, webDoc.nodes.length, "kry-render"]
    ]);
    assert.deepEqual(observedFields, [[["search-box"], "kry-render"]]);
    removeButtonObserver();
    removeFieldObserver();
    runtime.renderWebDocument(domRt, target);
    assert.equal(observedButtons.length, 2);
    assert.equal(observedFields.length, 1);
    const boundButtons = [];
    const removeButtonBinding = runtime.webDOMBind(target, "Button.primary", {
      mount(object, detail) {
        boundButtons.push(["mount", object.ref, detail.event?.type || ""]);
        return (cleanupObject, cleanupDetail) => {
          boundButtons.push(["cleanup", cleanupObject.ref, cleanupDetail.event?.type || ""]);
        };
      },
      update(object, detail, previous) {
        boundButtons.push(["update", object.ref, previous?.ref || "", detail.event?.type || ""]);
      },
      unmount(object, detail) {
        boundButtons.push(["unmount", object.ref, detail.event?.type || ""]);
      }
    });
    assert.equal(typeof removeButtonBinding, "function");
    assert.deepEqual(boundButtons, [["mount", "primary-action", ""]]);
    const boundFields = [];
    const removeFieldBinding = root.kryBind("TextField.field",
      (object, detail) => boundFields.push(["mount", object.ref, detail.event?.type || ""]),
      { immediate: false });
    assert.equal(typeof removeFieldBinding, "function");
    assert.deepEqual(boundFields, []);
    runtime.renderWebDocument(domRt, target);
    assert.deepEqual(boundButtons, [
      ["mount", "primary-action", ""],
      ["update", "primary-action", "primary-action", "kry-render"]
    ]);
    assert.deepEqual(boundFields, [["mount", "search-box", "kry-render"]]);
    removeButtonBinding();
    removeFieldBinding();
    assert.deepEqual(boundButtons, [
      ["mount", "primary-action", ""],
      ["update", "primary-action", "primary-action", "kry-render"],
      ["cleanup", "primary-action", ""],
      ["unmount", "primary-action", ""]
    ]);
    runtime.renderWebDocument(domRt, target);
    assert.equal(boundButtons.length, 4);
    assert.equal(boundFields.length, 1);
    const screen = root.children.find((child) => child.tagName === "MAIN");
    const firstText = screen.children[0];
    assert.equal(firstText.tagName, "SPAN");
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
    assert.equal(firstButton.dataset.kryWebRef, "primary-action");
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
    assert.equal(firstButton.krySourceRangeRef, tapSourceRangeRef);
    assert.equal(firstButton.krySourcePath, "src/valid.kry");
    assert.equal(firstButton.krySourceLine, webDoc.nodes[2].sourceLine);
    assert.equal(firstButton.krySourceColumn, webDoc.nodes[2].sourceColumn);
    assert.equal(firstButton.krySourceEndLine, webDoc.nodes[2].sourceEndLine);
    assert.equal(firstButton.krySourceEndColumn, webDoc.nodes[2].sourceEndColumn);
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
    assert.deepEqual(root.kryElements("primary-action"), [firstButton]);
    assert.deepEqual(root.kryElements("Button.primary"), [firstButton]);
    assert.equal(root.kryObject("primary-action").element, firstButton);
    assert.equal(root.kryQuery("Button.primary").element, firstButton);
    assert.deepEqual(root.kryQueryAll("Button.primary").map((object) => object.ref), ["primary-action"]);
    assert.equal(root.kryAtSource("src/valid.kry", webDoc.nodes[2].sourceLine,
      webDoc.nodes[2].sourceColumn).element, firstButton);
    assert.equal(root.krySourceMap.some((object) => object.ref === "primary-action"), true);
    assert.equal(Object.keys(root).includes("kryQuery"), false);
    assert.equal(Object.keys(root).includes("kryAddClass"), false);
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
    assert.equal(firstButton.attributes["aria-owns"], "search-field");
    assert.equal(firstButton.attributes["aria-details"], "kry-Scene-root-search_label");
    assert.equal(firstButton.attributes["aria-errormessage"], "kry-Scene-root-search_label");
    assert.equal(firstButton.attributes["aria-flowto"], "search-field");
    assert.equal(firstButton.attributes.popovertarget, "kry-Scene-root-search_label");
    assert.equal(firstButton.attributes.popovertargetaction, "toggle");
    assert.equal(firstButton.attributes.fetchpriority, "high");
    assert.equal(firstButton.attributes.part, "primary-action");
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(firstButton.style["--kry-background-end"], "#203850");
    assert.equal(firstButton.style.backgroundImage, "linear-gradient(#203040, #203850)");
    assert.equal(firstButton.style.color, "#f0f0f0");
    assert.equal(firstButton.style.borderRadius, "9px");
    assert.equal(firstButton.style.paddingLeft, "13px");
    assert.equal(firstButton.style.outlineWidth, "5px");
    assert.equal(firstButton.style["--kry-offset-y"], "8px");
    assert.equal(firstButton.style.transform,
      "translate(var(--kry-offset-x, 0px), var(--kry-offset-y, 0px))");
    assert.equal(firstButton.dataset.kryState, undefined);
    firstButton.mouseenter();
    assert.equal(firstButton.style.background, "#304050");
    assert.equal(firstButton.style.outlineColor, "#607080");
    assert.equal(firstButton.__kryDocNode.state.hover, true);
    assert.equal(firstButton.dataset.kryState, "hover");
    firstButton.mousedown();
    assert.equal(firstButton.style.background, "#405060");
    assert.equal(firstButton.style.borderWidth, "7px");
    assert.equal(firstButton.__kryDocNode.state.pressed, true);
    assert.equal(firstButton.dataset.kryState, "hover pressed");
    firstButton.mouseup();
    assert.equal(firstButton.style.background, "#304050");
    firstButton.mouseleave();
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(firstButton.__kryDocNode.state.hover, false);
    assert.equal(firstButton.dataset.kryState, undefined);
    assert.equal(runtime.webDOMFocus(target, "tap-button"), true);
    assert.equal(firstButton.style.borderColor, "#506070");
    assert.equal(firstButton.__kryDocNode.state.focus, true);
    assert.equal(firstButton.kryBlur(), true);
    assert.equal(firstButton.__kryDocNode.state.focus, false);
    assert.equal(root.kryFocus("tap-button"), true);
    assert.equal(firstButton.__kryDocNode.state.focus, true);
    assert.equal(root.kryBlur("tap-button"), true);
    assert.equal(firstButton.__kryDocNode.state.focus, false);
    assert.equal(firstButton.kryFocus(), true);
    assert.equal(firstButton.__kryDocNode.state.focus, true);
    assert.equal(runtime.webDOMBlur(target, "Scene/root/tap"), true);
    assert.equal(firstButton.style.borderColor, "");
    assert.equal(runtime.findWebNode(domRt, "Scene/root/tap").domId, "tap-button");
    assert.equal(runtime.findWebElement(target, "Scene/root/tap"), firstButton);
    assert.equal(runtime.findWebElement(target, "tap"), firstButton);
    assert.equal(runtime.findWebElement(target, "tap-button"), firstButton);
    assert.equal(runtime.findWebElement(target, tapSourceRef), firstButton);
    assert.equal(runtime.findWebElement(target, tapSourceColumnRef), firstButton);
    assert.deepEqual(runtime.findWebElements(target, "tap-button"), [firstButton]);
    assert.deepEqual(runtime.findWebElements(target, "Button.primary"), [firstButton]);
    assert.equal(runtime.webDOMObject(target, "Scene/root/tap").element, firstButton);
    assert.equal(runtime.webDOMObject(target, "tap-button").node.path, "Scene/root/tap");
    assert.equal(runtime.webDOMIdentity(target, "primary-action").domId, "tap-button");
    assert.equal(root.kryIdentity("primary-action").domId, "tap-button");
    assert.equal(root.krySnapshot("primary-action").identity.ref, "primary-action");
    assert.deepEqual(runtime.webDOMIdentity(target, "tap-button").aliases.slice(0, 4), [
      "primary-action",
      "Scene/root/tap",
      "tap",
      "tap-button"
    ]);
    assert.equal(runtime.webDOMObject(target, tapSourceRef).element, firstButton);
    assert.equal(runtime.webDOMObject(target, tapSourceRef).ref, tapSourceRef);
    assert.equal(runtime.webDOMObject(target, tapSourceColumnRef).ref, tapSourceColumnRef);
    const buttonObject = runtime.webDOMObject(target, "tap-button");
    assert.equal(buttonObject.root, root);
    assert.equal(buttonObject.identity.ref, "primary-action");
    assert.equal(buttonObject.snapshot.parentRef, "Scene/root");
    assert.deepEqual(buttonObject.snapshot.aliases.slice(0, 4), [
      "primary-action",
      "Scene/root/tap",
      "tap",
      "tap-button"
    ]);
    assert.equal(buttonObject.snapshot.identity.sourceRangeRef, tapSourceRangeRef);
    assert.equal(buttonObject.styleFacts.kind, "Button");
    assert.equal(firstButton.kryStyleFacts.kind, "Button");
    assert.equal(root.kryStyleFacts("tap-button").kind, "Button");
    assert.equal(runtime.webDOMStyleFacts(target, "tap-button").kind, "Button");
    assert.equal(buttonObject.styleTrace.resolved.background, "#203040");
    assert.equal(firstButton.kryStyleTrace.resolved.background, "#203040");
    assert.equal(root.kryStyleTrace("tap-button").resolved.background, "#203040");
    assert.equal(runtime.webDOMStyleTrace(target, "tap-button").resolved.background, "#203040");
    assert.equal(Object.keys(buttonObject).includes("styleTrace"), false);
    assert.equal(buttonObject.snapshot.relationRefs.previousSibling, webDoc.nodes[1].path);
    assert.equal(buttonObject.snapshot.relationRefs.nextSibling, "search-box");
    assert.deepEqual(buttonObject.snapshot.relationRefs.controls, ["search-box"]);
    assert.deepEqual(buttonObject.snapshot.relationRefs.owns, ["search-box"]);
    assert.equal(buttonObject.snapshot.relationRefs.details, "Scene/root/search_label");
    assert.equal(buttonObject.snapshot.relationRefs.errorMessage, "Scene/root/search_label");
    assert.deepEqual(buttonObject.snapshot.relationRefs.flowTo, ["search-box"]);
    assert.equal(buttonObject.snapshot.relationRefs.popoverTarget, "Scene/root/search_label");
    assert.equal(buttonObject.snapshot.eventRefs.click, "call_host");
    assert.equal(buttonObject.snapshot.eventRefs.keyUp, "");
    assert.equal(buttonObject.eventRefs.click, "call_host");
    assert.equal(firstButton.kryEventRefs.click, "call_host");
    assert.equal(root.kryEventRefs("tap-button").click, "call_host");
    assert.equal(runtime.webDOMEventRefs(target, "tap-button").click, "call_host");
    assert.equal(runtime.webDOMEventRefs(target, "tap-button").keyUp, "");
    assert.equal(buttonObject.parent.node.path, "Scene/root");
    assert.deepEqual(buttonObject.ancestors.map((object) => object.ref), ["Scene/root"]);
    assert.deepEqual(firstButton.kryAncestors.map((object) => object.ref), ["Scene/root"]);
    assert.deepEqual(root.kryAncestors("tap-button").map((object) => object.ref), ["Scene/root"]);
    assert.deepEqual(runtime.webDOMAncestors(target, "tap-button").map((object) => object.ref),
      ["Scene/root"]);
    assert.equal(buttonObject.previousSibling.ref, webDoc.nodes[1].path);
    assert.equal(buttonObject.nextSibling.ref, "search-box");
    assert.equal(firstButton.kryPreviousSibling.ref, webDoc.nodes[1].path);
    assert.equal(firstButton.kryNextSibling.ref, "search-box");
    assert.equal(root.kryPreviousSibling("tap-button").ref, webDoc.nodes[1].path);
    assert.equal(root.kryNextSibling("tap-button").ref, "search-box");
    assert.equal(runtime.webDOMPreviousSibling(target, "tap-button").ref, webDoc.nodes[1].path);
    assert.equal(runtime.webDOMNextSibling(target, "tap-button").ref, "search-box");
    assert.deepEqual(buttonObject.previousSiblings.map((object) => object.ref),
      [webDoc.nodes[1].path]);
    assert.equal(buttonObject.nextSiblings[0].ref, "search-box");
    assert.equal(buttonObject.siblings.some((object) => object.ref === "primary-action"), false);
    assert.deepEqual(firstButton.kryPreviousSiblings.map((object) => object.ref),
      [webDoc.nodes[1].path]);
    assert.equal(firstButton.kryNextSiblings[0].ref, "search-box");
    assert.equal(firstButton.krySiblings.some((object) => object.ref === "primary-action"), false);
    assert.deepEqual(root.kryPreviousSiblings("tap-button").map((object) => object.ref),
      [webDoc.nodes[1].path]);
    assert.equal(root.kryNextSiblings("tap-button")[0].ref, "search-box");
    assert.equal(root.krySiblings("tap-button").some((object) => object.ref === "primary-action"), false);
    assert.deepEqual(runtime.webDOMPreviousSiblings(target, "tap-button").map((object) => object.ref),
      [webDoc.nodes[1].path]);
    assert.equal(runtime.webDOMNextSiblings(target, "tap-button")[0].ref, "search-box");
    assert.equal(runtime.webDOMSiblings(target, "tap-button").some((object) => object.ref === "primary-action"), false);
    assert.deepEqual(buttonObject.children.map((object) => object.ref), []);
    assert.equal(buttonObject.relations.previousSibling.ref, webDoc.nodes[1].path);
    assert.equal(buttonObject.relations.nextSibling.ref, "search-box");
    assert.equal(buttonObject.relationRefs.previousSibling, webDoc.nodes[1].path);
    assert.equal(buttonObject.relationRefs.nextSibling, "search-box");
    assert.deepEqual(buttonObject.relations.controls.map((object) => object.ref), ["search-box"]);
    assert.deepEqual(buttonObject.relations.owns.map((object) => object.ref), ["search-box"]);
    assert.equal(buttonObject.relations.details.ref, "Scene/root/search_label");
    assert.equal(buttonObject.relations.errorMessage.ref, "Scene/root/search_label");
    assert.deepEqual(buttonObject.relations.flowTo.map((object) => object.ref), ["search-box"]);
    assert.equal(buttonObject.relations.popoverTarget.ref, "Scene/root/search_label");
    assert.deepEqual(root.kryRelations("tap-button").controls.map((object) => object.ref), ["search-box"]);
    assert.equal(firstButton.kryRelations.controls[0].ref, "search-box");
    assert.deepEqual(buttonObject.relationRefs.controls, ["search-box"]);
    assert.deepEqual(firstButton.kryRelationRefs.controls, ["search-box"]);
    assert.deepEqual(root.kryRelationRefs("tap-button").controls, ["search-box"]);
    assert.deepEqual(runtime.webDOMRelations(target, "search-box").controlledBy
      .map((object) => object.ref), ["primary-action"]);
    assert.deepEqual(runtime.webDOMRelations(target, "search-box").ownedBy
      .map((object) => object.ref), ["primary-action"]);
    assert.deepEqual(runtime.webDOMRelations(target, "search-box").labelledBy
      .map((object) => object.ref), ["Scene/root/search_label"]);
    assert.deepEqual(runtime.webDOMSnapshot(target, "search-box").relationRefs.labelledBy,
      ["Scene/root/search_label"]);
    assert.deepEqual(runtime.webDOMSnapshot(target, "search-box").relationRefs.controlledBy,
      ["primary-action"]);
    assert.deepEqual(runtime.webDOMSnapshot(target, "search-box").relationRefs.ownedBy,
      ["primary-action"]);
    assert.deepEqual(runtime.webNodeRelationRefs(rt, "search-box").controlledBy,
      ["primary-action"]);
    assert.deepEqual(runtime.webNodeRelationRefs(rt, "search-box").ownedBy,
      ["primary-action"]);
    assert.equal(runtime.webNodeRelationRefs(rt, "search-box").details,
      "Scene/root/search_label");
    assert.equal(runtime.webNodeRelationRefs(rt, "search-box").errorMessage,
      "Scene/root/search_label");
    assert.deepEqual(runtime.webNodeRelationRefs(rt, "search-box").flowTo,
      ["primary-action"]);
    assert.equal(runtime.webNodeRelations(rt, "primary-action").previousSibling.path,
      webDoc.nodes[1].path);
    assert.equal(runtime.webNodeRelations(rt, "primary-action").nextSibling.path,
      webDoc.nodes[3].path);
    assert.deepEqual(runtime.webNodeAncestors(rt, "primary-action").map((node) => node.path),
      ["Scene/root"]);
    assert.equal(runtime.webNodePreviousSibling(rt, "primary-action").path,
      webDoc.nodes[1].path);
    assert.equal(runtime.webNodeNextSibling(rt, "primary-action").path,
      webDoc.nodes[3].path);
    assert.deepEqual(runtime.webNodePreviousSiblings(rt, "primary-action").map((node) => node.path),
      [webDoc.nodes[1].path]);
    assert.equal(runtime.webNodeNextSiblings(rt, "primary-action")[0].path,
      webDoc.nodes[3].path);
    assert.equal(runtime.webNodeSiblings(rt, "primary-action")
      .some((node) => node.webRef === "primary-action"), false);
    assert.equal(runtime.webNodeRelationRefs(rt, "primary-action").previousSibling,
      webDoc.nodes[1].path);
    assert.equal(runtime.webNodeRelationRefs(rt, "primary-action").nextSibling,
      "search-box");
    assert.deepEqual(runtime.webNodeRelations(rt, "search-box").controlledBy
      .map((node) => node.webRef),
      ["primary-action"]);
    assert.deepEqual(runtime.webDOMRelationRefs(target, "search-box").controlledBy,
      ["primary-action"]);
    assert.deepEqual(runtime.webDOMRelationRefs(target, "search-box").ownedBy,
      ["primary-action"]);
    assert.deepEqual(runtime.webDOMRelations(target, "Scene/root/search_label").popoverInvokers
      .map((object) => object.ref), ["primary-action"]);
    assert.deepEqual(runtime.webDOMSnapshot(target, "Scene/root/search_label").relationRefs.popoverInvokers,
      ["primary-action"]);
    assert.equal(buttonObject.matches("Button.primary"), true);
    assert.equal(buttonObject.closest("Screen").node.path, "Scene/root");
    assert.equal(Object.keys(buttonObject).includes("root"), false);
    assert.equal(Object.keys(buttonObject).includes("identity"), false);
    assert.equal(Object.keys(buttonObject).includes("matches"), false);
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
    assert.equal(targetEvent.kryParent.ref, "Scene/root");
    assert.deepEqual(targetEvent.kryAncestors.map((object) => object.ref), ["Scene/root"]);
    assert.equal(targetEvent.kryPreviousSibling.ref, webDoc.nodes[1].path);
    assert.equal(targetEvent.kryNextSibling.ref, "search-box");
    assert.deepEqual(targetEvent.kryPreviousSiblings.map((object) => object.ref),
      [webDoc.nodes[1].path]);
    assert.equal(targetEvent.kryNextSiblings[0].ref, "search-box");
    assert.equal(targetEvent.krySiblings.some((object) => object.ref === "primary-action"), false);
    assert.deepEqual(targetEvent.kryChildren.map((object) => object.ref), []);
    assert.deepEqual(targetEvent.kryDescendants.map((object) => object.ref), []);
    assert.equal(targetEvent.kryMatches("Button.primary"), true);
    assert.equal(targetEvent.kryClosest("Screen").ref, "Scene/root");
    assert.equal(targetEvent.kryQuery("Text"), null);
    assert.deepEqual(targetEvent.kryQueryAll("Text"), []);
    assert.equal(targetEvent.kryEventRefs.click, "call_host");
    assert.deepEqual(targetEvent.kryRelations.controls.map((object) => object.ref), ["search-box"]);
    assert.deepEqual(targetEvent.kryRelationRefs.controls, ["search-box"]);
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
    assert.equal(buttonSnapshot.eventRefs.click, "call_host");
    assert.equal(buttonSnapshot.element, undefined);
    assert.equal(buttonSnapshot.role, "button");
    assert.equal(buttonSnapshot.parentRef, "Scene/root");
    assert.equal(buttonSnapshot.attrs.id, "tap-button");
    assert.equal(buttonSnapshot.dataset.kryPath, "Scene/root/tap");
    assert.equal(buttonSnapshot.styleFacts.ref, "primary-action");
    assert.equal(buttonSnapshot.styleFacts.kind, "Button");
    assert.deepEqual(buttonSnapshot.styleFacts.classes, ["primary", "action"]);
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
    assert.deepEqual(root.krySnapshots("Button.primary").map((snapshot) => snapshot.ref),
      ["primary-action"]);
    assert.equal(Object.keys(root).includes("krySnapshots"), false);
    assert.equal(runtime.webDOMElementMatches(nestedSpan, "Button.primary"), true);
    assert.equal(runtime.webDOMMatches(target, "tap-button", "Button.primary"), true);
    assert.equal(runtime.webDOMMatches(target, "tap-button", "TextField"), false);
    const objectMethodEvents = [];
    const removeObjectMethod = buttonObject.listen("kry-object-method",
      (event, object) => objectMethodEvents.push([event.kryRef, object?.ref]));
    assert.equal(typeof removeObjectMethod, "function");
    firstButton.dispatchEvent({ type: "kry-object-method" });
    assert.deepEqual(objectMethodEvents, [["primary-action", "primary-action"]]);
    removeObjectMethod();
    firstButton.dispatchEvent({ type: "kry-object-method" });
    assert.deepEqual(objectMethodEvents, [["primary-action", "primary-action"]]);
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
        "Scene/root/search_label",
        selectablePath,
        ...inputPaths
      ]);
    assert.deepEqual(runtime.webDOMChildren(target).map((object) => object.node.path),
      ["Scene/root"]);
    assert.deepEqual(runtime.webDOMDescendants(target, "Scene/root")
      .map((object) => object.node.path), [
        firstText.dataset.kryPath,
        "Scene/root/tap",
        "Scene/root/search",
        "Scene/root/search_label",
        selectablePath,
        ...inputPaths
      ]);
    assert.equal(runtime.webDOMQueryWithin(target, "Scene/root", "Button.primary").element,
      firstButton);
    assert.deepEqual(runtime.webDOMQueryAllWithin(target, "Scene/root", "Input")
      .map((object) => object.node.path), inputPaths);
    assert.equal(runtime.webDOMQueryWithin(target, "Scene/root/tap", "TextField"), null);
    assert.equal(root.kryQueryWithin("Scene/root", "TextField.field").element,
      runtime.findWebElement(target, "q"));
    assert.deepEqual(root.kryQueryAllWithin("Scene/root", "Input")
      .map((object) => object.node.path), inputPaths);
    assert.deepEqual(root.kryDescendants("Scene/root")
      .map((object) => object.node.path), [
        firstText.dataset.kryPath,
        "Scene/root/tap",
        "Scene/root/search",
        "Scene/root/search_label",
        selectablePath,
        ...inputPaths
      ]);
    assert.deepEqual(screen.kryDescendants().map((object) => object.node.path), [
      firstText.dataset.kryPath,
      "Scene/root/tap",
      "Scene/root/search",
      "Scene/root/search_label",
      selectablePath,
      ...inputPaths
    ]);
    assert.equal(screen.kryQuery("Button.primary").element, firstButton);
    assert.deepEqual(screen.kryQueryAll("Input").map((object) => object.node.path), inputPaths);
    const screenObject = runtime.webDOMObject(target, "Scene/root");
    assert.equal(screenObject.query("Button.primary").element, firstButton);
    assert.deepEqual(screenObject.queryAll("Input").map((object) => object.node.path), inputPaths);
    assert.deepEqual(screenObject.descendants.map((object) => object.node.path),
      screen.kryDescendants().map((object) => object.node.path));
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
    assert.deepEqual(root.kryRect("tap-button"), {
      x: 10,
      y: 50,
      width: 120,
      height: 28,
      left: 10,
      top: 50,
      right: 130,
      bottom: 78
    });
    assert.deepEqual(firstButton.kryRect(), {
      x: 10,
      y: 50,
      width: 120,
      height: 28,
      left: 10,
      top: 50,
      right: 130,
      bottom: 78
    });
    assert.deepEqual(buttonObject.rect(), {
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
    assert.equal(runtime.webDOMQuery(target, "[webRef^=\"primary\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[webRef$=\"action\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[webRef*=\"ary-act\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[data.tracking_id|=\"tap\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[aria.controls~=\"search-box\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "Screen > Button[webRef=\"primary-action\"]").element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, "Screen TextField").element,
      runtime.findWebElement(target, "search-box"));
    assert.equal(runtime.webDOMQuery(target, "Screen > Text:first-child").element,
      firstText);
    assert.equal(runtime.webDOMQuery(target, "Screen > Input:last-child").node.path,
      inputPaths[inputPaths.length - 1]);
    assert.equal(runtime.webDOMQuery(target, "Screen > Button:nth-child(2)").element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, "Screen > Button:nth-last-child(6)").element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, "Screen > Text:nth-child(odd)").element,
      firstText);
    assert.equal(runtime.webDOMQuery(target, "Screen > Input:nth-child(even)").node.path,
      inputPaths[0]);
    assert.equal(runtime.webDOMQuery(target, "Screen > Text:nth-child(2n+1)").element,
      firstText);
    assert.equal(runtime.webDOMQuery(target, "Screen > Text:nth-last-child(-n+4)").ref,
      "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "Screen > Text:first-of-type").element,
      firstText);
    assert.equal(runtime.webDOMQuery(target, "Screen > Text:last-of-type").ref,
      "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "Screen > Button:only-of-type").element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, "Screen > Input:nth-of-type(1)").node.path,
      inputPaths[0]);
    assert.equal(runtime.webDOMQuery(target, "Screen > Input:nth-of-type(n+2)").node.path,
      inputPaths[inputPaths.length - 1]);
    assert.equal(runtime.webDOMQuery(target, "Screen > Input:nth-last-of-type(2n)").node.path,
      inputPaths[0]);
    assert.equal(runtime.webDOMQuery(target, "Screen > Input:nth-last-of-type(1)").node.path,
      inputPaths[inputPaths.length - 1]);
    assert.equal(runtime.webDOMQuery(target, "Button:not(.secondary)").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, ":is(Button, TextField)[webRef^=\"primary\"]").element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, ":where(TextField, Button)[data.tracking_id|=\"tap\"]").element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, "Screen:root").node.path, "Scene/root");
    assert.equal(runtime.webDOMQueryWithin(target, "Scene/root", ":scope > Button").element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, "Text + Button").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "Button ~ Input").node.path, inputPaths[0]);
    assert.equal(runtime.webDOMQuery(target, "#tap-button").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[value=\"tap-value\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[index=2]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[domValue=\"tap-value\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[role=button]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[data-tracking-id=\"tap-1\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[aria-current=page]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[aria.pressed=false]").element, firstButton);
    firstButton.className += " native-selected";
    firstButton.setAttribute("data-native", "yes");
    firstButton.disabled = true;
    firstButton.textContent = "Native";
    const nativeSynced = root.krySync("tap-button");
    assert.equal(nativeSynced.ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "Button.native-selected").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "[data-native=\"yes\"]").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "Button:disabled").element, firstButton);
    assert.equal(runtime.webDOMGetText(target, "tap-button"), "Native");
    firstButton.className = firstButton.className.replace(/\bnative-selected\b/g, "").trim();
    firstButton.removeAttribute("data-native");
    firstButton.disabled = false;
    firstButton.textContent = "Tap";
    assert.equal(firstButton.krySync().ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "Button.native-selected"), null);
    assert.equal(runtime.webDOMQuery(target, "[data-native=\"yes\"]"), null);
    assert.equal(runtime.webDOMQuery(target, "Button:disabled"), null);
    firstButton.setAttribute("title", "Native title");
    assert.equal(buttonObject.sync().node.title, "Native title");
    assert.equal(runtime.webDOMSync(target, "tap-button").node.title, "Native title");
    firstButton.setAttribute("title", "Tap details");
    assert.equal(runtime.webDOMSync(target).some((object) => object.ref === "primary-action"), true);
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
    assert.equal(root.kryAddClass("tap-button", "root-selected"), true);
    assert.equal(root.kryHasClass("Scene/root/tap", "root-selected"), true);
    assert.equal(runtime.webDOMQuery(target, "Button.root-selected").element, firstButton);
    assert.equal(root.kryToggleClass("tap-button", "root-selected"), true);
    assert.equal(root.kryHasClass("tap-button", "root-selected"), false);
    assert.equal(root.kryToggleClass("tap-button", "root-selected", true), true);
    assert.equal(root.kryRemoveClass("tap-button", "root-selected"), true);
    assert.equal(root.kryHasClass("tap-button", "root-selected"), false);
    assert.equal(firstButton.kryAddClass("method-selected"), true);
    assert.equal(firstButton.kryHasClass("method-selected"), true);
    assert.equal(runtime.webDOMQuery(target, "Button.method-selected").element, firstButton);
    runtime.renderWebDocument(domRt, target);
    assert.equal(firstButton.kryHasClass("method-selected"), true);
    assert.equal(firstButton.kryToggleClass("method-selected"), true);
    assert.equal(firstButton.kryHasClass("method-selected"), false);
    assert.equal(firstButton.kryToggleClass("method-selected", true), true);
    assert.equal(firstButton.kryRemoveClass("method-selected"), true);
    assert.equal(firstButton.kryHasClass("method-selected"), false);
    assert.equal(Object.keys(firstButton).includes("kryAddClass"), false);
    assert.equal(buttonObject.addClass("object-selected"), true);
    assert.equal(buttonObject.hasClass("object-selected"), true);
    assert.equal(runtime.webDOMQuery(target, "Button.object-selected").element, firstButton);
    assert.equal(buttonObject.toggleClass("object-selected"), true);
    assert.equal(buttonObject.hasClass("object-selected"), false);
    assert.equal(buttonObject.toggleClass("object-selected", true), true);
    assert.equal(buttonObject.removeClass("object-selected"), true);
    assert.equal(buttonObject.hasClass("object-selected"), false);
    assert.equal(runtime.webDOMSetAttribute(target, "tap-button", "data-runtime", "1"), true);
    assert.equal(runtime.webDOMGetAttribute(target, "Scene/root/tap", "data-runtime"), "1");
    assert.equal(runtime.webDOMHasAttribute(target, "tap-button", "data-runtime"), true);
    assert.equal(runtime.webDOMQuery(target, "[data-runtime=\"1\"]").element, firstButton);
    assert.equal(root.krySetAttr("tap-button", "data-root", "4"), true);
    assert.equal(root.kryGetAttr("tap-button", "data-root"), "4");
    assert.equal(root.kryHasAttr("tap-button", "data-root"), true);
    assert.equal(runtime.webDOMQuery(target, "[data-root=\"4\"]").element, firstButton);
    assert.equal(root.kryRemoveAttr("tap-button", "data-root"), true);
    assert.equal(root.kryHasAttr("tap-button", "data-root"), false);
    assert.equal(firstButton.krySetAttr("data-local", "2"), true);
    assert.equal(firstButton.kryGetAttr("data-local"), "2");
    assert.equal(firstButton.kryHasAttr("data-local"), true);
    assert.equal(runtime.webDOMQuery(target, "[data-local=\"2\"]").element, firstButton);
    assert.equal(firstButton.kryRemoveAttr("data-local"), true);
    assert.equal(firstButton.kryHasAttr("data-local"), false);
    assert.equal(Object.keys(firstButton).includes("krySetAttr"), false);
    assert.equal(buttonObject.setAttr("data-object", "3"), true);
    assert.equal(buttonObject.getAttr("data-object"), "3");
    assert.equal(buttonObject.hasAttr("data-object"), true);
    assert.equal(runtime.webDOMQuery(target, "[data-object=\"3\"]").element, firstButton);
    assert.equal(buttonObject.removeAttr("data-object"), true);
    assert.equal(buttonObject.hasAttr("data-object"), false);
    assert.equal(firstButton.style.paddingTop, "9px");
    assert.equal(runtime.webDOMSetStyle(target, "tap-button", "background", "pink"), true);
    assert.equal(runtime.webDOMSetStyle(target, "tap-button", "--accent-level", "2"), true);
    assert.equal(runtime.webDOMGetStyle(target, "Scene/root/tap", "background"), "pink");
    assert.equal(runtime.webDOMComputedStyle(target, "Scene/root/tap", "background"), "pink");
    assert.equal(firstButton.kryComputedStyle("background"), "pink");
    assert.equal(root.kryComputedStyle("tap-button", "background"), "pink");
    assert.equal(runtime.webDOMGetStyle(target, "tap-button", "--accent-level"), "2");
    assert.equal(runtime.webDOMComputedStyle(target, "tap-button", "--accent-level"), "2");
    assert.equal(firstButton.kryComputedStyle("--accent-level"), "2");
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
    assert.equal(root.krySetStyle("tap-button", "background", "seagreen"), true);
    assert.equal(root.kryGetStyle("tap-button", "background"), "seagreen");
    assert.equal(root.kryRemoveStyle("tap-button", "background"), true);
    assert.equal(firstButton.style.background, "#203040");
    assert.equal(buttonObject.setStyle("background", "cornflowerblue"), true);
    assert.equal(buttonObject.getStyle("background"), "cornflowerblue");
    assert.equal(buttonObject.computedStyle("background"), "cornflowerblue");
    assert.equal(buttonObject.removeStyle("background"), true);
    assert.equal(buttonObject.computedStyle("background"), "#203040");
    assert.equal(runtime.webDOMGetAttribute(target, "tap-button", "data-runtime"), "1");
    assert.equal(runtime.webDOMRemoveAttribute(target, "tap-button", "data-runtime"), true);
    assert.equal(runtime.webDOMHasAttribute(target, "tap-button", "data-runtime"), false);
    assert.equal(runtime.webDOMGetState(target, "tap-button", "disabled"), false);
    assert.equal(runtime.webDOMSetProperty(target, "tap-button", "disabled", true), true);
    assert.equal(runtime.webDOMGetProperty(target, "tap-button", "disabled"), true);
    assert.equal(root.kryGetProp("tap-button", "disabled"), true);
    assert.equal(firstButton.kryGetProp("disabled"), true);
    assert.equal(runtime.webDOMGetState(target, "tap-button", "disabled"), true);
    assert.equal(runtime.webDOMSetProperty(target, "tap-button", "disabled", false), true);
    assert.equal(root.krySetProp("tap-button", "disabled", true), true);
    assert.equal(root.kryGetProp("tap-button", "disabled"), true);
    assert.equal(firstButton.krySetProp("disabled", true), true);
    assert.equal(firstButton.kryGetProp("disabled"), true);
    assert.equal(runtime.webDOMSetState(target, "tap-button", "disabled", true), true);
    assert.equal(runtime.webDOMGetState(target, "Scene/root/tap", "disabled"), true);
    assert.equal(root.kryGetState("tap-button", "disabled"), true);
    assert.equal(root.kryToggleState("tap-button", "disabled"), true);
    assert.equal(root.kryGetState("tap-button", "disabled"), false);
    assert.equal(root.krySetState("tap-button", "disabled", true), true);
    assert.equal(firstButton.kryGetState("disabled"), true);
    assert.equal(firstButton.krySetState("disabled", false), true);
    assert.equal(firstButton.kryGetState("disabled"), false);
    assert.equal(firstButton.krySetState("disabled", true), true);
    assert.equal(buttonObject.getState("disabled"), true);
    assert.equal(buttonObject.setState("disabled", false), true);
    assert.equal(buttonObject.getState("disabled"), false);
    assert.equal(buttonObject.setState("disabled", true), true);
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
    assert.equal(firstButton.krySetProp("textContent", "SetProp"), true);
    assert.equal(firstButton.kryGetProp("textContent"), "SetProp");
    assert.equal(runtime.webDOMGetText(target, "tap-button"), "SetProp");
    assert.equal(runtime.webDOMSetText(target, "tap-button", "Launch"), true);
    assert.equal(runtime.webDOMGetText(target, "Scene/root/tap"), "Launch");
    assert.equal(runtime.webDOMObject(target, "tap-button").node.text, "Launch");
    assert.equal(root.kryText("tap-button"), "Launch");
    assert.equal(root.kryText("tap-button", "Root text"), true);
    assert.equal(root.kryText("tap-button"), "Root text");
    assert.equal(root.kryText("tap-button", "Launch"), true);
    assert.equal(firstButton.kryText(), "Launch");
    assert.equal(firstButton.kryText("Go"), true);
    assert.equal(firstButton.kryText(), "Go");
    assert.equal(runtime.webDOMObject(target, "tap-button").node.text, "Go");
    assert.equal(buttonObject.text(), "Go");
    assert.equal(buttonObject.text("Object text"), true);
    assert.equal(buttonObject.text(), "Object text");
    assert.equal(runtime.webDOMObject(target, "tap-button").node.text, "Object text");
    assert.equal(runtime.webDOMQuery(target, "[sourcePath=\"src/valid.kry\"]").element, screen);
    assert.equal(runtime.webDOMQuery(target,
      `[source="src/valid.kry"][line=${webDoc.nodes[2].sourceLine}][column=${webDoc.nodes[2].sourceColumn}]`).element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, `[sourceRef="${tapSourceRef}"]`).element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, `[sourceColumnRef="${tapSourceColumnRef}"]`).element,
      firstButton);
    assert.equal(runtime.webDOMQuery(target, `[sourceRangeRef="${tapSourceRangeRef}"]`).element,
      firstButton);
    assert.equal(firstButton.dataset.krySourceRangeRef, tapSourceRangeRef);
    assert.equal(runtime.webDOMObjectAtSource(target, "src/valid.kry", webDoc.nodes[2].sourceLine,
      webDoc.nodes[2].sourceColumn).element, firstButton);
    assert.equal(runtime.webDOMObjectAtSourceRange(target, "src/valid.kry",
      webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn + 1).element, firstButton);
    assert.equal(root.kryAtSourceRange("src/valid.kry", webDoc.nodes[2].sourceLine,
      webDoc.nodes[2].sourceColumn + 1).element, firstButton);
    assert.equal(runtime.webDOMObjectOverlappingSourceRange(target, "src/valid.kry",
      webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn + 1,
      webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn - 1)
      .element, firstButton);
    assert.deepEqual(runtime.webDOMObjectsOverlappingSourceRange(target, "src/valid.kry",
      webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn + 1,
      webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn - 1)
      .map((object) => object.node.path), [webDoc.nodes[2].path, webDoc.nodes[0].path]);
    assert.equal(root.kryOverlappingSourceRange("src/valid.kry",
      webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn + 1,
      webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn - 1)
      .element, firstButton);
    assert.deepEqual(root.kryOverlappingSourceRanges("src/valid.kry",
      webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn + 1,
      webDoc.nodes[2].sourceEndLine, webDoc.nodes[2].sourceEndColumn - 1)
      .map((object) => object.node.path), [webDoc.nodes[2].path, webDoc.nodes[0].path]);
    assert.deepEqual(runtime.webDOMObjectsAtSource(target, "src/valid.kry",
      webDoc.nodes[2].sourceLine, webDoc.nodes[2].sourceColumn).map((object) => object.ref),
      [tapSourceColumnRef]);
    assert.equal(runtime.webDOMSourceMap(target)
      .some((object) => object.ref === "primary-action"), true);
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
    assert.equal(runtime.webDOMQuery(target, "TextField:readonly").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "TextField:read-only").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "TextField:required").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "Button:enabled").element, firstButton);
    assert.equal(runtime.webDOMQuery(target, "Screen:has(> Button)").element, screen);
    assert.equal(runtime.webDOMQuery(target, "Text:has(+ Button)").element, firstText);
    assert.equal(runtime.webDOMQuery(target, "Text:has(~ TextField)").element, firstText);
    assert.equal(runtime.webDOMSetState(target, "tap-button", "focus", true), true);
    assert.equal(runtime.webDOMQuery(target, "Screen:focus-within").element, screen);
    assert.equal(runtime.webDOMSetState(target, "tap-button", "focus", false), true);
    runtime.ReplaceRoute("/#tap-button");
    assert.equal(runtime.webDOMQuery(target, "Button:target").element, firstButton);
    runtime.ReplaceRoute("/");
    assert.equal(runtime.webDOMQuery(target, "Input:optional").node.path, inputPaths[0]);
    assert.equal(runtime.webDOMQuery(target, "TextField:valid").element, runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMQuery(target, "Screen:valid"), null);
    assert.equal(runtime.webDOMSetState(target, "q", "valid", true), true);
    assert.equal(runtime.webDOMGetState(target, "q", "valid"), true);
    assert.equal(runtime.webDOMSetState(target, "q", "placeholder-shown", true), true);
    assert.equal(runtime.webDOMGetState(target, "q", "placeholder_shown"), true);
    assert.equal(runtime.webDOMQuery(target, "TextField:placeholder-shown").element,
      runtime.findWebElement(target, "q"));
    assert.equal(runtime.webDOMSetState(target, "q", "autofill", true), true);
    assert.equal(runtime.webDOMQuery(target, "TextField:autofill").element,
      runtime.findWebElement(target, "q"));
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
    assert.equal(runtime.webDOMQuery(target, "[for=\"search-box\"]").ref, "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "[htmlFor=\"search-box\"]").ref, "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "[popover=manual]").ref, "Scene/root/search_label");
    assert.equal(runtime.webDOMQuery(target, "[popoverTarget=\"Scene/root/search_label\"]").ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[aria-label=\"Tap the action\"]").ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[aria-description=\"Runs the host action\"]").ref,
      "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[aria-controls=\"search-box\"]").ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[aria-owns=\"search-box\"]").ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[aria-details=\"Scene/root/search_label\"]").ref,
      "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[aria-errormessage=\"Scene/root/search_label\"]").ref,
      "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[aria-flowto=\"search-box\"]").ref, "primary-action");
    const resolvedSearchLabel = runtime.findWebElement(target, "Scene/root/search_label");
    assert.equal(resolvedSearchLabel.id, "kry-Scene-root-search_label");
    assert.equal(resolvedSearchLabel.attributes.for, "search-field");
    assert.equal(firstButton.attributes.popovertarget, resolvedSearchLabel.id);
    assert.equal(runtime.webDOMQuery(target, "[fetchpriority=high]").ref, "primary-action");
    assert.equal(runtime.webDOMQuery(target, "[part=\"primary-action\"]").ref, "primary-action");
    const domRefs = runtime.webDOMObjects(target).map((object) => object.ref);
    assert.equal(domRefs[0], "Scene/root");
    assert.match(domRefs[1], /^Scene\/root\/Text@\d+$/);
    assert.deepEqual(domRefs.slice(2), [
      "primary-action",
      "search-box",
      "Scene/root/search_label",
      selectablePath,
      ...inputPaths
    ]);
    const domObjectMap = runtime.webDOMObjectMap(target);
    assert.equal(domObjectMap.get("primary-action").element, firstButton);
    assert.equal(domObjectMap.get("Scene/root/tap").element, firstButton);
    assert.equal(domObjectMap.get("tap").element, firstButton);
    assert.equal(domObjectMap.get("tap-button").element, firstButton);
    assert.equal(domObjectMap.get(tapSourceRef).element, firstButton);
    assert.equal(domObjectMap.get(tapSourceColumnRef).element, firstButton);
    assert.equal(domObjectMap.get(selectablePath).node.kind, "Selectable");
    const firstField = screen.children[2];
    assert.equal(firstField.attributes["aria-invalid"], "false");
    assert.equal(firstField.__kryDocNode.scrollLeft, 7);
    assert.equal(firstField.__kryDocNode.scrollTop, 19);
    const fieldObject = runtime.webDOMObject(target, "search-box");
    assert.deepEqual(firstField.kryScroll(), {
      left: 7,
      top: 19,
      width: 0,
      height: 0
    });
    assert.equal(firstField.kryScroll(11, 23), true);
    assert.equal(firstField.__kryDocNode.scrollLeft, 11);
    assert.equal(firstField.__kryDocNode.scrollTop, 23);
    assert.deepEqual(fieldObject.scroll(), {
      left: 11,
      top: 23,
      width: 0,
      height: 0
    });
    assert.equal(fieldObject.scroll(13, 29), true);
    assert.equal(firstField.__kryDocNode.scrollLeft, 13);
    assert.equal(firstField.__kryDocNode.scrollTop, 29);
    assert.deepEqual(root.kryScroll("search-box"), {
      left: 13,
      top: 29,
      width: 0,
      height: 0
    });
    assert.equal(root.kryScroll("search-box", 17, 31), true);
    assert.equal(firstField.__kryDocNode.scrollLeft, 17);
    assert.equal(firstField.__kryDocNode.scrollTop, 31);
    assert.equal(runtime.webDOMScrollIntoView(target, "[data-role]", { block: "center" }), true);
    assert.deepEqual(firstField.scrolledIntoView, { block: "center" });
    assert.equal(firstField.kryScrollIntoView({ inline: "nearest" }), true);
    assert.deepEqual(firstField.scrolledIntoView, { inline: "nearest" });
    assert.equal(fieldObject.scrollIntoView({ block: "nearest" }), true);
    assert.deepEqual(firstField.scrolledIntoView, { block: "nearest" });
    assert.equal(root.kryScrollIntoView("search-box", { inline: "center" }), true);
    assert.deepEqual(firstField.scrolledIntoView, { inline: "center" });
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
    assert.equal(firstField.attributes.inert, "");
    assert.equal(firstField.inert, true);
    assert.equal(firstField.attributes.autocapitalize, "words");
    assert.equal(firstField.attributes.enterkeyhint, "search");
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
    assert.equal(firstField.attributes["aria-details"], "kry-Scene-root-search_label");
    assert.equal(firstField.attributes["aria-errormessage"], "kry-Scene-root-search_label");
    assert.equal(firstField.attributes["aria-flowto"], "tap-button");
    assert.equal(firstField.attributes["aria-labelledby"], "kry-Scene-root-search_label");
    assert.equal(firstField.attributes["aria-activedescendant"], "kry-Scene-root-search_label");
    assert.deepEqual(runtime.webDOMRelations(target, "search-box").describedBy
      .map((object) => object.ref), ["primary-action"]);
    assert.deepEqual(runtime.webDOMRelations(target, "primary-action").describes
      .map((object) => object.ref), ["search-box"]);
    assert.equal(runtime.webDOMRelations(target, "search-box").details.ref,
      "Scene/root/search_label");
    assert.equal(runtime.webDOMRelations(target, "search-box").errorMessage.ref,
      "Scene/root/search_label");
    assert.deepEqual(runtime.webDOMRelations(target, "search-box").flowTo
      .map((object) => object.ref), ["primary-action"]);
    assert.deepEqual(runtime.webDOMRelations(target, "Scene/root/search_label").detailedBy
      .map((object) => object.ref), ["primary-action", "search-box"]);
    assert.deepEqual(runtime.webDOMRelations(target, "Scene/root/search_label").errorFor
      .map((object) => object.ref), ["primary-action", "search-box"]);
    assert.deepEqual(runtime.webDOMRelations(target, "primary-action").flowFrom
      .map((object) => object.ref), ["search-box"]);
    assert.deepEqual(runtime.webDOMRelations(target, "search-box").labelledBy
      .map((object) => object.ref), ["Scene/root/search_label"]);
    assert.equal(runtime.webDOMRelations(target, "search-box").activeDescendant.ref,
      "Scene/root/search_label");
    assert.deepEqual(runtime.webDOMRelations(target, "Scene/root/search_label").activeDescendantOf
      .map((object) => object.ref), ["search-box"]);
    assert.deepEqual(runtime.webDOMSnapshot(target, "search-box").relationRefs.describedBy,
      ["primary-action"]);
    assert.equal(runtime.webDOMSnapshot(target, "search-box").relationRefs.details,
      "Scene/root/search_label");
    assert.equal(runtime.webDOMSnapshot(target, "search-box").relationRefs.errorMessage,
      "Scene/root/search_label");
    assert.deepEqual(runtime.webDOMSnapshot(target, "search-box").relationRefs.flowTo,
      ["primary-action"]);
    assert.deepEqual(runtime.webDOMSnapshot(target, "primary-action").relationRefs.describes,
      ["search-box"]);
    assert.equal(runtime.webDOMSnapshot(target, "search-box").relationRefs.activeDescendant,
      "Scene/root/search_label");
    assert.deepEqual(runtime.webDOMSnapshot(target, "Scene/root/search_label").relationRefs.activeDescendantOf,
      ["search-box"]);
    firstField.setAttribute("aria-describedby", "primary-action");
    firstField.setAttribute("aria-details", "Scene/root/search_label");
    firstField.setAttribute("aria-errormessage", "Scene/root/search_label");
    firstField.setAttribute("aria-flowto", "primary-action");
    assert.equal(runtime.webDOMSync(target, "search-box").ref, "search-box");
    assert.equal(firstField.attributes["aria-describedby"], "tap-button");
    assert.equal(firstField.attributes["aria-details"], "kry-Scene-root-search_label");
    assert.equal(firstField.attributes["aria-errormessage"], "kry-Scene-root-search_label");
    assert.equal(firstField.attributes["aria-flowto"], "tap-button");
    firstButton.setAttribute("aria-controls", "search-box");
    assert.equal(runtime.webDOMSync(target)
      .some((object) => object.ref === "primary-action"), true);
    assert.equal(firstButton.attributes["aria-controls"], "search-field");
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
    assert.equal(firstField.style.paddingLeft, "18px");
    assert.equal(firstField.style.paddingRight, "19px");
    assert.equal(firstField.style.paddingTop, "20px");
    assert.equal(firstField.style.paddingBottom, "21px");
    assert.equal(firstField.style.paddingInline, "22px");
    assert.equal(firstField.style.paddingBlock, "23px");
    assert.equal(firstField.style.paddingInlineStart, "24px");
    assert.equal(firstField.style.paddingInlineEnd, "25px");
    assert.equal(firstField.style.paddingBlockStart, "26px");
    assert.equal(firstField.style.paddingBlockEnd, "27px");
    assert.equal(firstField.style.marginLeft, "14px");
    assert.equal(firstField.style.marginRight, "15px");
    assert.equal(firstField.style.marginTop, "16px");
    assert.equal(firstField.style.marginBottom, "17px");
    assert.equal(firstField.style.marginInline, "28px");
    assert.equal(firstField.style.marginBlock, "29px");
    assert.equal(firstField.style.marginInlineStart, "30px");
    assert.equal(firstField.style.marginInlineEnd, "31px");
    assert.equal(firstField.style.marginBlockStart, "32px");
    assert.equal(firstField.style.marginBlockEnd, "33px");
    assert.equal(firstField.style.minWidth, "44px");
    assert.equal(firstField.style.maxHeight, "55px");
    assert.equal(firstField.style.inlineSize, "66px");
    assert.equal(firstField.style.blockSize, "77px");
    assert.equal(firstField.style.minInlineSize, "88px");
    assert.equal(firstField.style.maxInlineSize, "99px");
    assert.equal(firstField.style.minBlockSize, "111px");
    assert.equal(firstField.style.maxBlockSize, "122px");
    assert.equal(firstField.style.inset, "1px");
    assert.equal(firstField.style.top, "2px");
    assert.equal(firstField.style.right, "3px");
    assert.equal(firstField.style.bottom, "4px");
    assert.equal(firstField.style.left, "5px");
    assert.equal(firstField.style.insetInline, "6px");
    assert.equal(firstField.style.insetBlock, "7px");
    assert.equal(firstField.style.insetInlineStart, "8px");
    assert.equal(firstField.style.insetInlineEnd, "9px");
    assert.equal(firstField.style.insetBlockStart, "10px");
    assert.equal(firstField.style.insetBlockEnd, "11px");
    assert.equal(firstField.style["--kry-content-offset-x"], "3px");
    assert.equal(firstField.style["--kry-content-offset-y"], "7px");
    assert.equal(firstField.style["--kry-icon-size"], "14px");
    assert.equal(firstField.style.fontFamily, "ui-sans-serif");
    assert.equal(firstField.style.fontWeight, "600");
    assert.equal(firstField.style.fontStyle, "italic");
    assert.equal(firstField.style.fontVariant, "small-caps");
    assert.equal(firstField.style.fontVariantAlternates, "historical-forms");
    assert.equal(firstField.style.fontVariantCaps, "small-caps");
    assert.equal(firstField.style.fontVariantEastAsian, "ruby");
    assert.equal(firstField.style.fontVariantLigatures, "common-ligatures");
    assert.equal(firstField.style.fontVariantNumeric, "tabular-nums");
    assert.equal(firstField.style.fontVariantPosition, "sub");
    assert.equal(firstField.style.fontLanguageOverride, "\"TRK\"");
    assert.equal(firstField.style.fontPalette, "light");
    assert.equal(firstField.style.fontStretch, "condensed");
    assert.equal(firstField.style.fontKerning, "normal");
    assert.equal(firstField.style.fontOpticalSizing, "auto");
    assert.equal(firstField.style.fontFeatureSettings, "\"kern\" 1");
    assert.equal(firstField.style.fontVariationSettings, "\"wght\" 600");
    assert.equal(firstField.style.fontSizeAdjust, "0.5");
    assert.equal(firstField.style.fontSynthesis, "none");
    assert.equal(firstField.style.fontSynthesisWeight, "none");
    assert.equal(firstField.style.fontSynthesisStyle, "none");
    assert.equal(firstField.style.fontSynthesisSmallCaps, "none");
    assert.equal(firstField.style.fontSynthesisPosition, "none");
    assert.equal(firstField.style.lineHeight, "1.4");
    assert.equal(firstField.style.letterSpacing, "1px");
    assert.equal(firstField.style.textIndent, "12px");
    assert.equal(firstField.style.textAlign, "center");
    assert.equal(firstField.style.textRendering, "optimizeLegibility");
    assert.equal(firstField.style.textDecoration, "underline");
    assert.equal(firstField.style.textDecorationLine, "underline overline");
    assert.equal(firstField.style.textDecorationColor, "#667788");
    assert.equal(firstField.style.textDecorationStyle, "wavy");
    assert.equal(firstField.style.textDecorationSkip, "spaces");
    assert.equal(firstField.style.textDecorationSkipInk, "auto");
    assert.equal(firstField.style.textDecorationThickness, "2px");
    assert.equal(firstField.style.textUnderlineOffset, "3px");
    assert.equal(firstField.style.textUnderlinePosition, "under");
    assert.equal(firstField.style.textShadow, "0 1px 2px #0004");
    assert.equal(firstField.style.textEmphasis, "dot");
    assert.equal(firstField.style.textEmphasisColor, "#223344");
    assert.equal(firstField.style.textEmphasisStyle, "filled sesame");
    assert.equal(firstField.style.textEmphasisPosition, "over right");
    assert.equal(firstField.style.textTransform, "uppercase");
    assert.equal(firstField.style.textOverflow, "ellipsis");
    assert.equal(firstField.style.whiteSpace, "nowrap");
    assert.equal(firstField.style.textSizeAdjust, "none");
    assert.equal(firstField.style.textOrientation, "mixed");
    assert.equal(firstField.style.textWrap, "balance");
    assert.equal(firstField.style.textWrapMode, "wrap");
    assert.equal(firstField.style.textWrapStyle, "pretty");
    assert.equal(firstField.style.textJustify, "inter-word");
    assert.equal(firstField.style.textCombineUpright, "digits 2");
    assert.equal(firstField.style.rubyAlign, "center");
    assert.equal(firstField.style.rubyPosition, "over");
    assert.equal(firstField.style.wordBreak, "keep-all");
    assert.equal(firstField.style.overflowWrap, "anywhere");
    assert.equal(firstField.style.wordWrap, "break-word");
    assert.equal(firstField.style.lineBreak, "strict");
    assert.equal(firstField.style.hangingPunctuation, "first");
    assert.equal(firstField.style.verticalAlign, "5px");
    assert.equal(firstField.style.direction, "rtl");
    assert.equal(firstField.style.writingMode, "vertical-rl");
    assert.equal(firstField.style.tabSize, "4");
    assert.equal(firstField.style.hyphens, "auto");
    assert.equal(firstField.style.lineClamp, "2");
    assert.equal(firstField.style.webkitLineClamp, "2");
    assert.equal(firstField.style.listStyle, "square inside");
    assert.equal(firstField.style.listStyleType, "square");
    assert.equal(firstField.style.listStylePosition, "inside");
    assert.equal(firstField.style.listStyleImage, "none");
    assert.equal(firstField.style.counterReset, "section 2");
    assert.equal(firstField.style.counterIncrement, "section");
    assert.equal(firstField.style.counterSet, "item 4");
    assert.equal(firstField.style.quotes, "\"<<\" \">>\"");
    assert.equal(firstField.style.markerSide, "match-parent");
    assert.equal(firstField.style.markerStart, "open");
    assert.equal(firstField.style.markerEnd, "close");
    assert.equal(firstField.style.orphans, "3");
    assert.equal(firstField.style.widows, "4");
    assert.equal(firstField.style.boxDecorationBreak, "clone");
    assert.equal(firstField.style.webkitBoxDecorationBreak, "clone");
    assert.equal(firstField.style.borderCollapse, "collapse");
    assert.equal(firstField.style.borderSpacing, "3px");
    assert.equal(firstField.style.tableLayout, "fixed");
    assert.equal(firstField.style.captionSide, "bottom");
    assert.equal(firstField.style.emptyCells, "hide");
    assert.equal(firstField.style.display, "flex");
    assert.equal(firstField.style.position, "relative");
    assert.equal(firstField.style.zIndex, "3");
    assert.equal(firstField.style.overflowInline, "auto");
    assert.equal(firstField.style.overflowBlock, "hidden");
    assert.equal(firstField.style.overflowX, "auto");
    assert.equal(firstField.style.overflowY, "hidden");
    assert.equal(firstField.style.scrollBehavior, "smooth");
    assert.equal(firstField.style.overscrollBehavior, "contain");
    assert.equal(firstField.style.overscrollBehaviorX, "none");
    assert.equal(firstField.style.overscrollBehaviorY, "auto");
    assert.equal(firstField.style.overscrollBehaviorInline, "contain");
    assert.equal(firstField.style.overscrollBehaviorBlock, "none");
    assert.equal(firstField.style.scrollSnapType, "x mandatory");
    assert.equal(firstField.style.scrollSnapAlign, "start center");
    assert.equal(firstField.style.scrollSnapStop, "always");
    assert.equal(firstField.style.scrollbarColor, "#223344 #ddeeff");
    assert.equal(firstField.style.scrollbarWidth, "thin");
    assert.equal(firstField.style.scrollbarGutter, "stable both-edges");
    assert.equal(firstField.style.scrollMargin, "12px");
    assert.equal(firstField.style.scrollMarginTop, "13px");
    assert.equal(firstField.style.scrollMarginInlineStart, "19px");
    assert.equal(firstField.style.scrollMarginBlockEnd, "22px");
    assert.equal(firstField.style.scrollPadding, "23px");
    assert.equal(firstField.style.scrollPaddingLeft, "27px");
    assert.equal(firstField.style.scrollPaddingInline, "28px");
    assert.equal(firstField.style.scrollPaddingBlockEnd, "33px");
    assert.equal(firstField.style.touchAction, "manipulation");
    assert.equal(firstField.style.boxSizing, "border-box");
    assert.equal(firstField.style.alignItems, "center");
    assert.equal(firstField.style.justifyContent, "space-between");
    assert.equal(firstField.style.alignSelf, "stretch");
    assert.equal(firstField.style.justifySelf, "center");
    assert.equal(firstField.style.flexDirection, "column");
    assert.equal(firstField.style.flexWrap, "wrap");
    assert.equal(firstField.style.flex, "1 1 auto");
    assert.equal(firstField.style.flexGrow, "2");
    assert.equal(firstField.style.flexShrink, "0");
    assert.equal(firstField.style.flexBasis, "13px");
    assert.equal(firstField.style.gridTemplateColumns, "repeat(2, minmax(0, 1fr))");
    assert.equal(firstField.style.gridTemplateRows, "auto 1fr");
    assert.equal(firstField.style.gridTemplateAreas, "\"header header\" \"nav main\"");
    assert.equal(firstField.style.gridAutoColumns, "minmax(8px, auto)");
    assert.equal(firstField.style.gridAutoRows, "24px");
    assert.equal(firstField.style.gridAutoFlow, "row dense");
    assert.equal(firstField.style.gridColumn, "1 / span 2");
    assert.equal(firstField.style.gridColumnStart, "1");
    assert.equal(firstField.style.gridColumnEnd, "3");
    assert.equal(firstField.style.gridArea, "main");
    assert.equal(firstField.style.gridRow, "2 / span 1");
    assert.equal(firstField.style.gridRowStart, "2");
    assert.equal(firstField.style.gridRowEnd, "4");
    assert.equal(firstField.style.rowGap, "4px");
    assert.equal(firstField.style.columnGap, "6px");
    assert.equal(firstField.style.alignContent, "stretch");
    assert.equal(firstField.style.justifyItems, "center");
    assert.equal(firstField.style.placeItems, "center");
    assert.equal(firstField.style.placeContent, "stretch");
    assert.equal(firstField.style.placeSelf, "center");
    assert.equal(firstField.style.objectFit, "contain");
    assert.equal(firstField.style.objectPosition, "center top");
    assert.equal(firstField.style.objectViewBox, "inset(10% 20% 30% 40%)");
    assert.equal(firstField.style.aspectRatio, "16 / 9");
    assert.equal(firstField.style.imageRendering, "pixelated");
    assert.equal(firstField.style.imageOrientation, "from-image");
    assert.equal(firstField.style.imageResolution, "300dpi");
    assert.equal(firstField.style.backgroundImage, "linear-gradient(#102030, #203850)");
    assert.equal(firstField.style.backgroundSize, "cover");
    assert.equal(firstField.style.backgroundPosition, "center");
    assert.equal(firstField.style.backgroundPositionX, "left");
    assert.equal(firstField.style.backgroundPositionY, "top");
    assert.equal(firstField.style.backgroundRepeat, "no-repeat");
    assert.equal(firstField.style.backgroundRepeatX, "repeat");
    assert.equal(firstField.style.backgroundRepeatY, "no-repeat");
    assert.equal(firstField.style.backgroundClip, "padding-box");
    assert.equal(firstField.style.backgroundOrigin, "border-box");
    assert.equal(firstField.style.backgroundAttachment, "fixed");
    assert.equal(firstField.style.backgroundBlendMode, "multiply");
    assert.equal(firstField.style.visibility, "visible");
    assert.equal(firstField.style.transition, "opacity 120ms ease");
    assert.equal(firstField.style.transitionProperty, "opacity, transform");
    assert.equal(firstField.style.transitionDuration, "120ms");
    assert.equal(firstField.style.transitionTimingFunction, "ease-in-out");
    assert.equal(firstField.style.transitionDelay, "20ms");
    assert.equal(firstField.style.transitionBehavior, "allow-discrete");
    assert.equal(firstField.style.animation, "fade-in 200ms ease both");
    assert.equal(firstField.style.animationName, "fade-in");
    assert.equal(firstField.style.animationDuration, "200ms");
    assert.equal(firstField.style.animationTimingFunction, "ease");
    assert.equal(firstField.style.animationDelay, "10ms");
    assert.equal(firstField.style.animationIterationCount, "2");
    assert.equal(firstField.style.animationDirection, "alternate");
    assert.equal(firstField.style.animationFillMode, "both");
    assert.equal(firstField.style.animationPlayState, "running");
    assert.equal(firstField.style.animationComposition, "accumulate");
    assert.equal(firstField.style.transform,
      "translate(var(--kry-offset-x, 0px), var(--kry-offset-y, 0px)) scale(1.1)");
    assert.equal(firstField.style.transformOrigin, "center");
    assert.equal(firstField.style.offsetPath, "path(\"M 0 0 L 10 10\")");
    assert.equal(firstField.style.offsetDistance, "50%");
    assert.equal(firstField.style.offsetRotate, "auto 45deg");
    assert.equal(firstField.style.offsetAnchor, "center");
    assert.equal(firstField.style.offsetPosition, "normal");
    assert.equal(firstField.style.filter, "contrast(1.1)");
    assert.equal(firstField.style.backdropFilter, "blur(2px)");
    assert.equal(firstField.style.clipPath, "inset(0 round 4px)");
    assert.equal(firstField.style.maskImage, "linear-gradient(#000, transparent)");
    assert.equal(firstField.style.maskSize, "cover");
    assert.equal(firstField.style.maskPosition, "center");
    assert.equal(firstField.style.maskRepeat, "no-repeat");
    assert.equal(firstField.style.maskOrigin, "border-box");
    assert.equal(firstField.style.maskClip, "padding-box");
    assert.equal(firstField.style.maskComposite, "exclude");
    assert.equal(firstField.style.maskMode, "alpha");
    assert.equal(firstField.style.borderStyle, "dashed");
    assert.equal(firstField.style.borderTopStyle, "solid");
    assert.equal(firstField.style.borderRightStyle, "dotted");
    assert.equal(firstField.style.borderBottomStyle, "double");
    assert.equal(firstField.style.borderLeftStyle, "groove");
    assert.equal(firstField.style.borderInlineStyle, "ridge");
    assert.equal(firstField.style.borderBlockStyle, "inset");
    assert.equal(firstField.style.borderInlineStartStyle, "outset");
    assert.equal(firstField.style.borderInlineEndStyle, "hidden");
    assert.equal(firstField.style.borderBlockStartStyle, "none");
    assert.equal(firstField.style.borderBlockEndStyle, "solid");
    assert.equal(firstField.style.borderColor, "#101112");
    assert.equal(firstField.style.borderTopColor, "#111213");
    assert.equal(firstField.style.borderRightColor, "#121314");
    assert.equal(firstField.style.borderBottomColor, "#131415");
    assert.equal(firstField.style.borderLeftColor, "#141516");
    assert.equal(firstField.style.borderInlineColor, "#151617");
    assert.equal(firstField.style.borderBlockColor, "#161718");
    assert.equal(firstField.style.borderInlineStartColor, "#171819");
    assert.equal(firstField.style.borderInlineEndColor, "#18191a");
    assert.equal(firstField.style.borderBlockStartColor, "#191a1b");
    assert.equal(firstField.style.borderBlockEndColor, "#1a1b1c");
    assert.equal(firstField.style.borderTopWidth, "22px");
    assert.equal(firstField.style.borderRightWidth, "23px");
    assert.equal(firstField.style.borderBottomWidth, "24px");
    assert.equal(firstField.style.borderLeftWidth, "25px");
    assert.equal(firstField.style.borderInlineWidth, "30px");
    assert.equal(firstField.style.borderBlockWidth, "31px");
    assert.equal(firstField.style.borderInlineStartWidth, "32px");
    assert.equal(firstField.style.borderInlineEndWidth, "33px");
    assert.equal(firstField.style.borderBlockStartWidth, "34px");
    assert.equal(firstField.style.borderBlockEndWidth, "35px");
    assert.equal(firstField.style.borderTopLeftRadius, "26px");
    assert.equal(firstField.style.borderTopRightRadius, "27px");
    assert.equal(firstField.style.borderBottomRightRadius, "28px");
    assert.equal(firstField.style.borderBottomLeftRadius, "29px");
    assert.equal(firstField.style.borderStartStartRadius, "36px");
    assert.equal(firstField.style.borderStartEndRadius, "37px");
    assert.equal(firstField.style.borderEndStartRadius, "38px");
    assert.equal(firstField.style.borderEndEndRadius, "39px");
    assert.equal(firstField.style.cursor, "pointer");
    assert.equal(firstField.style.pointerEvents, "auto");
    assert.equal(firstField.style.outline, "2px solid #445566");
    assert.equal(firstField.style.outlineWidth, "2px");
    assert.equal(firstField.style.outlineOffset, "3px");
    assert.equal(firstField.style.outlineStyle, "solid");
    assert.equal(firstField.style.outlineColor, "#445566");
    assert.equal(firstField.style.boxShadow, "0 1px 2px #0004");
    assert.equal(firstField.style.colorScheme, "light dark");
    assert.equal(firstField.style.contain, "layout paint");
    assert.equal(firstField.style.container, "search / inline-size");
    assert.equal(firstField.style.containerType, "inline-size");
    assert.equal(firstField.style.containerName, "search");
    assert.equal(firstField.style.willChange, "transform");
    assert.equal(firstField.style.isolation, "isolate");
    assert.equal(firstField.style.mixBlendMode, "multiply");
    assert.equal(firstField.style.columns, "2 auto");
    assert.equal(firstField.style.columnCount, "2");
    assert.equal(firstField.style.columnWidth, "180px");
    assert.equal(firstField.style.columnFill, "balance");
    assert.equal(firstField.style.columnSpan, "all");
    assert.equal(firstField.style.columnRule, "1px solid #ccc");
    assert.equal(firstField.style.columnRuleColor, "#ccddee");
    assert.equal(firstField.style.columnRuleStyle, "dashed");
    assert.equal(firstField.style.columnRuleWidth, "4px");
    assert.equal(firstField.style.breakBefore, "avoid");
    assert.equal(firstField.style.breakAfter, "auto");
    assert.equal(firstField.style.breakInside, "avoid");
    assert.equal(firstField.style.float, "inline-start");
    assert.equal(firstField.style.clear, "both");
    assert.equal(firstField.style.order, "2");
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
    assert.equal(fieldObject.value(), "method");
    assert.equal(fieldObject.value("object"), true);
    assert.equal(fieldObject.value(), "object");
    assert.equal(runtime.webFormValue(target, "q"), "object");
    assert.equal(root.kryValue("search-box"), "object");
    assert.equal(root.kryValue("search-box", "root-value"), true);
    assert.equal(root.kryValue("search-box"), "root-value");
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
    assert.equal(fieldObject.dispatch("keydown", { key: "Escape" }), true);
    assert.equal(domState.count, countBeforeDispatch + 3000);
    assert.equal(root.kryDispatch("[name=q]", "keydown", { key: "Escape" }), true);
    assert.equal(domState.count, countBeforeDispatch + 4000);
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
    assert.equal(firstButton.kryClick(), true);
    assert.equal(domRt.input.events.at(-1).type, "tap");
    assert.equal(root.kryClick("tap-button"), true);
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
    const unmountRt = runtime.createRuntime({ app: generated.app });
    const unmountState = generated.createState();
    generated.frame(unmountRt, unmountState, host);
    const unmountTarget = document.createElement("div");
    const unmountEvents = [];
    unmountTarget.addEventListener("kry-unmount", (event) => unmountEvents.push([
      event.kryRef,
      event.detail?.object?.ref,
      event.detail?.element?.dataset?.kryRef
    ]));
    runtime.renderWebDocument(unmountRt, unmountTarget);
    const observedUnmount = [];
    const removeUnmountObserver = runtime.webDOMObserve(unmountTarget, "Button.primary",
      (objects, detail) => observedUnmount.push([objects.map((object) => object.ref), detail.event?.type || ""]));
    assert.deepEqual(observedUnmount, [[["primary-action"], ""]]);
    unmountRt.frame = [];
    runtime.renderWebDocument(unmountRt, unmountTarget);
    assert.deepEqual(observedUnmount, [
      [["primary-action"], ""],
      [[], "kry-render"]
    ]);
    removeUnmountObserver();
    const expectedUnmountRefs = unmountEvents.map((event) => event[0]);
    assert.ok(expectedUnmountRefs.includes("Scene/root"));
    assert.ok(expectedUnmountRefs.includes(webDoc.nodes[1].path));
    assert.ok(expectedUnmountRefs.includes("primary-action"));
    assert.ok(expectedUnmountRefs.includes("search-box"));
    assert.ok(expectedUnmountRefs.includes("Scene/root/search_label"));
    assert.deepEqual(unmountEvents.map((event) => event[1]), unmountEvents.map((event) => event[0]));
    assert.deepEqual(unmountEvents.map((event) => event[2]), unmountEvents.map((event) => event[0]));
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
assert.equal(defaultSnapshot.frame.length, 8);

const mounted = generated.main(null, host);
assert.equal(mounted.mounted, false);

const styleRuntime = runtime.createRuntime();
runtime.beginFrame(styleRuntime);
generated.Valid_StyleCopies(styleRuntime, state, host);
const styleFrame = runtime.endFrame(styleRuntime).frame;
assert.deepEqual(rectangle(styleFrame[0].args.bounds), { x: 10, y: 20, width: 50, height: 40 });
assert.deepEqual(styleFrame.map(item => item.name), ["Button", "Button", "Button"]);
assert.deepEqual(styleFrame.map(item => "style" in item.args), [false, false, false]);

for (const [actionName, action] of [
  ["DirectAction", generated.Valid_DirectAction],
  ["StoredAction", generated.Valid_StoredAction],
  ["InferredAction", generated.Valid_InferredAction],
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
    assert.equal(result.frame[0].meta.key, result.frame[0].meta.path);
    assert.equal(result.frame[0].meta.sourcePath, "src/valid.kry");
    assert.ok(result.frame[0].meta.sourceLine > 0);
    assert.ok(result.frame[0].meta.sourceColumn > 0);
    assert.deepEqual(rectangle(result.frame[0].args.bounds), { x: 20, y: 100, width: 80, height: 32 });
  }
}

{
  const actionRuntime = runtime.createRuntime({ app: generated.app });
  runtime.beginFrame(actionRuntime);
  assert.equal(generated.Valid_CompoundAction(actionRuntime, state, host, 20), 0);
  let result = runtime.endFrame(actionRuntime);
  assert.equal(result.frame.length, 1);
  assert.equal(result.frame[0].name, "Button");
  assert.match(result.frame[0].meta.path, /^CompoundAction\/Button@\d+$/);
  assert.equal(result.frame[0].meta.key, result.frame[0].meta.path);
  assert.equal(result.frame[0].meta.sourcePath, "src/valid.kry");
  assert.ok(result.frame[0].meta.sourceLine > 0);
  assert.ok(result.frame[0].meta.sourceColumn > 0);

  actionRuntime.QueueTap(30, 110);
  runtime.beginFrame(actionRuntime);
  assert.equal(generated.Valid_CompoundAction(actionRuntime, state, host, 20), 1);
  result = runtime.endFrame(actionRuntime);
  assert.equal(result.frame[0].meta.path, result.frame[0].meta.key);
}

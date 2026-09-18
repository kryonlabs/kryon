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

const kss = await import(new URL("./kss_parser.js", pathToFileURL(runtimePath)));
const properties = readFileSync(new URL("./fixtures/kss/css-properties.tsv", import.meta.url), "utf8").trimEnd().split("\n");
assert.equal(properties.length, 512);
for (const line of properties) {
  const [kind, raw, expectedRaw] = line.split("\t");
  const value = raw === "<empty>" ? "" : raw;
  const expected = expectedRaw === "<empty>" ? "" : expectedRaw;
  if (kind === "P") {
    assert.equal(kss.KssParser_KssCSSPropertyName(null, null, null, value), expected, line);
  } else if (kind === "U") {
    assert.equal(kss.KssParser_KssCSSNeedsPixels(null, null, null, value), expected === "1", line);
  } else {
    assert.equal(kind, "B");
    assert.equal(kss.KssParser_KssCSSBorderShorthand(null, null, null, value), expected === "1", line);
  }
}

// Property aliases and numeric units reach both ordinary rules and keyframes.
{
  const sheet = runtime.parseWebStyleSheet(`
    Button { foreground: #112233; radius: 0; opacity: 0; line-height: 2;
      border: 2px solid #223344; font-size: 10; --counter: 4; }
    @keyframes unit-probe {
      from { radius: 0; opacity: 0; line-height: 2; }
      to { radius: 8; opacity: 1; line-height: 3; }
    }
  `);
  const css = runtime.webStyleSheetToCSS(sheet);
  assert.match(css, /color: #112233;/);
  assert.match(css, /border-radius: 0px;/);
  assert.match(css, /opacity: 0;/);
  assert.match(css, /line-height: 2;/);
  assert.match(css, /border: 2px solid #223344;/);
  assert.match(css, /font-size: 10px;/);
  assert.match(css, /--counter: 4px;/);
  assert.match(css, /@keyframes unit-probe[\s\S]*border-radius: 8px;/);
  assert.doesNotMatch(css, /(?:opacity|line-height): [0-9]+px/);
}

const predicates = readFileSync(new URL("./fixtures/kss/selector-predicates.tsv", import.meta.url), "utf8").trimEnd().split("\n");
assert.equal(predicates.length, 53);
for (const line of predicates) {
  const [kind, first, second, third, expected] = line.split("\t");
  const actual = kind === "A"
    ? kss.KssParser_KssAttributeMatches(null, null, null, first === "<empty>" || first === "<missing>" ? "" : first,
      second === "<empty>" ? "" : second, third, first !== "<missing>")
    : kind === "G"
      ? kss.KssParser_KssSelectorGroupMatches(null, null, null, first, Number(second), Number(third))
      : kss.KssParser_KssNthMatches(null, null, null, first, Number(second));
  assert.equal(actual, expected === "1", line);
}

const factDefaults = {
  "I": {id: "", name: "", key: ""},
  "S": {
    "kind": "",
    "tag": "",
    "any_active": false,
    "requested": false,
    "hover": false,
    "pressed": false,
    "focus": false,
    "disabled": false,
    "node_disabled": false,
    "readonly": false,
    "readonly_camel": false,
    "node_readonly": false,
    "required": false,
    "node_required": false,
    "valid": false,
    "invalid": false,
    "aria_invalid": false,
    "extra_invalid": false,
    "placeholder_shown": false,
    "placeholder_shown_camel": false,
    "placeholder_present": false,
    "value_present": false
  },
  "R": {
    "has_parent": false,
    "has_scope": false,
    "is_scope": false,
    "sibling_index": 0,
    "sibling_count": 0,
    "type_index": 0,
    "type_count": 0,
    "has_children": false,
    "has_text": false,
    "focus_within": false,
    "target": false
  }
};
factDefaults.P = factDefaults.R;
const factCases = readFileSync(new URL("./fixtures/kss/selector-facts.tsv", import.meta.url), "utf8").trimEnd().split("\n");
assert.equal(factCases.length, 85);
for (const line of factCases) {
  const [kind, query, assignments, expected] = line.split("\t");
  const facts = {...factDefaults[kind]};
  if (assignments !== "-") {
    for (const assignment of assignments.split(",")) {
      const [key, value] = assignment.split("=");
      assert.ok(Object.hasOwn(facts, key), key);
      facts[key] = typeof facts[key] === "boolean" ? value === "1" : typeof facts[key] === "number" ? Number(value) : value;
    }
  }
  let actual;
  if (kind === "S") {
    actual = Number(kss.KssParser_KssStateMatches(null, null, null, query, facts));
  } else if (kind === "I") {
    actual = Number(kss.KssParser_KssIdentityMatches(null, null, null, query, facts));
  } else if (kind === "P") {
    const separator = query.indexOf("|");
    const name = separator < 0 ? query : query.slice(0, separator);
    const argument = separator < 0 ? "" : query.slice(separator + 1);
    actual = kss.KssParser_KssPseudoMatch(null, null, null, name, argument, separator >= 0, facts);
  } else {
    actual = kss.KssParser_KssStructuralMatch(null, null, null, query, facts);
  }
  assert.equal(actual, Number(expected), line);
}

// Streaming selector grammar: byte spans and atom values match C and Go.
kss.setHost({StringSlice: runtime.StringSlice});
const grammarCases = readFileSync(new URL("./fixtures/kss/selector-grammar.tsv", import.meta.url), "utf8").trimEnd().split("\n");
assert.equal(grammarCases.length, 42);
for (const line of grammarCases) {
  const [mode, source, expectedValid, expectedCount, expectedScore, ...expectedValues] = line.split("\t");
  let parser = kss.KssParser_KssBeginSelector(null, null, null, source);
  let cursor = {source, pos: 0, line: 0, column: 0, file: 0};
  let last;
  let valid = false;
  let count = 0;
  for (let steps = 0; steps <= runtime.StringByteLength(source) + 1; steps++) {
    const result = mode === "A" ? kss.KssParser_KssSelectorNext(null, null, null, parser)
      : mode === "R" ? kss.KssParser_KssRelativeSelectorPart(null, null, null, cursor)
        : kss.KssParser_KssSelectorPart(null, null, null, cursor, mode === "S");
    if (!result.ok)
      break;
    if (result.done) {
      valid = true;
      break;
    }
    if (mode === "A") {
      assert.ok(result.parser.cursor.pos > parser.cursor.pos);
      parser = result.parser;
    } else {
      assert.ok(result.parser.pos > cursor.pos);
      cursor = result.parser;
    }
    last = result;
    count++;
  }
  assert.equal(valid, expectedValid === "1", line);
  if (!valid)
    continue;
  assert.equal(count, Number(expectedCount), line);
  assert.equal(parser.specificity, Number(expectedScore), line);
  const values = mode === "A"
    ? [last.canonical.length ? String.fromCharCode(...last.canonical.bytes.slice(0, last.canonical.length)) : last.name,
      last.value, last.argument, last.operation]
    : [runtime.StringSlice(source, last.start, last.length),
      last.combinator === 32 ? "space" : last.combinator ? String.fromCharCode(last.combinator) : "", "", ""];
  assert.deepEqual(values, expectedValues.map(value => value === "-" ? "" : value), line);
}

// Exercise the actual host adapters, including reverse and same-type indices.
{
  const nodes = ["Button", "Text", "Button", "Button"].map((kind, index) => ({
    kind, path: `row/${index}`, parentPath: "row", title: "café 🌿"
  }));
  for (const node of nodes)
    node.__kryFrameNodes = nodes;
  const sheet = runtime.parseWebStyleSheet(`
    Button[title^="café"] { background: #112233; }
    Button[title$="🌿"] { foreground: #445566; }
    Button[title^=""] { background: #ffffff; }
    Button[data-absent=""] { background: #ffffff; }
    Button:nth-child(2n + 1) { border: #778899; }
    Button:nth-last-child(2) { radius: 4; }
    Button:nth-of-type(2) { padding-x: 7; }
    Button:nth-last-of-type(2) { padding-y: 8; }
    Button:nth-child(0x3) { radius: 99; }
    Button:nth-of-type { radius: 99; }
    Button:first-child(1) { radius: 99; }
    Button:unknown(2) { radius: 99; }
  `);
  assert.deepEqual(runtime.resolveWebStyle(nodes[2], sheet), {
    background: "#112233", foreground: "#445566", border: "#778899",
    radius: 4, "padding-x": 7, "padding-y": 8
  });
  assert.equal(runtime.resolveWebStyle(nodes[3], sheet).border, undefined);
}

// State and structural facts are collected from real frame nodes, then matched
// by the same generated functions used in the C/Go fixtures above.
{
  const root = {kind: "Column", path: "root", parentPath: "root"};
  const field = {kind: "TextField", tag: "input", path: "root/field", parentPath: "root",
    readOnly: true, required: true, placeholder: "Type here", state: {focus: true, custom: true}};
  const label = {kind: "Text", path: "root/label", parentPath: "root", text: "Label"};
  const nodes = [root, field, label];
  for (const node of nodes)
    node.__kryFrameNodes = nodes;
  const sheet = runtime.parseWebStyleSheet(`
    TextField[state=FOCUSED] { border: #112233; }
    TextField:read-only { radius: 5; }
    TextField:required { padding-x: 7; }
    TextField:placeholder-shown { foreground: #445566; }
    TextField:first-child { padding-y: 8; }
    TextField:only-of-type { gap: 9; }
    TextField:valid { opacity: 0.5; }
    TextField[state=custom] { font-size: 16; }
    TextField:normal { radius: 99; }
    TextField:empty { icon-size: 18; }
    Column:scope { radius: 2; }
    Column:focus-within { border-width: 3; }
    Text:last-child { radius: 4; }
    Text:empty { radius: 99; }
  `);
  assert.deepEqual(runtime.resolveWebStyle(field, sheet), {
    border: "#112233", radius: 5, "padding-x": 7, foreground: "#445566",
    "padding-y": 8, gap: 9, opacity: 0.5, "font-size": 16, "icon-size": 18
  });
  assert.deepEqual(runtime.resolveWebStyle(root, sheet), {radius: 2, "border-width": 3});
  assert.deepEqual(runtime.resolveWebStyle(label, sheet), {radius: 4});
}

// Parsed arguments keep quoted delimiters and nested selector groups intact.
{
  const parent = {kind: "Column", path: "root", parentPath: "root"};
  const button = {kind: "Button", path: "root/button", parentPath: "root",
    classes: ["accent"], title: "https://example.test/a,b] { café }"};
  parent.__kryFrameNodes = button.__kryFrameNodes = [parent, button];
  const sheet = runtime.parseWebStyleSheet(`
    Button[title="https://example.test/a,b] { café }"] { radius: 7; }
    Button/* comma, brace { and > */.accent { border: #112233; }
    Button:not(:is(.quiet, .disabled)) { foreground: #445566; }
    Button:is(Column > Button.accent, Text) { padding-x: 9; }
  `);
  assert.deepEqual(runtime.resolveWebStyle(button, sheet), {
    radius: 7, border: "#112233", foreground: "#445566", "padding-x": 9
  });
  assert.equal(sheet.rules[0].selector.attrs.title, button.title);
  assert.throws(() => runtime.parseWebStyleSheet('Button[title="x" i] {radius: 1;}'), /invalid KSS selector/);
  assert.throws(() => runtime.parseWebStyleSheet('Button:is() {radius: 1;}'), /invalid KSS selector/);
  assert.throws(() => runtime.parseWebStyleSheet('Button/* unfinished {radius: 1;}'), /unterminated selector comment/);
}

// CSS serialization and runtime matching consume the same nested pseudo grammar.
{
  const parent = {kind: "Column", path: "root", parentPath: "root"};
  const child = {kind: "Button", path: "root/button", parentPath: "root",
    classes: ["accent"], title: "text ) with ( delimiters"};
  parent.__kryFrameNodes = child.__kryFrameNodes = [parent, child];
  const source = 'Column:has(> Button:not(.quiet)[title="text ) with ( delimiters"]) { radius: 13; }';
  const sheet = runtime.parseWebStyleSheet(source);
  assert.deepEqual(runtime.resolveWebStyle(parent, sheet), {radius: 13});
  const css = runtime.webStyleSelectorToCSS(sheet.rules[0].selector);
  assert.equal(css, '[data-kry-kind="Column"]:has(> [data-kry-kind="Button"][title="text ) with ( delimiters"]:not(.kryon-node.quiet))');
  child.classes.push("quiet");
  assert.deepEqual(runtime.resolveWebStyle(parent, sheet), {});
  const base = runtime.parseWebStyleSheet('Button {radius: 1;}').rules[0].selector;
  assert.throws(() => runtime.webStyleSelectorToCSS({...base, pseudos: ["has(.accent))"]}), /invalid KSS pseudo selector/);
  assert.throws(() => runtime.webStyleSelectorToCSS({...base, pseudos: ["nth-child(2).injected"]}), /invalid KSS pseudo selector/);
  assert.equal(runtime.webStyleSelectorToCSS({...base, pseudos: ["read-only", "active"]}),
    '[data-kry-kind="Button"]:read-only:active');
  assert.equal(runtime.webStyleSelectorToCSS({...base, pseudos: ["nth_child(2n + 1)"]}),
    '[data-kry-kind="Button"]:nth-child(2n+1)');
}

// Separate functional groups must all hold; alternatives within a group may vary.
{
  const target = {kind: "Button", classes: ["a"]};
  const sheet = runtime.parseWebStyleSheet(`
    Button:is(.a, .other):is(.b, .third) { radius: 11; }
    Button:where(.a):where(.b) { padding-x: 12; }
    Button:not(.hidden, .disabled):not(.quiet) { foreground: #123456; }
  `);
  assert.deepEqual(runtime.resolveWebStyle(target, sheet), {foreground: "#123456"});
  target.classes.push("b");
  assert.deepEqual(runtime.resolveWebStyle(target, sheet), {
    radius: 11, "padding-x": 12, foreground: "#123456"
  });
  target.classes.push("quiet");
  assert.deepEqual(runtime.resolveWebStyle(target, sheet), {radius: 11, "padding-x": 12});
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[0].selector),
    '[data-kry-kind="Button"]:is(.kryon-node.a,.kryon-node.other):is(.kryon-node.b,.kryon-node.third)');
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[1].selector),
    '[data-kry-kind="Button"]:where(.kryon-node.a):where(.kryon-node.b)');
  const nested = runtime.parseWebStyleSheet('Button:not(:is(.a):is(.b)) {radius: 3;}');
  assert.deepEqual(runtime.resolveWebStyle({kind: "Button", classes: ["a"]}, nested), {radius: 3});
  assert.deepEqual(runtime.resolveWebStyle(target, nested), {});
}

// Repeated names are independent constraints, not overwrites in a map.
{
  const target = {kind: "Button", domId: "save", name: "action", key: "primary", title: "prefix-end"};
  const sheet = runtime.parseWebStyleSheet(`
    Button[title^="prefix"][title$="end"] { radius: 9; }
    Button[title="wrong"][title] { padding-x: 99; }
    Button[title="wrong"][title="prefix-end"] { padding-y: 99; }
    Button#missing#save { gap: 99; }
    Button#save#action#primary { foreground: #123456; }
    Button#save#save { border: #abcdef; }
  `);
  const expected = {radius: 9, foreground: "#123456", border: "#abcdef"};
  assert.deepEqual(runtime.resolveWebStyle(target, sheet), expected);
  assert.deepEqual(runtime.traceWebStyle(target, sheet).resolved, expected);
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[0].selector),
    '[data-kry-kind="Button"][title^="prefix"][title$="end"]');
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[1].selector),
    '[data-kry-kind="Button"][title="wrong"][title]');
  assert.deepEqual(sheet.rules[3].selector.ids, ["missing", "save"]);
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[3].selector),
    '[data-kry-kind="Button"]:is(#missing,[data-kry-name="missing"],[data-kry-key="missing"]):is(#save,[data-kry-name="save"],[data-kry-key="save"])');
  target.title = "other-end";
  assert.deepEqual(runtime.resolveWebStyle(target, sheet), {foreground: "#123456", border: "#abcdef"});
  const {ids, attributes, ...legacy} = sheet.rules[0].selector;
  assert.deepEqual(runtime.resolveWebStyle(target, {rules: [{...sheet.rules[0], selector: legacy}]}), {radius: 9});
}

// The shared driver searches complete chains, including earlier alternatives.
{
  const cases = readFileSync(new URL("./fixtures/kss/selector-chains.tsv", import.meta.url), "utf8").trimEnd().split("\n");
  assert.equal(cases.length, 27);
  for (const line of cases) {
    const [name, relations, graph, target, expected, anchor] = line.split("\t");
    const nodes = graph.split(";").map(text => text.split(",").map(Number));
    const stack = [kss.KssParser_KssSelectorChainBegin(null, null, null, relations.length, Number(target))];
    if (anchor !== undefined)
      stack[0] = kss.KssParser_KssRelativeSelectorBegin(null, null, null, relations.length, Number(target), Number(anchor));
    let actual = false;
    for (let steps = 0; stack.length && steps < 512; steps++) {
      const frame = stack.at(-1);
      const node = nodes[frame.cursor];
      const matched = !!node && frame.part >= 0 && !frame.entered && !!(node[2] & (1 << frame.part));
      const relation = relations[frame.part] === "D" ? 32 : relations[frame.part] === "0" ? 0 :
        (relations.charCodeAt(frame.part) || 0);
      const result = kss.KssParser_KssSelectorChainStep(null, null, null, frame, matched,
        relation, node?.[0] ?? -1, node?.[1] ?? -1);
      if (result.action === kss.KssSelectorAccept) {
        actual = true;
        break;
      }
      if (result.action === kss.KssSelectorPush) {
        stack[stack.length - 1] = result.frame;
        stack.push(result.next);
      } else {
        assert.equal(result.action, kss.KssSelectorPop, name);
        stack.pop();
      }
    }
    assert.ok(actual || stack.length === 0, name + " terminates");
    assert.equal(actual, expected === "1", name);
  }
  const nodes = [
    {kind: "Column", path: "root", parentPath: "root", classes: ["outer"]},
    {kind: "Column", path: "root/a", parentPath: "root", classes: ["branch"]},
    {kind: "Column", path: "root/a/b", parentPath: "root/a", classes: ["branch"]},
    {kind: "Button", path: "root/a/b/leaf", parentPath: "root/a/b", classes: ["leaf"]}
  ];
  for (const node of nodes) node.__kryFrameNodes = nodes;
  const sheet = runtime.parseWebStyleSheet('.outer > .branch .leaf {radius: 7;}');
  assert.deepEqual(runtime.resolveWebStyle(nodes[3], sheet), {radius: 7});
  const siblings = ["anchor", "branch", "other", "branch", "leaf"].map((cls, index) =>
    ({kind: "Button", path: "siblings/" + index, parentPath: "siblings", classes: [cls]}));
  for (const node of siblings) node.__kryFrameNodes = siblings;
  assert.deepEqual(runtime.resolveWebStyle(siblings[4],
    runtime.parseWebStyleSheet('.anchor + .branch ~ .leaf {radius: 8;}')), {radius: 8});
  const long = Array.from({length: 70}, (_, index) => ({kind: "Column", path: "long/" + index,
    parentPath: index ? "long/" + (index - 1) : "", classes: ["branch"]}));
  for (const node of long) node.__kryFrameNodes = long;
  assert.deepEqual(runtime.resolveWebStyle(long.at(-1),
    runtime.parseWebStyleSheet(Array(70).fill('.branch').join(' > ') + ' {radius: 9;}')), {radius: 9});
}

// Relative :has chains are anchored to their subject, including sibling chains.
{
  const nodes = [
    {kind: "Section", path: "outer", parentPath: "", classes: ["outside"]},
    {kind: "Column", path: "outer/subject", parentPath: "outer", classes: ["subject"]},
    {kind: "Column", path: "outer/subject/branch", parentPath: "outer/subject", classes: ["branch"]},
    {kind: "Button", path: "outer/subject/branch/leaf", parentPath: "outer/subject/branch",
      classes: ["leaf"], title: ":has("},
    {kind: "Column", path: "outer/peer", parentPath: "outer", classes: ["peer"]},
    {kind: "Button", path: "outer/peer/leaf", parentPath: "outer/peer", classes: ["leaf"]}
  ];
  for (const node of nodes)
    node.__kryFrameNodes = nodes;
  const sheet = runtime.parseWebStyleSheet(`
    .subject:has(> .branch .leaf) {radius: 7;}
    .subject:has(+ .peer > .leaf) {padding-x: 8;}
    .subject:has(~ .peer .leaf) {gap: 9;}
    .subject:has([title=":has("]) {foreground: #123456;}
    .subject:has(/* :has( */ > .branch .leaf) {border: #abcdef;}
    .subject:has(.missing, > .branch .leaf) {padding-y: 10;}
    .subject:has(.outside .leaf) {radius: 99;}
    .subject:has(> .leaf) {gap: 99;}
  `);
  const expected = {radius: 7, "padding-x": 8, gap: 9, foreground: "#123456",
    border: "#abcdef", "padding-y": 10};
  assert.deepEqual(runtime.resolveWebStyle(nodes[1], sheet), expected);
  assert.deepEqual(runtime.resolveWebStyle({...nodes[1]}, sheet), expected);
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[0].selector),
    '.kryon-node.subject:has(> .kryon-node.branch .kryon-node.leaf)');
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[4].selector),
    '.kryon-node.subject:has(> .kryon-node.branch .kryon-node.leaf)');
  assert.throws(() => runtime.parseWebStyleSheet('.subject:has(:is(.x, :has(.leaf))) {radius:1;}'), /invalid KSS selector/);
  assert.throws(() => runtime.parseWebStyleSheet('> .leaf {radius:1;}'), /invalid KSS selector/);
}

// Formula validation is shared with matching; CSS export must not rewrite invalid input.
{
  const lines = readFileSync(new URL("./fixtures/kss/nth-formulas.tsv", import.meta.url), "utf8").trimEnd().split("\n");
  assert.equal(lines.length, 30);
  for (const line of lines) {
    const [source, ok, step, offset, expectedText] = line.split("\t");
    const text = kss.KssParser_KssNthText(null, null, null, source);
    assert.equal(String.fromCharCode(...text.bytes.slice(0, text.length)), expectedText === "-" ? "" : expectedText, line);
    const formula = kss.KssParser_KssParseNth(null, null, null, source);
    assert.deepEqual([formula.ok, Number(formula.step), Number(formula.offset)],
      [ok === "1", Number(step), Number(offset)], line);
  }
  for (const name of ["nth-child", "nth-last-child", "nth-of-type", "nth-last-of-type"]) {
    for (const argument of ["1.5", "0x3", "1e2", "n+1.5", "n of .item", "2147483648"]) {
      const sheet = runtime.parseWebStyleSheet(`Button:${name}(${argument}) {radius: 99;}`);
      assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[0].selector),
        '[data-kry-kind="Button"]:not(*)', name + argument);
    }
    for (const [argument, canonical] of [["odd", "2n+1"], ["ODD", "2n+1"], ["2n + 1", "2n+1"],
      ["-n+3", "-n+3"], ["0n+4", "4"], ["-3", "-3"], ["0", "0"], ["odd// tail", "2n+1"]]) {
      const sheet = runtime.parseWebStyleSheet(`Button:${name}(${argument}) {radius: 7;}`);
      assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[0].selector),
        `[data-kry-kind="Button"]:${name}(${canonical})`);
    }
  }
  const sheet = runtime.parseWebStyleSheet('Button:not(:nth-child(1.5)) {radius: 7;}');
  assert.deepEqual(runtime.resolveWebStyle({kind: "Button"}, sheet), {radius: 7});
  assert.equal(runtime.webStyleSelectorToCSS(sheet.rules[0].selector),
    '[data-kry-kind="Button"]:not(.kryon-node:not(*))');
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

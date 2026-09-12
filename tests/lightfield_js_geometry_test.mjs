// Geometry values in the actual example, not a browser visual-parity test.
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

// Hosts accept positional geometry and records emitted from shared contracts.
const rectangle = value => Array.isArray(value) ? value : [value.x, value.y, value.width, value.height];
const vector = value => Array.isArray(value) ? value : [value.x, value.y];

const moduleURL = pathToFileURL(process.argv[2]);
const module = await import(moduleURL.href);
const host = await import(new URL("../kryon-runtime.js", moduleURL).href);
const runtime = host.createRuntime({ app: { ...module.app, width: 768, height: 1024 } });

const splitRuntime = host.createRuntime({ app: module.app });
host.SetThemeMode(host.THEME_MODE_SYSTEM);
const split = module.frame(splitRuntime).frame;
assert.equal(host.GetThemeMode(), host.THEME_MODE_SYSTEM, "split view must preserve follow-system mode");
const splitButtons = split.filter(item => item.name === "Button");
assert.equal(splitButtons.length, 181);
assert.equal(new Set(splitButtons.map(item => item.args.id)).size, 181);
assert.deepEqual(rectangle(splitButtons.find(item => item.args.id === 1000).args.bounds), [81, 161, 72, 34]);
assert.deepEqual(rectangle(splitButtons.find(item => item.args.id === 11000).args.bounds), [849, 161, 72, 34]);
assert.deepEqual(rectangle(splitButtons.find(item => item.args.id === 5000).args.bounds), [24, 901, 720, 34]);
assert.deepEqual(rectangle(splitButtons.find(item => item.args.id === 15000).args.bounds), [792, 901, 720, 34]);
const splitMenus = splitButtons.filter(item => item.args.split || item.args.menu);
assert.equal(splitMenus.length, 8);
const splitOpen = splitMenus.map(item => item.args.open);
splitOpen[0].value = 1;
assert.deepEqual(splitOpen.map(ref => ref.value), [1, 0, 0, 0, 0, 0, 0, 0]);
splitOpen[0].value = 0;

function checkGeometry(dark) {
  const frame = module.frame(runtime);
  const text = frame.frame.filter(item => item.name === "Text");
  for (const [label, y] of [["Small", 518], ["Medium", dark ? 561 : 562], ["Large", 617]]) {
    const caption = text.find(item => item.args.text === label);
    assert.ok(caption, `missing size caption ${label}`);
    assert.deepEqual(rectangle(caption.args.bounds), [24, y, 56, 24], `size caption ${label}`);
    assert.equal(caption.args.font, 17);
  }
  const buttons = frame.frame.filter(item => item.name === "Button");
  assert.equal(buttons.length, 93);
  const byID = new Map(buttons.map(item => [item.args.id, item.args]));
  assert.equal(byID.size, buttons.length);
  const compound = buttons.filter(item => item.args.split || item.args.menu);
  assert.equal(compound.length, 4);
  for (const item of compound) {
    const props = item.args;
    assert.ok(props && typeof props === "object", `${item.name} has unevaluated button props`);
    assert.equal(byID.get(props.id), props, `compound button ID ${props.id} is not canonical Button props`);
    assert.equal(item.args.item_count, 2);
    assert.equal(item.args.items.length, 2);
    assert.ok(item.args.open && "value" in item.args.open, `${item.name} lost its open-state reference`);
  }
  const openStates = compound.map(item => item.args.open);
  assert.deepEqual(openStates.map(ref => ref.value), [0, 0, 0, 0]);
  openStates[0].value = 1;
  assert.deepEqual(openStates.map(ref => ref.value), [1, 0, 0, 0]);
  openStates[0].value = 0;
  for (const [id, darkBounds, lightBounds] of [
    [3006, [540, 748, 98, 40], [538, 747, 98, 40]],
    [3026, [540, 801, 98, 40], [538, 801, 98, 40]],
    [3008, [657, 748, 88, 40], [656, 747, 88, 40]],
    [3028, [657, 801, 88, 40], [656, 801, 88, 40]],
  ]) {
    assert.deepEqual(rectangle(byID.get(id).bounds), dark ? darkBounds : lightBounds, `compound control ${id}`);
  }
  for (const button of buttons) {
    const bounds = rectangle(button.args.bounds);
    assert.equal(bounds.length, 4);
    assert.ok(bounds.every(Number.isFinite), `button ${button.args.id} has non-finite bounds`);
    assert.ok(bounds[2] > 0 && bounds[3] > 0);
  }
  assert.deepEqual(rectangle(byID.get(1000).bounds), [81, 161, 72, 34]);
  assert.deepEqual(rectangle(byID.get(5000).bounds), [24, 901, 720, 34]);
  assert.deepEqual(rectangle(byID.get(5001).bounds), [24, 945, 720, 38]);
  for (const [id, darkBounds, lightBounds] of [
    [1002, [250, 161, 72, 34], [251, 161, 72, 34]],
    [1010, [81, 206, 72, 38], [81, 206, 72, 39]],
    [1012, [250, 206, 72, 38], [251, 205, 72, 38]],
    [1013, [334, 206, 72, 36], [336, 206, 72, 36]],
    [1017, [672, 206, 70, 36], [674, 206, 72, 36]],
  ]) {
    assert.deepEqual(rectangle(byID.get(id).bounds), dark ? darkBounds : lightBounds, `state sample ${id}`);
  }
  for (const [id, darkBounds, lightBounds] of [
    [1030, [80, 305, 75, 38], [80, 305, 74, 38]],
    [1031, [166, 305, 72, 38], [166, 305, 72, 38]],
    [1032, [250, 305, 73, 38], [252, 305, 72, 38]],
    [1036, [587, 305, 73, 39], [590, 305, 74, 38]],
  ]) {
    assert.deepEqual(rectangle(byID.get(id).bounds), dark ? darkBounds : lightBounds, `focus sample ${id}`);
  }
  const smallBounds = dark ? [
    [91,511,56,24], [175,511,57,25], [260,511,57,24], [343,511,56,25],
    [428,511,56,24], [512,511,56,24], [594,511,56,24], [677,511,56,25],
  ] : [
    [94,511,56,24], [176,511,56,24], [262,511,57,25], [345,511,56,25],
    [430,511,56,25], [514,511,56,25], [598,511,56,25], [681,511,56,25],
  ];
  smallBounds.forEach((bounds, column) => {
    assert.deepEqual(rectangle(byID.get(2000 + column).bounds), bounds, `small sample ${column}`);
  });
  const mediumBounds = dark ? [
    [83, 548, 71, 38], [168, 548, 71, 39], [253, 548, 70, 38], [335, 548, 71, 39],
    [420, 548, 70, 39], [504, 548, 70, 38], [587, 548, 70, 38], [671, 548, 70, 39],
  ] : [
    [87, 548, 70, 41], [168, 549, 70, 38], [254, 550, 71, 38], [338, 549, 70, 38],
    [423, 549, 70, 39], [508, 549, 70, 39], [592, 549, 71, 39], [676, 549, 70, 38],
  ];
  mediumBounds.forEach((bounds, column) => {
    assert.deepEqual(rectangle(byID.get(2010 + column).bounds), bounds, `medium sample ${column}`);
  });
  const largeBounds = dark ? [
    [81, 599, 75, 49], [166, 599, 75, 49], [250, 599, 75, 49], [333, 599, 75, 49],
    [418, 599, 74, 49], [502, 599, 74, 49], [585, 599, 74, 49], [669, 599, 75, 49],
  ] : [
    [86, 600, 72, 51], [166, 601, 74, 48], [252, 599, 74, 51], [336, 601, 74, 49],
    [421, 601, 74, 49], [506, 601, 74, 48], [590, 601, 74, 48], [674, 601, 74, 49],
  ];
  largeBounds.forEach((bounds, column) => {
    assert.deepEqual(rectangle(byID.get(2020 + column).bounds), bounds, `large sample ${column}`);
    assert.deepEqual(vector(byID.get(2020 + column).style.normal.content_offset),
      dark ? [0, 1] : [-1, 0], `large sample label ${column}`);
  });
  assert.equal(byID.get(1000).style.normal.font_size, dark ? 19 : 17);
  for (let column = 0; column < 8; column++) {
    const hover = byID.get(1010 + column).style.hover;
    const raised = dark && column !== 3 && column !== 7;
    assert.equal(hover.fields ?? 0, raised ? 4096 : 0, `hover label style ${column}`);
    if (raised) {
      assert.deepEqual(vector(hover.content_offset), [0, -1]);
    }
  }
  assert.equal(byID.get(2000).style.normal.font_size, 15);
  assert.equal(byID.get(2020).style.normal.font_size, dark ? 18 : 19);
  for (const id of [1000, 2000, 2020]) {
    assert.equal(byID.get(id).style.normal.fields, 5120);
  }
  assert.equal(byID.get(2000).size, 1);
  assert.equal(byID.get(2010).size, 0);
  assert.equal(byID.get(2020).size, 2);
  assert.equal(byID.get(3000).emphasis, dark ? host.ButtonEmphasisFilled : host.ButtonEmphasisSoft);
  for (const [id, darkBounds, lightBounds] of [
    [3003, [296.5, 746.5, 43, 43], [297, 748, 40, 40]],
    [3022, [200, 800, 82, 43], [200, 802, 81, 39]],
    [3024, [360, 800, 100, 43], [360, 801, 99, 41]],
    [3023, [297, 800, 43, 43], [296, 802, 40, 40]],
    [3005, [479, 747, 42, 42], [479, 748, 40, 40]],
    [3025, [479, 800, 43, 43], [479, 802, 40, 40]],
  ]) {
    assert.deepEqual(rectangle(byID.get(id).bounds), dark ? darkBounds : lightBounds, `icon control ${id}`);
  }
  for (const id of [1003, 1013, 1023, 1033, 1043, 1053, 2003, 2013, 2023]) {
    assert.equal(byID.get(id).tone, dark ? host.ButtonToneNeutral : host.ButtonToneAccent);
  }
  assert.equal(byID.get(3023).style.normal.icon_size, 20);
  assert.equal(byID.get(3023).style.normal.fields, host.StyleIconSize);
}

host.SetThemeMode(host.THEME_MODE_LIGHT);
checkGeometry(false);
runtime.QueueTap(700, 38);
module.frame(runtime);
assert.equal(host.GetThemeMode(), host.THEME_MODE_DARK);
checkGeometry(true);
runtime.QueueTap(620, 38);
module.frame(runtime);
assert.equal(host.GetThemeMode(), host.THEME_MODE_LIGHT);
checkGeometry(false);
console.log("Lightfield JavaScript light/dark controls and geometry pass");

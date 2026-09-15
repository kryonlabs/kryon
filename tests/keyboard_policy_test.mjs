import assert from "node:assert/strict";
import * as menu from "./menu.js";
import * as collapsible from "./collapsible.js";
import * as text from "./text_input.js";

// Exercise generated decisions, including captured focus and simultaneous keys.
for (const focused of [false, true]) {
  for (const captured of [false, true]) {
    for (const escape of [false, true]) {
      assert.equal(menu.Menu_MenuEscapeShouldClose(null, undefined, undefined,
        focused, captured, escape), focused && !captured && escape);
    }
  }
}
for (const opened of [false, true]) {
  for (const inside of [false, true]) {
    for (const released of [false, true]) {
      assert.equal(menu.Menu_MenuContextShouldSuppressClose(null, undefined, undefined,
        opened, inside, released), opened || (inside && released));
    }
  }
}
for (let mask = 0; mask < 16; mask++) {
  const down = Boolean(mask & 1);
  const up = Boolean(mask & 2);
  const right = Boolean(mask & 4);
  const left = Boolean(mask & 8);
  const expected = down ? collapsible.Collapsible_CollapsibleKeyDown()
    : up ? collapsible.Collapsible_CollapsibleKeyUp()
    : right ? collapsible.Collapsible_CollapsibleKeyRight()
    : left ? collapsible.Collapsible_CollapsibleKeyLeft()
    : collapsible.Collapsible_CollapsibleKeyNone();
  assert.equal(collapsible.Collapsible_CollapsibleKeyFor(null, undefined, undefined,
    down, up, right, left), expected);
  for (const modifier of [false, true]) {
    const shortcuts = text.TextInput_TextShortcutInputFor(null, undefined, undefined,
      modifier, down, up, right, left);
    assert.equal(shortcuts.select_all, modifier && down);
    assert.equal(shortcuts.copy, modifier && up);
    assert.equal(shortcuts.cut, modifier && right);
    assert.equal(shortcuts.paste, modifier && left);
  }
}
console.log("generated JavaScript keyboard policy passed");

for (const [previous, current, boundary] of [
  ["a", ".", true], [".", "b", true], [".", "!", false],
  ["a", " ", false], ["　", "界", true], ["界", "。", true],
  ["界", "β", false],
]) {
  assert.equal(text.TextInput_TextWordBoundaryFor(null, undefined, undefined,
    previous.codePointAt(0), current.codePointAt(0)), boundary);
}

#!/bin/sh
set -eu
compiler=${1:-build/linux-x86_64/bin/k2js}
work=$(mktemp -d /tmp/kryon-text-capacity.XXXXXX)
trap 'rm -rf "$work"' EXIT INT TERM
"$compiler" --root . -o "$work" tests/parity/text_capacity.kry
cp web/*.js "$work/"
printf '%s\n' '{"type":"module"}' > "$work/package.json"
cat > "$work/check.mjs" <<'JS'
import assert from "node:assert/strict";
import * as fixture from "./tests/parity/text_capacity.js";
import {createRuntime} from "./kryon-runtime.js";
const rt=createRuntime(), state=fixture.createState();
rt.SetFocus(35101);
rt.SetSelection(35101,0,1);
rt.QueueText("😀");
fixture.frame(rt,state);
assert.equal(state.byte,206);
assert.equal(state.value,"😀β");
assert.equal(state.cursor,4);
rt.QueueText("日");
fixture.frame(rt,state);
assert.equal(state.value,"😀β");
assert.equal(state.cursor,4);
console.log("compiled web buffer capacity passed");
JS
node "$work/check.mjs"

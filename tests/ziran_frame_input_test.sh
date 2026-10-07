#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
build=$repo/build/frame-input-replay
mkdir -p "$build"
exec 8>"$build/lock"
flock 8
"$ziran" ir --root "$repo/tests/fixtures" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$build/ir" "$repo/tests/fixtures/frame_input_replay.zi"
"$ziran" bundle --root "$repo/tests/fixtures" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --entry frame_input_replay:Frame \
    -o "$build/source.zib" "$repo/tests/fixtures/frame_input_replay.zi"
"$ziran" bundle --root "$build/ir" --module-path "$build/ir" \
    --entry frame_input_replay:Frame -o "$build/saved.zib" "$build/ir/frame_input_replay.zir"
cmp "$build/source.zib" "$build/saved.zib"
for source in source saved; do
    "$repo/tools/frame-replay.sh" "$build/$source.zib" frame_input_replay \
        "$repo/tests/fixtures/frame_input_replay.trace" "$build/$source.json"
done
cmp "$build/source.json" "$build/saved.json"
python3 "$repo/tests/frame_input_assert.py" "$build"
"$repo/tools/frame-inspect.sh" "$build/source.json" 4 1 >"$build/inspect.txt"
"$repo/tools/frame-inspect.sh" --diff "$build/source.json" "$build/saved.json"
if "$repo/tools/frame-inspect.sh" --diff "$build/source.json" "$build/changed.json" >"$build/diff.txt"; then
    echo 'Inspector accepted a changed frame' >&2; exit 1
fi
rg -F '$.frames[3].capture.nodes[1].label' "$build/diff.txt"
if "$repo/tools/frame-replay.sh" "$build/source.zib" frame_input_replay \
    "$build/failure.trace" "$build/failure.json"; then
    echo 'Replay accepted a rejected frame' >&2; exit 1
fi
python3 - "$build/failure.json" <<'PY'
import json, sys
frame = json.load(open(sys.argv[1]))['frames'][0]
assert frame['effects'] == 0
assert frame['failure']['key'] == 99 and frame['failure']['kind'] == 8
assert frame['failure']['status'] == 5
PY
if "$repo/tools/frame-replay.sh" "$build/source.zib" frame_input_replay \
    "$build/invalid.trace" "$build/invalid.json"; then
    echo 'Replay accepted invalid UTF-8' >&2; exit 1
fi
echo 'Rich frame replay, rejection diagnostics and first-difference inspection: PASS'

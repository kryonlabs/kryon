#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT HUP INT TERM

"$ziran" ir --root "$repo/tests/fixtures" \
    --module-path "$repo/src/ui" -o "$output/ir" \
    "$repo/tests/fixtures/frame_replay.zi"
"$ziran" bundle --root "$repo/tests/fixtures" \
    --module-path "$repo/src/ui" --entry frame_replay:Frame \
    -o "$output/source.zib" "$repo/tests/fixtures/frame_replay.zi"
"$ziran" bundle --root "$output/ir" --module-path "$output/ir" \
    --entry frame_replay:Frame -o "$output/saved.zib" \
    "$output/ir/frame_replay.zir"
cmp "$output/source.zib" "$output/saved.zib"

"$repo/tools/frame-replay.sh" "$output/source.zib" frame_replay \
    "$repo/tests/fixtures/frame_replay.trace" "$output/source.json"
"$repo/tools/frame-replay.sh" "$output/saved.zib" frame_replay \
    "$repo/tests/fixtures/frame_replay.trace" "$output/saved.json"
cmp "$output/source.json" "$output/saved.json"

python3 - "$output/source.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as file:
    frames = json.load(file)["frames"]
assert len(frames) == 4
assert [frame["result"] for frame in frames] == [0, 0, 1, 1]
assert [frame["input"]["pressed"] for frame in frames] == [False, True, False, False]
assert [frame["input"]["released"] for frame in frames] == [False, False, True, False]
assert all(len(frame["capture"]["nodes"]) == 2 for frame in frames)
assert all(len(frame["capture"]["paint"]) == 3 for frame in frames)
assert all(frame["capture"]["paint"][-1]["node"] == -1 for frame in frames)
assert all(frame["effects"] > 0 for frame in frames)
button_nodes = [frame["capture"]["nodes"][1] for frame in frames]
assert [node["label"] for node in button_nodes] == [
    "Hello, Ziran", "Hello, Ziran", "Hello, Ziran", "Clicked!"
]
assert len({node["identity_generation"] for node in button_nodes}) == 1
assert any(command["value"] == "Clicked!" for command in frames[3]["capture"]["paint"])
PY

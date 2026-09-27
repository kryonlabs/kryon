#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "style_picker"
#import "style_picker_props"

#program_export
Answer :: () -> s32 {
    packs: [3]StylePackOption
    packs[0].id = "dawn"
    packs[1].id = "dusk"
    packs[2].id = "night"
    packs[2].active = true
    if StylePickerActiveIndex(packs[0:3], 3) != 2 ||
        StylePickerSelectedId(packs[0:3], 2) != "night" ||
        StylePickerSelectedId(packs[0:3], 3) != "" { return -3 }
    if StylePickerOptionCountFor(-1, 32) != 0 ||
        StylePickerOptionCountFor(40, 32) != 32 ||
        StylePickerSelectedIndexFor(-1, 3) != 0 ||
        StylePickerSelectedIndexFor(7, 3) != 2 ||
        StylePickerSelectedIndexFor(0, 0) != -1 { return -1 }
    state: StylePickerState = StylePickerStateFor(40, 2, 32)
    if state.option_count != 32 || state.selected_index != 2 ||
        !state.has_options { return -2 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

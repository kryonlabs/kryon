#!/bin/sh
# Every deliberately broken implementation must be rejected by the laws, so
# the laws are known not to be vacuous. The unmutated tree must pass.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
std=${ZIRAN_STD:-"$repo/../ziran/std"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

fresh() {
    rm -rf "$work/ui"
    cp -r "$repo/src/ui" "$work/ui"
}

check() {
    "$ziran" check --root "$work/ui" --module-path "$std" "$work/ui/$1" \
        > "$work/out.json" 2> "$work/out.err"
}

fresh
for laws in "$repo"/src/ui/*_laws.zi; do
    check "$(basename "$laws")"
done

# file, law module, sed expression, expected disproved law
mutate() {
    file=$1 module=$2 expression=$3 law=$4
    fresh
    sed -i "$expression" "$work/ui/$file"
    if cmp -s "$repo/src/ui/$file" "$work/ui/$file"; then
        echo "mutation did not change $file: $expression" >&2
        exit 1
    fi
    if check "$module"; then
        echo "laws accepted mutation: $file: $expression" >&2
        exit 1
    fi
    if ! grep -q "law $law is disproved" "$work/out.err"; then
        echo "mutation $expression did not disprove $law" >&2
        cat "$work/out.err" >&2
        exit 1
    fi
}

# Scrolling past the end, or before the start.
mutate scroll.zi scroll_laws.zi 's/    if value > maximum { return maximum }/    if value > maximum + 1 { return maximum }/' ClampStaysInRange
mutate scroll.zi scroll_laws.zi 's/    if value < 0 { return 0 }/    if value < -1 { return 0 }/' ClampStaysInRange
# Negative overflow when content is shorter than its viewport.
mutate scroll.zi scroll_laws.zi 's/    if maximum < 0 { maximum = 0 }/    if maximum < -5 { maximum = 0 }/' MaxNeverNegative
# Content moving the wrong way under the finger.
mutate scroll.zi scroll_laws.zi 's/return ScrollClamp(start_scroll - delta_y, max_scroll)/return ScrollClamp(start_scroll + delta_y, max_scroll)/' DragFollowsFinger
# A corner that curves in past its own radius.
mutate paint_queue.zi paint_shape_laws.zi 's/    if dy >= radius { return radius }/    if dy >= radius { return radius + 1.0 }/' CornerInsetInRange
# A square root that stops iterating too early for large values.
mutate paint_queue.zi paint_shape_laws.zi 's/    while step < 12 {/    while step < 2 {/' SquareRootAccurate
# Truncating negative pixels toward zero instead of rounding.
mutate dpi.zi paint_shape_laws.zi 's/    if scaled < 0.0 { return -cast(s32)(0.5 - scaled) }//' ScaleIdentity
echo "Kryon laws reject all mutations"

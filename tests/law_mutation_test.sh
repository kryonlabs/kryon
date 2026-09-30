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
# A pressed checkbox that does not toggle.
mutate checkbox.zi checkbox_laws.zi 's/        result.checked = !checked/        result.checked = checked/' PressToggles
# Space activating a control while text is typed into it.
mutate focus.zi focus_laws.zi 's/    (enter_pressed || (!text_input_active && space_pressed))/    (enter_pressed || space_pressed)/' TypingBlocksSpace
# Shift+Tab moving focus forward.
mutate focus.zi focus_laws.zi 's/    if shift_down { return -1 }/    if shift_down { return 1 }/' ShiftTabMovesBackward
# A keyboard drag running under a popup that holds the keyboard.
mutate drag.zi drag_laws.zi 's/    return focus_active && keyboard_enabled && !popup_captures/    return focus_active \&\& keyboard_enabled/' PopupBlocks
# A dropdown still marked as just opened after the pointer is released.
mutate dropdown.zi dropdown_laws.zi 's/    return just_opened && pointer_pressed/    return just_opened/' ReleaseClearsMark
# A disabled radio button reporting its id.
mutate radio.zi radio_laws.zi 's/    if activated && !disabled {/    if activated {/' DisabledReportsNothing
# A drop copying the larger of the data and the output.
mutate drag_drop.zi drag_drop_laws.zi 's/    if data_size < output_size { return data_size }/    if data_size > output_size { return data_size }/' CopyTakesSmaller
# An input taking a release it did not act on.
mutate input.zi input_laws.zi 's/    interaction.consume_release = interaction.activated/    interaction.consume_release = interaction.active/' ConsumesOnlyWhenActivated
# An inspector move or resize ending without a release.
mutate inspect.zi inspect_laws.zi 's/    decision.end_edit = released && (dragging || resizing)/    decision.end_edit = dragging || resizing/' NoReleaseNoEnd
# A disabled link activating.
mutate link.zi link_laws.zi 's/    return !disabled && (clicked || focus_activate)/    return clicked || focus_activate/' DisabledNeverActivates
# A disabled list box row toggling.
mutate list_box_multi.zi list_box_multi_laws.zi 's/    if hot && !disabled && released {/    if hot \&\& released {/' RowToggles
# An accelerator firing without its required Alt.
mutate menu.zi menu_laws.zi 's/    if alt_required && !alt_down { return false }//' AcceleratorRule
# Arrow keys skipping an item when they wrap.
mutate menu.zi menu_laws.zi 's/    return (start + direction + item_count) % item_count/    return (start + direction + item_count + 1) % item_count/' DownFromLastWraps
# A modal dismissed by a release someone else already took.
mutate modal.zi modal_laws.zi 's/    if released && !release_consumed && !pointer_inside {/    if released \&\& !pointer_inside {/' OtherwiseStays
# An overlay that forbids dismissing closing anyway.
mutate overlay.zi overlay_laws.zi 's/ && !dismiss_disabled//' OtherwiseStays
# An ownerless popup under another popup keeping the input.
mutate popup_ownership.zi popup_ownership_laws.zi 's/    return has_top && (!has_owner || !same_top)/    return has_top \&\& !same_top/' OwnerlessTopCaptures
echo "Kryon laws reject all mutations"

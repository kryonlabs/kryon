#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_page_test.zi

widget_host=ziran_text_page_host
widget_mesh_host=ziran_mesh_noop_host
set --
. "$repo/tests/widget_raster_bindings.sh"

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"

# Browser-only branches also keep a font selection alive across lazy loading.
"$ziran" ir --define PLATFORM_WEB --root "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$widget_std" \
    -o "$work/canvas-ir" "$repo/src/backend/canvas_state.zi"
"$ziran" check --root "$work/canvas-ir" --module-path "$work/canvas-ir" \
    "$work/canvas-ir/canvas_state.zir"

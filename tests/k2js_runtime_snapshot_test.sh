#!/bin/sh
# k2js runtime snapshot test - verifies generated ESM records expected frames.
set -eu

root=$(cd "${1:-.}" && pwd)
build_arg=${2:-build/linux-x86_64}
case "$build_arg" in
    /*) build=$build_arg ;;
    *) build=$root/$build_arg ;;
esac

k2js=${3:-$build/bin/k2js}
work=${TMPDIR:-/tmp}/kryon-k2js-runtime-snapshot.$$

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

if [ ! -f "$k2js" ]; then
    echo "k2js not found: $k2js" >&2
    exit 1
fi
if ! command -v node >/dev/null 2>&1; then
    echo "k2js runtime snapshot skipped: node not found"
    exit 0
fi

mkdir -p "$work/out"

sources="
examples/01_file_dialog.kry
examples/02_buttons.kry
examples/03_theme.kry
examples/04_modal.kry
examples/05_color.kry
examples/06_scaling.kry
examples/07_layout.kry
examples/09_geometry.kry
examples/10_menus.kry
examples/11_basic_controls.kry
examples/12_collections.kry
examples/13_text_editor.kry
examples/14_canvas.kry
examples/15_containers.kry
examples/16_dialogs.kry
examples/17_keyboard_platform.kry
examples/18_accessibility.kry
examples/19_pictures.kry
examples/20_scene.kry
examples/20_scroll.kry
examples/21_signals.kry
examples/22_physics.kry
examples/23_animation.kry
examples/24_tilemap.kry
tests/parity/generated_form.kry
tests/parity/fields.kry
tests/parity/focus.kry
tests/parity/buttons_layout.kry
tests/parity/long_text.kry
tests/parity/basic_controls.kry
tests/parity/list_box.kry
tests/parity/tree_view.kry
tests/parity/progress.kry
tests/parity/plots.kry
tests/parity/menus.kry
tests/parity/selection_images.kry
tests/parity/table_view.kry
tests/parity/widget_catalog.kry
"

source_args=
for source in $sources; do
    source_args="$source_args $root/$source"
done

# shellcheck disable=SC2086
"$k2js" --root "$root" -o "$work/out" $source_args
cp "$root/web/kryon-runtime.js" "$work/out/kryon-runtime.js"
printf '%s\n' '{"type":"module"}' > "$work/out/package.json"

cat > "$work/runner.mjs" <<'EOF'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const outDir = process.argv[2];

const expected = new Map([
  ["examples/01_file_dialog.js", ["Screen", "Background", "Text", "Text", "Button", "Button", "Button", "Text", "Text", "Text"]],
  ["examples/02_buttons.js", ["Screen", "Background", "Text", "Text", "Button", "Button", "Button", "Text", "Text", "Text", "Text"]],
  ["examples/03_theme.js", ["Screen", "Background", "Text", "Text", "Text"]],
  ["examples/04_modal.js", ["Screen", "Background", "Text", "Text", "Button", "Text", "Text", "Button"]],
  ["examples/05_color.js", ["Screen", "Background", "Text", "Rect", "Rect", "Text", "Rect", "Rect", "Text", "Rect", "Rect", "Text", "Text", "Text"]],
  ["examples/06_scaling.js", ["Screen", "Background", "Text", "Rect", "Text", "Text", "Text"]],
  ["examples/07_layout.js", ["Screen", "Background", "Text", "Rect", "Line", "Line", "Rect"]],
  ["examples/09_geometry.js", ["Screen", "Background", "Text", "Separator", "Text", "Text", "Text", "Text", "Text", "Text", "Text", "Separator", "Text"]],
  ["examples/10_menus.js", ["Screen", "Background", "Text"]],
  ["examples/11_basic_controls.js", ["Screen", "Background", "Text", "Text", "Spinbox", "Combobox", "Progress"]],
  ["examples/12_collections.js", ["Screen", "Background", "ListBox", "TreeView", "TableView", "Text"]],
  ["examples/13_text_editor.js", ["Screen", "Background", "TextArea", "Text", "Button", "Button"]],
  ["examples/14_canvas.js", ["Screen", "Background", "CanvasGrid"]],
  ["examples/15_containers.js", ["Screen", "Background", "TabBar", "PanedView", "Collapsible", "Text"]],
  ["examples/16_dialogs.js", ["Screen", "Background", "Button", "Button", "Button", "ColorPicker"]],
  ["examples/17_keyboard_platform.js", ["Screen", "Background", "Text", "Text", "Text"]],
  ["examples/18_accessibility.js", ["Screen", "Background", "Text", "Checkbox", "Button"]],
  ["examples/19_pictures.js", ["Screen", "Background", "Text", "Stack", "Rect", "Picture", "Picture", "Text"]],
  ["examples/20_scene.js", []],
  ["examples/20_scroll.js", ["Screen", "Background", "Text", "Text", "Text", "Text", "Text", "Text", "Text"]],
  ["examples/21_signals.js", []],
  ["examples/22_physics.js", []],
  ["examples/23_animation.js", []],
  ["examples/24_tilemap.js", []],
  ["tests/parity/generated_form.js", ["Screen", "Column", "Text", "TextField", "TextField", "TextField", "TextArea", "Row", "Button", "Button"]],
  ["tests/parity/fields.js", ["Screen", "TextField", "TextArea"]],
  ["tests/parity/focus.js", ["Screen", "TextField", "TextField", "TextField"]],
  ["tests/parity/buttons_layout.js", ["Screen", "Column", "Text", "Row", "Button", "Button"]],
  ["tests/parity/long_text.js", ["Screen", "Column", "Text", "TextField", "TextField"]],
  ["tests/parity/basic_controls.js", ["Screen", "Slider", "Toggle", "Checkbox", "Dropdown"]],
  ["tests/parity/list_box.js", ["Screen", "ListBox"]],
  ["tests/parity/tree_view.js", ["Screen", "TreeView"]],
  ["tests/parity/progress.js", ["Screen", "Progress"]],
  ["tests/parity/plots.js", ["Screen", "PlotLines", "PlotHistogram", "DragFloat", "DragInt", "DragFloatRange2", "DragIntRange2", "SliderFloat", "SliderInt", "VSliderFloat", "VSliderInt", "SliderAngle", "InputFloat", "InputInt", "InputDouble", "SmallButton", "InvisibleButton", "ArrowButton", "Bullet", "Separator", "ColorEdit3", "ColorEdit4", "ColorPicker3", "ColorPicker4", "ColorButton", "TextColored", "TextDisabled", "TextWrapped", "LabelText", "BulletText"]],
  ["tests/parity/menus.js", ["Screen", "PopupMenu", "ContextMenu", "Tooltip", "Progress"]],
  ["tests/parity/selection_images.js", ["Screen", "Selectable", "CheckboxFlags", "ImageWithBg", "ImageButton", "SeparatorText"]],
  ["tests/parity/table_view.js", ["Screen", "TableView"]],
  ["tests/parity/widget_catalog.js", ["Screen", "Background", "TitleBar", "TopNav", "Toolbar", "BottomNav", "Column", "Text", "Row", "Button", "IconButton", "Href", "Stack", "Rect", "Line", "Bevel", "TextInRect", "TextLines", "Paragraph", "SelectableText", "ShowToast", "TextField", "TextArea", "Dropdown", "Slider", "Toggle", "Checkbox", "Radio", "Spinbox", "Combobox", "Progress", "ColorPicker", "LabelFrame", "Icon", "Picture", "Notebook", "ListBox", "TreeView", "SourceView", "TableView", "PanedView", "Collapsible", "Modal", "MessageDialog", "ConfirmDialog", "PromptDialog", "ActionModal", "CanvasGrid"]]
]);

for (const [relPath, widgets] of expected) {
  const mod = await import(pathToFileURL(`${outDir}/${relPath}`).href);
  assert.equal(typeof mod.createState, "function", `${relPath}: missing createState`);
  assert.equal(typeof mod.frame, "function", `${relPath}: missing frame`);
  const state = mod.createState();
  const snap = mod.frame(undefined, state);
  const got = snap.frame.map((item) => item.name);
  assert.deepEqual(got, widgets, `${relPath}: recorded widget stream`);
}

assert.equal(expected.size, 38, "conformance source count");
EOF

node "$work/runner.mjs" "$work/out"

echo "k2js runtime snapshot ok"

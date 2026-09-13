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
examples/19_images.kry
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
cp "$root"/web/*.js "$work/out/"
printf '%s\n' '{"type":"module"}' > "$work/out/package.json"

if rg -n '\b(BeginButton|BeginCard|BeginCanvas|BeginDisabled|EndCanvas|EndDisabled|InvisibleButton)\b' "$work/out" >/tmp/kryon-k2js-lowered.$$ 2>/dev/null; then
    cat /tmp/kryon-k2js-lowered.$$ >&2
    rm -f /tmp/kryon-k2js-lowered.$$
    echo "k2js output must use canonical widget names, not lowered scope names" >&2
    exit 1
fi
rm -f /tmp/kryon-k2js-lowered.$$

cat > "$work/runner.mjs" <<'EOF'
import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const outDir = process.argv[2];

const expected = new Map([
  ["examples/01_file_dialog.js", ["Screen", "AppBackground", "Text", "Text", "Button", "Button", "Button", "Text", "Text", "Text"]],
  ["examples/02_buttons.js", ["Screen","Text","Text","Text","Text","Text","Text","Text","Text","Text","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Text","Text","Text","Text","Text","Text","Text","Text","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button","Button","Button","Button","Button","Text","Button"]],
  ["examples/03_theme.js", ["Screen", "Background", "Text", "Text", "Text", "Button", "Button", "Text", "Button", "Button", "TextField", "Dropdown", "Slider", "Toggle", "Checkbox", "Text", "Text", "Text", "Text", "Text", "Text", "Text", "Text"]],
  ["examples/04_modal.js", ["Screen", "AppBackground", "Text", "Text", "Button", "Text", "Text", "Button"]],
  ["examples/05_color.js", ["Screen", "Background", "Text", "Box", "Box", "Text", "Box", "Box", "Text", "Box", "Box", "Text", "Text", "Text"]],
  ["examples/06_scaling.js", ["Screen", "AppBackground", "Text", "Box", "Text", "Text", "Text"]],
  ["examples/07_layout.js", ["Screen", "Background", "Text", "Box", "Line", "Line", "Box", "Text", "Text", "Text", "Button", "Button", "Button", "Text", "Text"]],
  ["examples/09_geometry.js", ["Screen", "Background", "Text", "Box", "Box", "Box", "Separator", "Text", "Text", "Box", "Box", "Box", "Box", "Text", "Text", "Box", "Text", "Text", "Text", "Separator", "Box", "Text"]],
  ["examples/10_menus.js", ["Screen", "AppBackground", "Menu", "Text"]],
  ["examples/11_basic_controls.js", ["Screen", "AppBackground", "Text", "Radio", "Radio", "Text", "Spinbox", "Dropdown", "Progress"]],
  ["examples/12_collections.js", ["Screen", "AppBackground", "ListBox", "TreeView", "TableView", "Text"]],
  ["examples/13_text_editor.js", ["Screen", "AppBackground", "TextArea", "Text", "Button", "Button"]],
  ["examples/14_canvas.js", ["Screen", "AppBackground", "Canvas", "CanvasGrid", "Box", "Box"]],
  ["examples/15_containers.js", ["Screen", "AppBackground", "TabBar", "PanedView", "Collapsible", "Text"]],
  ["examples/16_dialogs.js", ["Screen", "AppBackground", "Button", "Button", "Button", "ColorPicker"]],
  ["examples/17_keyboard_platform.js", ["Screen", "AppBackground", "Text", "Text", "Text"]],
  ["examples/18_accessibility.js", ["Screen", "AppBackground", "Text", "Checkbox", "Button"]],
  ["examples/19_images.js", ["Screen", "AppBackground", "Text", "Stack", "Box", "Image", "Image", "Text"]],
  ["examples/20_scene.js", []],
  ["examples/20_scroll.js", ["Screen", "AppBackground", "Background", "Text", "Scroll", "Text", "Text", "Text", "Text", "Text", "Text"]],
  ["examples/21_signals.js", []],
  ["examples/22_physics.js", []],
  ["examples/23_animation.js", []],
  ["examples/24_tilemap.js", []],
  ["tests/parity/generated_form.js", ["Screen", "Column", "Text", "TextField", "TextField", "TextField", "TextArea", "Row", "Button", "Button"]],
  ["tests/parity/fields.js", ["Screen", "TextField", "TextArea"]],
  ["tests/parity/focus.js", ["Screen", "TextField", "TextField", "TextField"]],
  ["tests/parity/buttons_layout.js", ["Disabled", "Disabled", "Disabled", "Screen", "Column", "Text", "Row", "Disabled", "Disabled", "Button", "Button", "Button"]],
  ["tests/parity/long_text.js", ["Screen", "Column", "Text", "TextField", "TextField"]],
    ["tests/parity/basic_controls.js", ["Screen", "Slider", "Toggle", "Checkbox", "Dropdown", "Selectable", "Checkbox", "Radio"]],
  ["tests/parity/list_box.js", ["Screen", "ListBox"]],
  ["tests/parity/tree_view.js", ["Screen", "TreeView"]],
  ["tests/parity/progress.js", ["Screen", "Progress"]],
  ["tests/parity/plots.js", ["Screen", "Plot", "Plot", "Drag", "Drag", "Drag", "Drag", "Slider", "Slider", "Slider", "Slider", "Slider", "Input", "Input", "Input", "Button", "Button", "Button", "Bullet", "Separator", "ColorPicker", "ColorPicker", "ColorPicker", "ColorPicker", "Button", "Text", "Text", "Text", "Text", "Text", "Bullet", "Text", "Text", "Text", "Text", "Text"]],
  ["tests/parity/menus.js", ["Screen", "Menu", "Menu", "Menu", "Popup", "Progress"]],
  ["tests/parity/selection_images.js", ["Screen", "Selectable", "Checkbox", "Image", "Button", "Separator", "Button", "TabBar", "DragDrop", "DragDrop", "ListBox"]],
  ["tests/parity/table_view.js", ["Screen", "TableView"]],
  ["tests/parity/widget_catalog.js", ["Screen", "Background", "TitleBar", "Toolbar", "Toolbar", "NavigationBar", "Column", "Text", "Row", "Button", "Button", "Link", "Stack", "Box", "Line", "Bevel", "Text", "Text", "Text", "Text", "Paragraph", "Text", "Toast", "TextField", "TextArea", "Dropdown", "Slider", "Toggle", "Checkbox", "Radio", "Spinbox", "Dropdown", "Progress", "ColorPicker", "Fieldset", "Image", "Icon", "Button", "Image", "TabBar", "ListBox", "TreeView", "Box", "Text", "Text", "TableView", "PanedView", "Collapsible", "Modal", "Modal", "Modal", "Modal", "Modal", "Canvas", "CanvasGrid"]]
]);

for (const [relPath, widgets] of expected) {
  const mod = await import(pathToFileURL(`${outDir}/${relPath}`).href);
  assert.equal(typeof mod.createState, "function", `${relPath}: missing createState`);
  assert.equal(typeof mod.frame, "function", `${relPath}: missing frame`);
  const state = mod.createState();
  const snap = mod.frame(undefined, state);
  const got = snap.frame.map((item) => item.name);
  if (relPath === "examples/02_buttons.js") {
    assert.equal(got.filter((name) => name === "Button").length, 181,
      `${relPath}: complete split-theme button matrices`);
    const headings = snap.frame.filter((item) => item.name === "Text").map((item) => item.args.text);
    assert.ok(headings.includes("DARK THEME") && headings.includes("LIGHT THEME"),
      `${relPath}: evaluated split-theme headings`);
    assert.ok(got.includes("Screen") && got.includes("Text"),
      `${relPath}: required layout primitives`);
    // Surface calls are currently recorded statements, not JS raster output.
    // The example uses shared styled surfaces, not the former Rect panels.
    assert.equal(snap.statements.filter((item) => /^Surface\(/.test(item.text)).length, 1540,
      `${relPath}: shared header columns and two theme panel surfaces`);
    continue;
  }
  assert.deepEqual(got, widgets, `${relPath}: recorded widget stream`);
}

assert.equal(expected.size, 38, "conformance source count");
EOF

node "$work/runner.mjs" "$work/out"

echo "k2js runtime snapshot ok"

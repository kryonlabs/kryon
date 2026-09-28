#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/src"

cat > "$work/ziran.toml" <<EOF
[package]
name = "plot_probe"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

[dependencies.Kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"

[tool.Kryon]
default_profile = "tui"

[tool.Kryon.profiles.tui]
backend = "terminal"
EOF

cat > "$work/ziran.local.toml" <<EOF
[overrides]
Kryon = "$repo"
ziran = "$repo/../ziran"
EOF

cat > "$work/src/app.zi" <<'EOF'
using UI :: #import "Kryon";
using Charts :: #import "PlotWidget";
using Styles :: #import "Kss";
using Coloring :: #import "Syntax";
using Tables :: #import "TableView";
using Trees :: #import "TreeView";

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    environment: KssEnvironment = KssDefaultEnvironment()
    if environment.theme != 0 { return 1 }
    if !SyntaxDarkBackground(cast(u32)0x101010ff) { return 1 }
    props: PlotProps
    props.scale_min = 0.0
    props.scale_max = 10.0
    range: PlotRange = PlotRangeFor(props.scale_min, props.scale_max,
        2.0, 8.0)
    if range.min_value != 0.0 || range.max_value != 10.0 {
        return 1
    }
    if TableViewSelectedRowFor(8, 3) != 2 ||
        TreeViewContentHeight(2, 20) != 40 {
        return 1
    }
    return 0
}
EOF

(cd "$work" && "$repo/../ziran/build/bin/ziran" lock &&
    "$repo/../ziran/build/bin/ziran" tool Kryon build --profile tui)
test -s "$work/build/generated/tui/ir/"*_plot.zir
test -s "$work/build/generated/tui/ir/"*_kss_parser.zir
test -s "$work/build/generated/tui/ir/"*_syntax.zir
test -s "$work/build/generated/tui/ir/"*_table_view.zir
test -s "$work/build/generated/tui/ir/"*_tree_view.zir
test -x "$work/build/plot_probe-tui"
"$work/build/plot_probe-tui" </dev/null > "$work/frame.txt"
test -s "$work/frame.txt"

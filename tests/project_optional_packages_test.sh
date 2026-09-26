#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/src"

cat > "$work/kryon.toml" <<EOF
[project]
name = "plot_probe"

[paths]
kryon = "$repo"
ziran = "$repo/../ziran"

[profiles.tui]
backend = "terminal"
EOF

cat > "$work/src/app.zi" <<'EOF'
#import "geometry"
#import "kss_parser"
#import "raylib_game"
#import "plot"
#import "plot_props"
#import "session"
#import "syntax"
#import "table_view"
#import "tree_view"

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
    if KEY_SPACE != 32 { return 1 }
    return 0
}
EOF

(cd "$work" && "$repo/build/bin/kryon" build --profile tui)
test -s "$work/build/generated/tui/ir/plot.zir"
test -s "$work/build/generated/tui/ir/kss_parser.zir"
test -s "$work/build/generated/tui/ir/syntax.zir"
test -s "$work/build/generated/tui/ir/table_view.zir"
test -s "$work/build/generated/tui/ir/tree_view.zir"
test -s "$work/build/generated/tui/ir/raylib_game.zir"
test -x "$work/build/plot_probe-tui"

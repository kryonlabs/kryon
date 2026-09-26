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
#import "plot"
#import "plot_props"
#import "session"

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    props: PlotProps
    props.scale_min = 0.0
    props.scale_max = 10.0
    range: PlotRange = PlotRangeFor(props.scale_min, props.scale_max,
        2.0, 8.0)
    if range.min_value != 0.0 || range.max_value != 10.0 {
        return 1
    }
    return 0
}
EOF

(cd "$work" && "$repo/build/bin/kryon" build --profile tui)
test -s "$work/build/generated/tui/ir/plot.zir"
test -x "$work/build/plot_probe-tui"

#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$work/src"

cat > "$work/ziran.toml" <<EOF
[package]
name = "kss_probe"
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
if [ -n "${RAYLIB_SOURCE:-}" ]; then
    printf 'raylib = "%s"\n' "$RAYLIB_SOURCE" >> "$work/ziran.local.toml"
fi

cat > "$work/src/app.zi" <<'EOF'
using UI :: #import "Kryon";
using Styles :: #import "Kss";

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    environment: KssEnvironment = KssDefaultEnvironment()
    if environment.theme != 0 { return 1 }
    return 0
}
EOF

(cd "$work" && "$repo/../ziran/build/bin/ziran" lock &&
    "$repo/../ziran/build/bin/ziran" tool Kryon build --profile tui)
test -s "$work/build/generated/tui/ir/"*_kss_parser.zir
test -x "$work/build/kss_probe-tui"
"$work/build/kss_probe-tui" </dev/null > "$work/frame.txt"
test -s "$work/frame.txt"

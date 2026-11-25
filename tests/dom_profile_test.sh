#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY WAYLAND_SOCKET XAUTHORITY GDK_DISPLAY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER XDG_SESSION_ID XDG_SESSION_TYPE
export YUE_DESKTOP_RECOVERY=0
export EMCC_CORES=1
work="$repo/build/scratch/dom-profile"
project="$work/project"
mkdir -p "$project/src"
copy_changed() {
    if ! cmp -s "$1" "$2"; then cp "$1" "$2"; fi
}
cat > "$work/ziran.toml" <<'EOF'
[package]
name = "dom_profile"
entry = "src/app.zi"
module_roots = ["src"]

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"

[tool.kryon]
default_profile = "web"

[tool.kryon.profiles.web]
backend = "dom"
platform = "web"
EOF
cat > "$work/ziran.local.toml" <<EOF
[overrides]
kryon = "$repo"
ziran = "$ziran_root"
EOF
copy_changed "$work/ziran.toml" "$project/ziran.toml"
copy_changed "$work/ziran.local.toml" "$project/ziran.local.toml"
copy_changed "$repo/tests/dom_profile_app.zi" "$project/src/app.zi"
(
    cd "$project"
    "$ziran" lock --offline
    "$ziran" tool kryon build --profile web
    test -s build/dom_profile-web.html
    test -s build/generated/web/ir/*_app.zir
    test -s build/generated/web/c/*_app.c
)

# Profile socket names fit Chromium's Unix-domain socket path limit.
profile=$(mktemp -d "$work/chromium.XXXXXX")
trap 'rm -rf "$profile"' EXIT HUP INT TERM
"$ziran" build --target=c --no-main --root "$repo/tests" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    -o "$work/browser-c" "$repo/tests/dom_profile_browser.zi"
"${CC:-cc}" -std=c11 -O1 -I"$ziran_root/include" -I"$work/browser-c" \
    "$work/browser-c"/*.c -lm -o "$work/browser-test"
status=0
DOM_TEST_SITE="$project/build" DOM_TEST_PROFILE="$profile" "$work/browser-test" || status=$?
if [ "$status" -ne 0 ]; then
    echo "DOM profile test failed at assertion $status" >&2
    tail -20 "$profile/browser.log" >&2 || true
    exit 1
fi
echo 'DOM profile: HTML semantics, browser focus and activation, keyed updates, viewport, CSS media and frame shutdown verified'

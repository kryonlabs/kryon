#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_tab_bar_widget_test.zi

widget_host=ziran_widget_noop_host
widget_mesh_host=ziran_mesh_noop_host
set --
. "$repo/tests/widget_raster_bindings.sh"

widget_native=1
widget_run_saved=1
widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"

# Execute native C from saved IR as well as the source matrix above.
"$ziran" build --target=c --root "$work/ir" --module-path "$work/ir" \
    -o "$work/saved-c" "$work/ir/ziran_tab_bar_widget_test.zir"
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$ziran_root/include" -I"$work/saved-c" "$work/saved-c"/*.c -o "$work/saved-c/app"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/saved-c/app"

# The same Tab routing contract also executes in WebAssembly in Chromium.
# No owner display or profile is inherited.
if test -n "${EMCC:-}" && test -x "$EMCC" && command -v chromium >/dev/null; then
    "$EMCC" -std=c11 -I"$ziran_root/include" -I"$work/native-c" \
        "$work/native-c"/*.c -sENVIRONMENT=web -sINVOKE_RUN=0 \
        -sEXPORTED_FUNCTIONS='["_main"]' -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 \
        -o "$work/browser.js"
    cat > "$work/browser.html" <<'HTML'
<!doctype html><body>pending<script>
var Module={onRuntimeInitialized(){
try { document.body.textContent='tab:'+Module._main(); }
catch(e) { document.body.textContent='failed:'+e; }
}};
</script><script src="browser.js"></script>
HTML
    mkdir -p "$work/home/runtime" "$work/home/config" "$work/home/data" "$work/home/cache" "$work/home/state"
    chmod 700 "$work/home/runtime"
    env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER \
        HOME="$work/home" XDG_RUNTIME_DIR="$work/home/runtime" \
        XDG_CONFIG_HOME="$work/home/config" XDG_DATA_HOME="$work/home/data" \
        XDG_CACHE_HOME="$work/home/cache" XDG_STATE_HOME="$work/home/state" YUE_DESKTOP_RECOVERY=0 \
        timeout 45 chromium --headless --no-sandbox --disable-gpu --no-first-run \
        --allow-file-access-from-files --user-data-dir="$work/profile" \
        --virtual-time-budget=30000 --dump-dom "file://$work/browser.html" \
        > "$work/browser-dom" 2> "$work/browser-errors"
    rg -q '<body>tab:0</body>' "$work/browser-dom" || {
        head -c 1000 "$work/browser-dom"
        tail -c 1000 "$work/browser-errors"
        exit 1
    }
    printf '%s\n' 'TabBar retained focus Chromium WebAssembly Tab routing test passed'
else
    printf '%s\n' 'TabBar retained focus browser test unavailable (set EMCC and install Chromium)' >&2
fi

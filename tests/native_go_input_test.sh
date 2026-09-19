#!/bin/sh
# Run Linux window and IME integration away from the user's desktop and bus.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "${KRYON_PRIVATE_INPUT_SESSION:-}" != 1 ]; then
    exec xvfb-run -a dbus-run-session -- env KRYON_PRIVATE_INPUT_SESSION=1 sh "$0"
fi
config=$(mktemp -d /tmp/kryon-input-test.XXXXXX)
trap 'rm -rf -- "$config"' EXIT
export XDG_CONFIG_HOME="$config"
export GSETTINGS_BACKEND=memory
export GIO_USE_VFS=local
export GTK_IM_MODULE=ibus
export XMODIFIERS=@im=ibus
export IBUS_ADDRESS="unix:path=$config/ibus.sock"
ibus-daemon --daemonize --panel disable --emoji-extension disable --address "$IBUS_ADDRESS"
attempt=0
while [ ! -S "$config/ibus.sock" ]; do
    attempt=$((attempt + 1))
    if [ "$attempt" -ge 100 ]; then
        echo 'Private IBus daemon did not start' >&2
        exit 1
    fi
    sleep 0.1
done
cd "$root/go/kryon"
KRYON_LIVE_IBUS_TEST=1 KRYON_LIVE_IBUS_HANGUL_TEST=1 KRYON_LIVE_X11_TEST=1 \
    go test -run '^(TestIBusLive|TestX11Live)' -v -timeout 30s -count=1

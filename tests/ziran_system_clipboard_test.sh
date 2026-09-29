#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_system_clipboard_test.zi

set -- \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost

"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/../ziran/std" "$@" \
    --entry ziran_system_clipboard_test:main -o "$work/clipboard.zib" "$source" "$repo/tests/ziran_system_clipboard_host.zi"
test "$("$ziran" run "$work/clipboard.zib")" = 0

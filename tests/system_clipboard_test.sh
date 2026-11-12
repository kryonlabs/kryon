#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/system_clipboard_test.zi

set -- \
    --bind kryon/system_clipboard:SystemClipboardOpenHost=system_clipboard_host:SystemClipboardOpenHost \
    --bind kryon/system_clipboard:SystemClipboardByteHost=system_clipboard_host:SystemClipboardByteHost \
    --bind kryon/system_clipboard:SystemClipboardWriteHost=system_clipboard_host:SystemClipboardWriteHost

"$ziran" bundle --root "$repo/tests" $kryon_project_args \
     "$@" \
    --entry system_clipboard_test:main -o "$work/clipboard.zx" "$source" "$repo/tests/system_clipboard_host.zi"
test "$("$ziran" run "$work/clipboard.zx")" = 0

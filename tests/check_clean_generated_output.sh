#!/bin/sh
set -eu

target=${1:?usage: check_clean_generated_output.sh PATH}

legacy_draw='Draw''UI'
legacy_text='UI''Text'
legacy_input='Text''InputControl'
legacy_render='UI''Render'
legacy_node='UI''NodeId'
legacy_begin='Begin''Drawing'
legacy_end='End''Drawing'
legacy_frame='Begin''UIFrameBox|UIFramePack|UIGridCell|UIPlace|UIFrame|UIGrid|UISide|UI_SIDE_'
legacy_canvas='Begin''UICanvas|EndUICanvas|UICanvas'
legacy_menu='UIMenu|UI_MENU_|UIContextMenu|UIAccelerator|DispatchUIAccelerators|UIAcceleratorPressed'
legacy_nav='UIBottomNav|UITopNav|UIToolbar|UITab|UISubtab|UIIconRow|UIPane|UIProfilePicture|UISidebarAccountHeader|UI_PANE_DROP_'
legacy_runtime='kry''runtime'
legacy_kryui='go/''kryui'
dot_import='import \. "github.com/waozixyz/kryon/go/kryon"'

matches="$(
    rg -n "${legacy_draw}|${legacy_text}|${legacy_input}|${legacy_render}|${legacy_node}|${legacy_begin}|${legacy_end}|${legacy_frame}|${legacy_canvas}|${legacy_menu}|${legacy_nav}|${legacy_runtime}|rt\\.|import \"C\"|${dot_import}|${legacy_kryui}" "$target" \
        --glob '*.go' \
        --glob '*.c' \
        --glob '*.h' || true
)"

if [ -n "$matches" ]; then
    echo "generated output contains blocked generated-runtime names:" >&2
    echo "$matches" >&2
    exit 1
fi

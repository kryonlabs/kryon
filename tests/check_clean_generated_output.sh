#!/bin/sh
set -eu

target=${1:?usage: check_clean_generated_output.sh PATH}

stale_draw='Draw''UI'
stale_text='UI''Text'
stale_input='Text''InputControl'
stale_render='UI''Render'
stale_node='UI''NodeId'
stale_key='UI''Key'
stale_begin='Begin''Drawing'
stale_end='End''Drawing'
stale_frame='Begin''UIFrameBox|UIFramePack|UIGridCell|UIPlace|UIGrid|UISide|UI_SIDE_'
stale_canvas='Begin''UICanvas|EndUICanvas|UICanvas'
stale_menu='UIMenu|UI_MENU_|UIContextMenu|UIAccelerator|DispatchUIAccelerators|UIAcceleratorPressed'
stale_nav='UIBottomNav|UITopNav|UIToolbar|UITab|UISubtab|UIIconRow|UIPane|UIProfilePicture|UISidebarAccountHeader|UI_PANE_DROP_'
stale_runtime='kry''runtime'
stale_kryui='go/''kryui'
dot_import='import \. "github.com/waozixyz/kryon/go/kryon"'

matches="$(
    rg -n "${stale_draw}|${stale_text}|${stale_input}|${stale_render}|${stale_node}|${stale_key}|${stale_begin}|${stale_end}|${stale_frame}|${stale_canvas}|${stale_menu}|${stale_nav}|${stale_runtime}|rt\\.|import \"C\"|${dot_import}|${stale_kryui}" "$target" \
        --glob '*.go' \
        --glob '*.c' \
        --glob '*.h' || true
)"

go_matches="$(
    rg -n 'Begin''UI|End''UI' "$target" \
        --glob '*.go' || true
)"

if [ -n "$matches$go_matches" ]; then
    echo "generated output contains blocked generated-runtime names:" >&2
    printf '%s\n%s\n' "$matches" "$go_matches" >&2
    exit 1
fi

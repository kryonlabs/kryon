#!/bin/sh
set -eu

root="${1:-.}"
cd "$root"

if [ "${KRYON_SMART_CHANGED+x}" = x ]; then
    changed="$(printf '%s\n' "$KRYON_SMART_CHANGED" | sed '/^$/d' | sort -u)"
else
    changed="$(
        {
            git diff --name-only --diff-filter=ACDMRTUXB
            git diff --cached --name-only --diff-filter=ACDMRTUXB
            git ls-files --others --exclude-standard
        } | sort -u
    )"
fi

if [ -z "$changed" ]; then
    exec make fast-test
fi

targets="canonical-surface-test"

needs_text=0
needs_public_names=0
needs_snapshot=0
needs_all_headers=0
needs_changed_headers=0
needs_image=0
needs_link=0
needs_collapsible=0
needs_paned_view=0
needs_title_bar=0
needs_paragraph=0
needs_bevel=0
needs_icon=0
needs_transition_fade=0
needs_modal=0
needs_menu=0
needs_tree_view=0
needs_table_view=0
needs_primitive=0
needs_layout=0
needs_group=0
needs_grid=0
needs_toast=0
needs_canvas=0
needs_drag_drop=0
needs_guide=0
needs_guide_pager=0
needs_scroll=0
needs_focus=0
needs_terminal_pane=0
needs_surface=0
needs_style=0
needs_style_sheet=0
needs_radio=0
needs_tab_bar=0
needs_text_policy=0
needs_text_input_policy=0
needs_examples_syntax=0
needs_go_runtime=0
needs_k2c_syntax=0
needs_k2cpp_syntax=0
needs_k2go_syntax=0
needs_k2js_syntax=0
needs_k2js_snapshot=0
needs_generated_parity=0
changed_headers=""
changed_examples=""
interesting=0

diff_matches()
{
    path="$1"
    pattern="$2"
    if git diff --quiet -- "$path" && git diff --cached --quiet -- "$path"; then
        return 2
    fi
    { git diff -U0 -- "$path"; git diff --cached -U0 -- "$path"; } |
        grep -Eq "$pattern"
}

while IFS= read -r path; do
    case "$path" in
        include/kryon.h|Makefile|tests/public_headers_compile_test.sh)
            needs_all_headers=1
            interesting=1
            ;;
        include/*.h)
            needs_changed_headers=1
            changed_headers="$changed_headers $path"
            interesting=1
            ;;
    esac

    case "$path" in
        include/*.h|scripts/public-api-snapshot.py|docs/PUBLIC_API_SNAPSHOT.txt)
            needs_snapshot=1
            needs_public_names=1
            interesting=1
            ;;
    esac

    case "$path" in
        docs/CANONICAL_WIDGET_SURFACE.md)
            interesting=1
            ;;
        include/*|src/*|src/*/*|cmd/*|cmd/*/*|go/*|go/*/*|runtime/*.kry|examples/*|tests/*|docs/API.md|docs/FEATURE_MATRIX.md|docs/FEATURE_MATRIX.html)
            needs_public_names=1
            interesting=1
            ;;
    esac

    case "$path" in
        include/ui_text*.h|src/ui/ui_text*.c|runtime/text*.kry|runtime/style.kry|runtime/surface.kry|examples/*.kry|tests/*text*|scripts/check-clean-text-api.py)
            needs_text=1
            interesting=1
            ;;
    esac

    case "$path" in
        examples/*.kry|tests/examples_syntax_test.sh)
            needs_examples_syntax=1
            changed_examples="$changed_examples $path"
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/focus.kry|src/ui/ui.c|src/ui/ui_tree.c|go/kryon/focus.go|tests/focus_policy_test.c|include/ui_tree.h)
            needs_focus=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/terminal_pane.kry|src/ui/terminal_pane.c|tests/terminal_pane_policy_test.c|include/terminal_pane.h)
            needs_terminal_pane=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/grid.kry|runtime/grid_props.kry|go/kryon/grid.go|go/kryon/grid_props.go|tests/grid_policy_test.c|include/ui_tree.h)
            needs_grid=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/layout.kry|src/ui/ui_tree.c|go/kryon/layout.go|tests/layout_policy_test.c|include/ui_tree.h)
            needs_layout=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/group.kry|go/kryon/group.go|tests/group_policy_test.c)
            needs_group=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/primitive.kry|src/ui/ui_tree.c|go/kryon/primitive.go|tests/primitive_policy_test.c|include/ui_tree.h)
            needs_primitive=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/table_view.kry|go/kryon/table_view.go|tests/table_view_policy_test.c|include/ui_tk.h)
            needs_table_view=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/tree_view.kry|go/kryon/tree_view.go|tests/tree_view_policy_test.c|include/ui_tk.h)
            needs_tree_view=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/modal.kry|src/ui/modal.c|go/kryon/modal.go|tests/modal_policy_test.c|include/ui_modal.h)
            needs_modal=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/transition_fade.kry|runtime/transition_props.kry|src/ui/ui_transition.c|include/ui_transition.h|include/ui_transition_props.generated.h|tests/transition_fade_policy_test.c|tests/transition_test.c)
            needs_transition_fade=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/toast.kry|src/ui/toast.c|go/kryon/toast.go|tests/toast_policy_test.c|include/ui_toast.h)
            needs_toast=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/canvas.kry|go/kryon/canvas.go|tests/canvas_policy_test.c|include/ui_tk.h)
            needs_canvas=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/drag_drop.kry|go/kryon/drag_drop.go|tests/drag_drop_policy_test.c|include/ui_tk.h)
            needs_drag_drop=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/guide_pager.kry|src/ui/pager.c|src/ui/ui_pager_internal.h|go/kryon/guide_pager.go|tests/guide_pager_policy_test.c)
            needs_guide_pager=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/guide.kry|src/ui/guide.c|go/kryon/guide.go|tests/guide_policy_test.c)
            needs_guide=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/icon.kry|src/ui/ui_tree.c|go/kryon/icon.go|tests/icon_policy_test.c)
            needs_icon=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/bevel.kry|src/ui/ui.c|go/kryon/bevel.go|tests/bevel_policy_test.c)
            needs_bevel=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/paragraph.kry|src/ui/ui.c|go/kryon/paragraph.go|tests/paragraph_policy_test.c)
            needs_paragraph=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/title_bar.kry|src/ui/ui_titlebar.c|go/kryon/title_bar.go|tests/title_bar_policy_test.c)
            needs_title_bar=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/paned_view.kry|src/ui/tab_bar.c|go/kryon/paned_view.go|tests/paned_view_policy_test.c)
            needs_paned_view=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/collapsible.kry|go/kryon/collapsible.go|tests/collapsible_policy_test.c)
            needs_collapsible=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/image.kry|src/ui/ui_image_cache.c|include/ui_image.h|src/ui/ui_image_internal.h|tests/image_policy_test.c)
            needs_image=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/link.kry|src/ui/ui.c|include/ui_page.h|tests/link_policy_test.c)
            needs_link=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/scroll.kry|src/ui/scroll.c|go/kryon/scroll.go|tests/scroll_policy_test.c|include/ui_scroll.h)
            needs_scroll=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/surface.kry|tests/surface_policy_test.c)
            needs_surface=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/style.kry|runtime/surface.kry|tests/style_policy_test.c)
            needs_style=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/style_sheet.kry|include/ui_style_sheet.h|src/ui/style_sheet.c|src/ui/style_picker.c|tests/style_sheet_policy_test.c|tests/style_pack_registry_test.c|tests/style_picker_test.c)
            needs_style_sheet=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/radio.kry|go/kryon/radio.go|tests/radio_policy_test.c)
            needs_radio=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/tab_bar.kry|src/ui/tab_bar.c|go/kryon/tab_bar.go|go/kryon/runtime.go|tests/tab_bar_policy_test.c)
            needs_tab_bar=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/text.kry|runtime/style.kry|runtime/surface.kry|tests/text_policy_test.c)
            needs_text_policy=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/menu.kry|tests/menu_policy_test.c)
            needs_menu=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/text_input.kry|src/ui/ui.c|tests/text_input_policy_test.c)
            needs_text_input_policy=1
            interesting=1
            ;;
    esac

    case "$path" in
        go/kryon/runtime.go|go/kryon/*_test.go|tests/generated_runtime_parity_test.sh|tests/parity/*.kry)
            needs_go_runtime=1
            interesting=1
            ;;
    esac

    case "$path" in
        cmd/kir/*|cmd/k2c/*|tests/parity/*.kry|tests/k2c_syntax_test.sh)
            needs_k2c_syntax=1
            interesting=1
            ;;
    esac

    case "$path" in
        cmd/kir/*|cmd/k2cpp/*|tests/parity/*.kry|tests/k2cpp_syntax_test.sh)
            needs_k2cpp_syntax=1
            interesting=1
            ;;
    esac

    case "$path" in
        cmd/kir/*|cmd/k2go/*|tests/parity/*.kry|tests/k2go_syntax_test.sh)
            needs_k2go_syntax=1
            interesting=1
            ;;
    esac

    case "$path" in
        cmd/kir/*|cmd/k2js/*|tests/parity/*.kry|tests/k2js_syntax_test.sh)
            needs_k2js_syntax=1
            interesting=1
            ;;
    esac

    case "$path" in
        cmd/k2js/*|web/kryon-runtime.js|web/kryon-runtime.d.ts|tests/k2js_runtime_snapshot_test.sh|tests/parity/*.kry)
            needs_k2js_snapshot=1
            interesting=1
            ;;
    esac

    case "$path" in
        cmd/k2c/*|cmd/k2go/*|cmd/k2js/*|cmd/kir/*|tests/parity/*.kry|tests/generated_runtime_parity_test.sh|web/kryon-runtime.js|web/kryon-runtime.d.ts)
            needs_generated_parity=1
            interesting=1
            ;;
    esac

    case "$path" in
        runtime/card.kry|runtime/control_props.kry|runtime/dropdown.kry|runtime/menu.kry|runtime/radio.kry|runtime/segmented_control.kry|runtime/text.kry|runtime/style.kry|runtime/surface.kry|runtime/theme.kry|runtime/toggle.kry)
            needs_k2c_syntax=1
            needs_k2cpp_syntax=1
            needs_k2go_syntax=1
            needs_k2js_syntax=1
            needs_k2js_snapshot=1
            needs_generated_parity=1
            interesting=1
            ;;
        runtime/*.kry)
            interesting=1
            ;;
    esac

    case "$path" in
        src/ui/ui_tk.c)
            interesting=1
            matched_ui_tk=0
            if diff_matches "$path" 'Collapsible|collapsible'; then
                needs_collapsible=1
                matched_ui_tk=1
            fi
            if diff_matches "$path" 'Paned|paned'; then
                needs_paned_view=1
                matched_ui_tk=1
            fi
            if diff_matches "$path" 'TreeView|tree_view'; then
                needs_tree_view=1
                matched_ui_tk=1
            fi
            if diff_matches "$path" 'TableView|table_view'; then
                needs_table_view=1
                matched_ui_tk=1
            fi
            if diff_matches "$path" 'Canvas|canvas'; then
                needs_canvas=1
                matched_ui_tk=1
            fi
            if diff_matches "$path" 'Drag|Drop|drag_drop'; then
                needs_drag_drop=1
                matched_ui_tk=1
            fi
            if diff_matches "$path" 'FocusDebugOverlay|focus'; then
                needs_focus=1
                matched_ui_tk=1
            fi
            if [ "$matched_ui_tk" -eq 0 ]; then
                needs_collapsible=1
                needs_paned_view=1
                needs_tree_view=1
                needs_table_view=1
                needs_canvas=1
                needs_drag_drop=1
            fi
            ;;
    esac
done <<EOF
$changed
EOF

if [ "$interesting" -eq 0 ]; then
    printf 'smart-test: changed files\n%s\n' "$changed"
    printf 'smart-test: running make %s\n' "$targets"
    make $targets
    git diff --check
    exit 0
fi

if [ "$needs_text" -eq 1 ]; then
    targets="$targets clean-text-api-check"
fi
if [ "$needs_public_names" -eq 1 ]; then
    targets="$targets public-api-names-check"
fi
if [ "$needs_snapshot" -eq 1 ]; then
    targets="$targets public-api-snapshot-check"
fi
if [ "$needs_all_headers" -eq 1 ]; then
    targets="$targets public-headers-compile-check"
elif [ "$needs_changed_headers" -eq 1 ]; then
    targets="$targets public-headers-compile-changed-check"
fi
if [ "$needs_image" -eq 1 ]; then
    targets="$targets image-policy-test"
fi
if [ "$needs_collapsible" -eq 1 ]; then
    targets="$targets collapsible-policy-test"
fi
if [ "$needs_paned_view" -eq 1 ]; then
    targets="$targets paned-view-policy-test"
fi
if [ "$needs_title_bar" -eq 1 ]; then
    targets="$targets title-bar-policy-test"
fi
if [ "$needs_paragraph" -eq 1 ]; then
    targets="$targets paragraph-policy-test"
fi
if [ "$needs_bevel" -eq 1 ]; then
    targets="$targets bevel-policy-test"
fi
if [ "$needs_icon" -eq 1 ]; then
    targets="$targets icon-policy-test"
fi
if [ "$needs_transition_fade" -eq 1 ]; then
    targets="$targets transition-fade-policy-test"
fi
if [ "$needs_modal" -eq 1 ]; then
    targets="$targets modal-policy-test"
fi
if [ "$needs_menu" -eq 1 ]; then
    targets="$targets menu-policy-test"
fi
if [ "$needs_tree_view" -eq 1 ]; then
    targets="$targets tree-view-policy-test"
fi
if [ "$needs_table_view" -eq 1 ]; then
    targets="$targets table-view-policy-test"
fi
if [ "$needs_primitive" -eq 1 ]; then
    targets="$targets primitive-policy-test"
fi
if [ "$needs_layout" -eq 1 ]; then
    targets="$targets layout-policy-test"
fi
if [ "$needs_group" -eq 1 ]; then
    targets="$targets group-policy-test"
fi
if [ "$needs_grid" -eq 1 ]; then
    targets="$targets grid-policy-test"
fi
if [ "$needs_toast" -eq 1 ]; then
    targets="$targets toast-policy-test"
fi
if [ "$needs_canvas" -eq 1 ]; then
    targets="$targets canvas-policy-test"
fi
if [ "$needs_drag_drop" -eq 1 ]; then
    targets="$targets drag-drop-policy-test"
fi
if [ "$needs_guide" -eq 1 ]; then
    targets="$targets guide-policy-test"
fi
if [ "$needs_guide_pager" -eq 1 ]; then
    targets="$targets guide-pager-policy-test"
fi
if [ "$needs_scroll" -eq 1 ]; then
    targets="$targets scroll-policy-test"
fi
if [ "$needs_focus" -eq 1 ]; then
    targets="$targets focus-policy-test"
fi
if [ "$needs_terminal_pane" -eq 1 ]; then
    targets="$targets terminal-pane-policy-test"
fi
if [ "$needs_link" -eq 1 ]; then
    targets="$targets link-policy-test"
fi
if [ "$needs_surface" -eq 1 ]; then
    targets="$targets surface-policy-test"
fi
if [ "$needs_style" -eq 1 ]; then
    targets="$targets style-policy-test"
fi
if [ "$needs_style_sheet" -eq 1 ]; then
    targets="$targets style-sheet-policy-test style-pack-registry-test style-picker-test"
fi
if [ "$needs_radio" -eq 1 ]; then
    targets="$targets radio-policy-test"
fi
if [ "$needs_tab_bar" -eq 1 ]; then
    targets="$targets tab-bar-policy-test"
fi
if [ "$needs_text_policy" -eq 1 ]; then
    targets="$targets text-policy-test"
fi
if [ "$needs_text_input_policy" -eq 1 ]; then
    targets="$targets text-input-policy-test"
fi
if [ "$needs_examples_syntax" -eq 1 ]; then
    targets="$targets examples-syntax-test examples-manifest-check"
fi
if [ "$needs_go_runtime" -eq 1 ]; then
    targets="$targets go-runtime-test"
fi
if [ "$needs_k2c_syntax" -eq 1 ]; then
    targets="$targets k2c-syntax-test"
fi
if [ "$needs_k2cpp_syntax" -eq 1 ]; then
    targets="$targets k2cpp-syntax-test"
fi
if [ "$needs_k2go_syntax" -eq 1 ]; then
    targets="$targets k2go-syntax-test"
fi
if [ "$needs_k2js_syntax" -eq 1 ]; then
    targets="$targets k2js-syntax-test"
fi
if [ "$needs_k2js_snapshot" -eq 1 ]; then
    targets="$targets k2js-runtime-snapshot-test"
fi
if [ "$needs_generated_parity" -eq 1 ]; then
    targets="$targets generated-runtime-parity-test"
fi

printf 'smart-test: changed files\n%s\n' "$changed"
printf 'smart-test: running make %s\n' "$targets"
make $targets KRYON_CHANGED_HEADERS="$changed_headers" KRYON_CHANGED_EXAMPLES="$changed_examples"
git diff --check

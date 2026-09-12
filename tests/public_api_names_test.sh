#!/usr/bin/env sh
set -eu

root="${1:-.}"
cd "$root"

matches="$(
    bad_prefix_a='UI''Draw'
    bad_prefix_b='Draw''UIStyled'
    bad_prefix_c='Kry''LoadPic''ture'
    bad_prefix_d='Kry''ImageFit'
    bad_prefix_e='Kry''DrawPic''ture'
    helper_d='UIPic''ture'
    helper_e='UI_PIC''TURE_FIT_'
    helper_f='DrawPic''ture'
    helper_a='Load''ImageTexture'
    helper_b='Image''FitRect'
    helper_c='Image''Texture'
    rg -n "\b(${bad_prefix_a}[A-Za-z0-9_]*|${bad_prefix_b}[A-Za-z0-9_]*|${bad_prefix_c}[A-Za-z0-9_]*|${bad_prefix_d}[A-Za-z0-9_]*|${bad_prefix_e}[A-Za-z0-9_]*|${helper_a}|${helper_b}|${helper_c}|${helper_d}[A-Za-z0-9_]*|${helper_e}[A-Za-z0-9_]*|${helper_f})\b" \
        include docs examples \
        --glob '!vendor/**' \
        --glob '!docs/AGENTS.md' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$matches" ]; then
    echo "Image API must use ImageProps names without stale framework prefixes:"
    echo "$matches"
    exit 1
fi

removed_widget_matches="$(
    rg -n '\b(Href|Picture|LabelFrame|Combo|BeginCombo|EndCombo|CloseCombo|ComboProps|ComboFlags)\b' \
        include src cmd go web docs examples tests tools scripts \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!docs/CANONICAL_WIDGET_SURFACE.md' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$removed_widget_matches" ]; then
    echo "Removed widget names must stay out of public/runtime/codegen surfaces; use Link, Image, Dropdown, Popup, or Menu:"
    echo "$removed_widget_matches"
    exit 1
fi

form_matches="$(
    rg -n '\bUIForm[A-Za-z0-9_]*\b' \
        include/ui_rows.h src/ui/rows.c docs/API.md docs/FEATURE_MATRIX.md \
        docs/FEATURE_MATRIX.html tests/ui_tree_api_test.c || true
)"

if [ -n "$form_matches" ]; then
    echo "Form cursor API must use the canonical Form names:"
    echo "$form_matches"
    exit 1
fi

generated_matches="$(
    rg -n '\b(TextInputControl|GenericButton|TextButton|LocaleDropdown|VerticalSlider|VerticalSliderWithMarks|ReadonlyTextBox|DrawCenteredUIControlText|UIDropdownOption|DropdownEx|SetUIDropdownClipTop|SetUIDropdownClipBottom|RenderDropdown|RenderDropdownEx|UIParagraphSpec|UIParagraphLayout|UIModalAction|UINodeId|UIKey|UISide|UI_SIDE_[A-Z_]+|UIFrame|UIGrid|BeginUIFrameBox|UIFramePack|UIGridCell|UIPlace|PageGrid|GridLayout|GridLayoutProps|UICanvas|BeginUICanvas|EndUICanvas|UIMenuItemKind|UIMenuItem|UIMenuBarResult|UIMenu|UI_MENU_[A-Z_]+|UIContextMenu|UIAccelerator|UIAcceleratorPressed|DispatchUIAccelerators|UIIconRowItem|UIIconRowResult|UIBottomNavItem|UIBottomNavResult|UIBottomNavOption|UIBottomNavConfigResult|UIToolbarAction|UIToolbarResult|UIToolbarHeaderResult|UISubtab|UITab|UIPaneDropZone|UIPaneTabBar|UIPaneTabBarResult|GetUIPaneDropZone|GetUITabBarHeight|UI_PANE_DROP_[A-Z_]+|UISidebarAccountHeaderSpec|UISidebarAccountHeaderResult|UIProfilePicturePickerModal|UIProfilePicturePickerResult)\b' \
        go/kryon include/ui_controls.h include/ui_tree.h include/ui_tk.h include/ui_nav.h include/ui_profile.h include/ui_draw.h include/ui_modal.h src/ui/dropdown.c src/ui/ui_node_registry.c cmd/k2b examples tests/k2c_syntax_test.sh tests/k2go_syntax_test.sh docs/API.md docs/RUNTIME_PARITY.md docs/FEATURE_MATRIX.md docs/FEATURE_MATRIX.html \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$generated_matches" ]; then
    echo "Generated runtime surface must use clean widget/layout names such as Button, TextField, FrameBox, Grid, and Canvas:"
    echo "$generated_matches"
    exit 1
fi

rect_matches="$(
    rg -n '\bRectangleShape\b' \
        include src cmd docs examples tests \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$rect_matches" ]; then
    echo "Rect is the canonical rectangle widget; do not reintroduce RectangleShape:"
    echo "$rect_matches"
    exit 1
fi

public_button_matches="$(
    rg -n '\b(UIButtonSpec|UIButtonNode)\b' \
        include/ui_controls.h \
        include/ui_tree.h \
        README.md \
        src/ui/button.c \
        src/ui/modal.c \
        src/ui/ui_internal.h \
        src/ui/ui_tk.c \
        src/ui/ui_tree.c \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_button_matches" ]; then
    echo "Public button APIs must use clean ButtonSpec/ButtonNode names without stale UI prefixes:"
    echo "$public_button_matches"
    exit 1
fi

stale_tree_api_matches="$(
    rg -n '\b(BeginUI|EndUI|InvalidateUI|NextUIEvent|UIReconcileTree|UILayoutTree|UIRouteInput|UIUpdateTree|UIGetTreeNodes|UIGetNodeHeight(ById)?|UIGetNode|UIHitTestNode|UIGetAccessibilitySnapshot|SetUIAccessibilitySink|UINode[A-Za-z0-9_]*)\b' \
        include src cmd go docs examples tests \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$stale_tree_api_matches" ]; then
    echo "Tree APIs must use clean names such as BeginTree, GetTreeNodes, and NodeParagraph:"
    echo "$stale_tree_api_matches"
    exit 1
fi

button_style_matches="$(
    rg -n '\b(UIButtonStyle[A-Za-z0-9_]*|UI_BUTTON_STYLE_[A-Z_]+|ButtonStyle|StyledButton|RenderStyledButton)\b' \
        include \
        src/ui \
        go/kryon \
        cmd/k2go \
        cmd/k2b \
        docs/API.md \
        examples \
        tests/k2c_syntax_test.sh \
        tests/k2go_syntax_test.sh \
        tests/parity \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$button_style_matches" ]; then
    echo "Buttons must use semantic tone/emphasis/state properties, with no named-style legacy API:"
    echo "$button_style_matches"
    exit 1
fi

text_size_matches="$(
    rg -n '\bUI_TEXT(_BASE_SIZE|_[0-9]+)\b' \
        include \
        src \
        go/kryon \
        cmd/k2go \
        cmd/k2b \
        docs/API.md \
        examples \
        tests/k2c_syntax_test.sh \
        tests/k2go_syntax_test.sh \
        tests/krb_cartridge_test.sh \
        tests/parity \
        tests/perf \
        tests/spec \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$text_size_matches" ]; then
    echo "Text size APIs must use clean Text8/Text16/TextBaseSize names without stale UI_TEXT prefixes:"
    echo "$text_size_matches"
    exit 1
fi

cursor_api_matches="$(
    rg -n '\b(MarkUI[A-Za-z0-9_]*|SetUICursor[A-Za-z0-9_]*|GetUIMouseCursor|MarkUITextCursor)\b' \
        include src docs examples tests \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$cursor_api_matches" ]; then
    echo "Cursor intent APIs must use clean Mark*/SetCursor*/GetMouseCursorIntent names without stale UI prefixes:"
    echo "$cursor_api_matches"
    exit 1
fi

reorder_api_matches="$(
    rg -n '\b(UIReorder[A-Za-z0-9_]*|UpdateUIReorder[A-Za-z0-9_]*)\b' \
        include src docs examples tests \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$reorder_api_matches" ]; then
    echo "Reorder APIs must use clean Reorder* names without stale UI prefixes:"
    echo "$reorder_api_matches"
    exit 1
fi

window_api_matches="$(
    rg -n '\b(UIWindow|OpenUIWindow|CloseUIWindow|BeginUIWindow|EndUIWindow|IsUIWindowClicked|IsUIWindowRightClicked|IsUIWindowDragged|GetUIWindowPosition|GetUIWindowClickPosition|UI_WINDOW_[A-Z_]+|PumpUIWindows|StealUICoreWindowClose)\b' \
        include/ui_window.h src/ui/ui_window.c tests/ui_window_test.c tests/texture_scope_test.c docs/API.md docs/FEATURE_MATRIX.md docs/FEATURE_MATRIX.html examples || true
)"

if [ -n "$window_api_matches" ]; then
    echo "Window APIs must use PumpWindows and StealCoreWindowClose without stale UI prefixes:"
    echo "$window_api_matches"
    exit 1
fi

if [ -d go/kryui ]; then
    echo "The removed go/kryui cgo bridge package must not exist; generated Go uses go/kryon." >&2
    exit 1
fi

stale_doc_matches="$(
    stale_bridge='go/''kryui'
    stale_input='Text''InputControl'
    stale_queue='Queue''UITextInput'
    rg -n "${stale_bridge}|${stale_input}|${stale_queue}" \
        docs/RUNTIME_PARITY.md docs/FEATURE_MATRIX.md docs/FEATURE_MATRIX.html \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$stale_doc_matches" ]; then
    echo "Generated runtime docs must describe the clean native Go and C surfaces, not removed bridge/input names:"
    echo "$stale_doc_matches"
    exit 1
fi

draw_ui_pattern='Draw''UI[A-Za-z0-9_]*'
api_doc_matches="$(
    rg -n "\b(${draw_ui_pattern}|UITextInputControlNode|QueueUITextInput[A-Za-z0-9_]*|UIGenericButtonNode|UIVerticalSliderNode|UIActionModalNode|UIModalNode|UIModal3ButtonNode|UIModalFrameNode)\b" \
        docs/API.md || true
)"

if [ -n "$api_doc_matches" ]; then
    echo "Public API docs must advertise clean generated/runtime names, not stale widget helpers:"
    echo "$api_doc_matches"
    exit 1
fi

public_stale_input='Queue''UITextInput'
public_stale_matches="$(
    rg -n "${public_stale_input}[A-Za-z0-9_]*" \
        include/ui_controls.h src/ui/ui.c docs/API.md \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_stale_matches" ]; then
    echo "Public text input queue APIs must use clean QueueTextInput* names:"
    echo "$public_stale_matches"
    exit 1
fi

stale_ui_fragment='UI'
public_control_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${stale_ui_fragment}[A-Za-z0-9_]*\b" \
        include/ui_controls.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_control_draw_matches" ]; then
    echo "Public control headers must expose clean widget names, not stale draw-prefixed internals:"
    echo "$public_control_draw_matches"
    exit 1
fi

public_composite_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${stale_ui_fragment}[A-Za-z0-9_]*\b|\b(ShowUIToast|ShowUIToastFor|ClearUIToast)\b" \
        include/ui_overlay.h \
        include/ui_rows.h \
        include/ui_toast.h \
        include/ui_modal.h \
        include/ui_nav.h \
        include/ui_tk.h \
        go/kryon \
        tests/k2go_syntax_test.sh \
        docs/FEATURE_MATRIX.md \
        docs/FEATURE_MATRIX.html \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_composite_draw_matches" ]; then
    echo "Public/generated composite widget surfaces must use clean names such as TabBar, Modal, and ShowToast:"
    echo "$public_composite_draw_matches"
    exit 1
fi

public_text_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${stale_ui_fragment}[A-Za-z0-9_]*\b" \
        include/ui_text.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_draw_matches" ]; then
    echo "Public text headers must expose clean text names, not stale draw-prefixed internals:"
    echo "$public_text_draw_matches"
    exit 1
fi

public_text_helper_matches="$(
    rg -n '\b(UITextStyle|UISelectableTextBlock|MeasureUIText|GetUITextHeight|GetUITextLineHeight|MeasureScaledUIText|PushUITextSelectable|PopUITextSelectable|GetUITextY|GetScaledUITextY|DrawFittedUITextInRect)\b' \
        include/ui_text.h \
        include/ui_draw.h \
        docs/API.md \
        docs/site/highlight.js \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_helper_matches" ]; then
    echo "Public text helper APIs must use clean Text* names without stale UIText prefixes:"
    echo "$public_text_helper_matches"
    exit 1
fi

split_text_widget_matches="$(
    # These are forbidden widget functions. A palette's TextDisabled field is
    # a color property, not a second Text implementation.
    rg -n '\b(TextInRect|TextColored|TextDisabled|TextWrapped)\s*\(' \
        include/ui_tree.h \
        go/kryon/api.go \
        go/kryon/runtime.go || true
)"

if [ -n "$split_text_widget_matches" ]; then
    echo "Text behavior must be properties of the canonical Text widget:"
    echo "$split_text_widget_matches"
    exit 1
fi

split_tooltip_widget_matches="$(
    rg -n '\bTooltipProps\b|\bTooltip\s*\(' \
        include/ui_tree.h \
        include/ui_tk.h \
        go/kryon \
        cmd/k2go \
        cmd/kir \
        examples \
        tests/parity \
        tests/k2c_syntax_test.sh \
        tests/k2cpp_syntax_test.sh \
        tests/k2go_syntax_test.sh || true
)"

if [ -n "$split_tooltip_widget_matches" ]; then
    echo "Tooltip content must use the canonical Popup scope with PopupTooltip:"
    echo "$split_tooltip_widget_matches"
    exit 1
fi

split_context_popup_matches="$(
    rg -n '\b(BeginPopupContext|EndPopupContext|ContextPopupProps)\b' \
        include/ui_tree.h \
        include/ui_tk.h \
        go/kryon \
        cmd/k2go \
        cmd/kir \
        examples \
        tests/parity || true
)"

if [ -n "$split_context_popup_matches" ]; then
    echo "Context popup content must use the canonical Popup scope with PopupContext:"
    echo "$split_context_popup_matches"
    exit 1
fi

if [ ! -f docs/CANONICAL_WIDGET_SURFACE.md ]; then
    echo "docs/CANONICAL_WIDGET_SURFACE.md must exist as the shared widget/node naming review surface."
    exit 1
fi

registry_doc_misses="$(
    python3 - <<'PY'
from pathlib import Path
import re

registry = Path("src/ui/ui_node_registry.c").read_text()
doc = Path("docs/CANONICAL_WIDGET_SURFACE.md").read_text()
names = re.findall(r'\{"([^"]+)"\s*,', registry)
for name in names:
    if f"`{name}`" not in doc:
        print(name)
PY
)"

if [ -n "$registry_doc_misses" ]; then
    echo "Canonical widget surface doc must list every registered public node/widget name:"
    echo "$registry_doc_misses"
    exit 1
fi

registry_game2d_misses="$(
    python3 - <<'PY'
from pathlib import Path
import re

registry = Path("src/ui/ui_node_registry.c").read_text()
scene = Path("include/scene_tree.h").read_text()
entries = re.findall(r'\{"([^"]+)"\s*,\s*"[^"]*"\s*,\s*"([^"]+)"', registry)
kinds = set(re.findall(r'\bNODE_([A-Z0-9_]+)\b', scene))
aliases = {
    "Scene": "ROOT",
    "Node2D": "NODE2D",
    "Camera2D": "CAMERA2D",
    "Sprite2D": "SPRITE2D",
    "AnimatedSprite2D": "ANIMATED_SPRITE2D",
    "TileMap": "TILEMAP",
    "CollisionShape2D": "COLLISION_SHAPE2D",
    "Area2D": "AREA2D",
    "Body2D": "BODY2D",
    "Light2D": "LIGHT2D",
}
for name, group in entries:
    if not group.startswith("Game2D/"):
        continue
    expected = aliases.get(name, re.sub(r'(?<=[a-z0-9])(?=[A-Z])', '_', name).upper())
    if expected not in kinds:
        print(f"{name}: missing NODE_{expected}")
PY
)"

if [ -n "$registry_game2d_misses" ]; then
    echo "Game2D registry names must map to real scene NodeKind values:"
    echo "$registry_game2d_misses"
    exit 1
fi

registry_snippet_misses="$(
    python3 - <<'PY'
from pathlib import Path

registry = Path("src/ui/ui_node_registry.c").read_text()
required = {
    "Text": "Text((TextProps){",
    "Checkbox": "Checkbox((CheckboxProps){",
}
for name, needle in required.items():
    if needle not in registry:
        print(f"{name}: missing {needle}")
for stale in ('Text("', 'Checkbox(%d,'):
    if stale in registry:
        print(f"stale snippet form: {stale}")
PY
)"

if [ -n "$registry_snippet_misses" ]; then
    echo "Editor registry snippets must use canonical props-based widget calls:"
    echo "$registry_snippet_misses"
    exit 1
fi

doc_status_misses="$(
    python3 - <<'PY'
from pathlib import Path
import re

doc = Path("docs/CANONICAL_WIDGET_SURFACE.md").read_text()
statuses = (
    ".kry canonical",
    "Native canonical",
    "Native support",
    "Composite candidate",
    "Rename review",
    "Remove after migration",
)
for status in statuses:
    if status not in doc:
        print(status)
PY
)"

if [ -n "$doc_status_misses" ]; then
    echo "Canonical widget surface doc must keep the review status key complete:"
    echo "$doc_status_misses"
    exit 1
fi

public_text_layout_matches="$(
    rg -n '\b(UITextLayout|UITextElement|UITextElementType|ParseUITextLayout|ReflowUITextLayout|GetUITextLayoutHeight|FreeUITextLayout|RenderTextLayout)\b' \
        include/ui_text_layout.h \
        docs/API.md \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_layout_matches" ]; then
    echo "Public text layout APIs must use TextLayout/TextElement names without stale UIText prefixes:"
    echo "$public_text_layout_matches"
    exit 1
fi

public_text_platform_matches="$(
    rg -n '\b(UITextInputPlatformCallback|SetUITextInputPlatformCallback)\b' \
        include/ui_core.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_platform_matches" ]; then
    echo "Public platform text input callbacks must use TextInputPlatformCallback names without stale UIText prefixes:"
    echo "$public_text_platform_matches"
    exit 1
fi

public_text_input_matches="$(
    rg -n '\b(UITextInputStyle|UITextInputFilter|UITextEdit|EditUIText|GetUITextAreaSelection|SetUITextAreaSelection|UITextInput|UISyntaxMode|UISyntax[A-Za-z0-9_]*|UI_SYNTAX_[A-Z_]+)\b' \
        go/kryon \
        cmd/k2go \
        include/ui_controls.h \
        docs/API.md \
        examples \
        src/ui/ui.c \
        src/ui/ui_node_registry.c \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_input_matches" ]; then
    echo "Public text input APIs must use TextInputStyle/TextEdit/SyntaxMode names without stale UIText/UISyntax prefixes:"
    echo "$public_text_input_matches"
    exit 1
fi

internal_text_input_matches="$(
    rg -n '\b(RenderTextInputControl|RenderTextField|RenderTextArea|UITextFieldState|UITextSelection|UITextAreaHeightCacheEntry)\b' \
        src/ui/ui.c \
        src/ui/ui_tree.c \
        src/ui/rows.c \
        src/ui/ui_tk.c \
        src/ui/ui_internal.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$internal_text_input_matches" ]; then
    echo "Text input implementation must use clean internal names without stale UIText prefixes:"
    echo "$internal_text_input_matches"
    exit 1
fi

internal_button_matches="$(
    rg -n '\b(RenderButton|RenderTextButton|RenderGenericButton)\b' \
        src/ui/button.c \
        src/ui/rows.c \
        src/ui/bottom_nav.c \
        src/ui/modal.c \
        src/ui/ui_tk.c \
        src/ui/ui_tree.c \
        src/ui/ui_internal.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$internal_button_matches" ]; then
    echo "Button implementation must use clean internal Render* names without stale draw prefixes:"
    echo "$internal_button_matches"
    exit 1
fi

public_tree_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${stale_ui_fragment}[A-Za-z0-9_]*\b" \
        include/ui_tree.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_tree_draw_matches" ]; then
    echo "Public retained tree headers must expose clean lifecycle names, not stale draw-prefixed internals:"
    echo "$public_tree_draw_matches"
    exit 1
fi

frame_begin='Begin''Drawing'
frame_end='End''Drawing'
frame_alias_matches="$(
    rg -n "\b(${frame_begin}|${frame_end})\s*\(" \
        examples tests \
        --glob '*.kry' \
        --glob '*.sh' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$frame_alias_matches" ]; then
    echo "Kry source must use clean frame names BeginFrame and EndFrame:"
    echo "$frame_alias_matches"
    exit 1
fi

kry_source_draw_matches="$(
    rg -n '\bDraw[A-Z][A-Za-z0-9_]*\s*\(|\bRenderToggleSwitch\b|\bRender[A-Z][A-Za-z0-9_]*\s*\(' \
        examples runtime tests src \
        --glob '*.kry' \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$kry_source_draw_matches" ]; then
    echo "Kry source must use clean declarative names such as Circle, Line, Triangle, Button, and Toggle:"
    echo "$kry_source_draw_matches"
    exit 1
fi

kryc_name='kry''c'
kryc_tool_matches="$(
    find . \
        -path './.git' -prune -o \
        -path './build' -prune -o \
        -path './vendor' -prune -o \
        -name "$kryc_name" -print
)"

if [ -n "$kryc_tool_matches" ]; then
    echo "Do not add a kryc tool. Kryon uses k2go for Go and k2c for C:"
    echo "$kryc_tool_matches"
    exit 1
fi

kryc_text_matches="$(
    rg -n "\b${kryc_name}\b" \
        README.md Makefile .github docs examples go include scripts src tests tools \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$kryc_text_matches" ]; then
    echo "Do not document or reference kryc in user-facing/runtime surfaces; use k2go and k2c:"
    echo "$kryc_text_matches"
    exit 1
fi

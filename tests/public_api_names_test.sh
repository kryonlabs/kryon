#!/usr/bin/env sh
set -eu

root="${1:-.}"
cd "$root"

matches="$(
    bad_prefix_a='UI''Draw'
    bad_prefix_b='Draw''UIStyled'
    bad_prefix_c='Kry''LoadPicture'
    bad_prefix_d='Kry''PictureFit'
    bad_prefix_e='Kry''DrawPicture'
    helper_d='UIPicture'
    helper_e='UI_PICTURE_FIT_'
    helper_f='DrawPicture'
    helper_a='Load''PictureTexture'
    helper_b='Picture''FitRect'
    helper_c='Picture''Texture'
    rg -n "\b(${bad_prefix_a}[A-Za-z0-9_]*|${bad_prefix_b}[A-Za-z0-9_]*|${bad_prefix_c}[A-Za-z0-9_]*|${bad_prefix_d}[A-Za-z0-9_]*|${bad_prefix_e}[A-Za-z0-9_]*|${helper_a}|${helper_b}|${helper_c}|${helper_d}[A-Za-z0-9_]*|${helper_e}[A-Za-z0-9_]*|${helper_f})\b" \
        include docs examples \
        --glob '!vendor/**' \
        --glob '!docs/AGENTS.md' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$matches" ]; then
    echo "Picture API must use Picture/PictureProps names without legacy framework prefixes:"
    echo "$matches"
    exit 1
fi

generated_matches="$(
    rg -n '\b(TextInputControl|GenericButton|TextButton|LocaleDropdown|VerticalSlider|VerticalSliderWithMarks|ReadonlyTextBox)\b' \
        go/kryon include/ui_tree.h src/ui/ui_node_registry.c cmd/k2b examples tests/k2g_syntax_test.sh docs/RUNTIME_PARITY.md docs/FEATURE_MATRIX.md \
        --glob '!vendor/**' \
        --glob '!build/**' \
        --glob '!tests/public_api_names_test.sh' || true
)"

if [ -n "$generated_matches" ]; then
    echo "Generated runtime surface must use clean widget names such as Button, Dropdown, Slider, Text, and TextField:"
    echo "$generated_matches"
    exit 1
fi

if [ -d go/kryui ]; then
    echo "The legacy go/kryui cgo bridge package must not exist; generated Go uses go/kryon." >&2
    exit 1
fi

legacy_doc_matches="$(
    legacy_bridge='go/''kryui'
    legacy_input='Text''InputControl'
    legacy_queue='Queue''UITextInput'
    rg -n "${legacy_bridge}|${legacy_input}|${legacy_queue}" \
        docs/RUNTIME_PARITY.md docs/FEATURE_MATRIX.md docs/FEATURE_MATRIX.html \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$legacy_doc_matches" ]; then
    echo "Generated runtime docs must describe the clean native Go and C surfaces, not legacy bridge/input names:"
    echo "$legacy_doc_matches"
    exit 1
fi

api_doc_matches="$(
    rg -n '\b(DrawUI[A-Za-z0-9_]*|UITextInputControlNode|QueueUITextInput[A-Za-z0-9_]*|UIGenericButtonNode|UIVerticalSliderNode)\b' \
        docs/API.md || true
)"

if [ -n "$api_doc_matches" ]; then
    echo "Public API docs must advertise clean generated/runtime names, not legacy widget helpers:"
    echo "$api_doc_matches"
    exit 1
fi

public_legacy_input='Queue''UITextInput'
public_legacy_matches="$(
    rg -n "${public_legacy_input}[A-Za-z0-9_]*" \
        include/ui_controls.h src/ui/ui.c docs/API.md \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_legacy_matches" ]; then
    echo "Public text input queue APIs must use clean QueueTextInput* names:"
    echo "$public_legacy_matches"
    exit 1
fi

legacy_ui_fragment='UI'
public_control_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${legacy_ui_fragment}[A-Za-z0-9_]*\b" \
        include/ui_controls.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_control_draw_matches" ]; then
    echo "Public control headers must expose clean widget names, not DrawUI* internals:"
    echo "$public_control_draw_matches"
    exit 1
fi

public_composite_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${legacy_ui_fragment}[A-Za-z0-9_]*\b|\b(ShowUIToast|ShowUIToastFor|ClearUIToast)\b" \
        include/ui_overlay.h \
        include/ui_rows.h \
        include/ui_toast.h \
        include/ui_modal.h \
        include/ui_nav.h \
        include/ui_tk.h \
        go/kryon \
        tests/k2g_syntax_test.sh \
        docs/FEATURE_MATRIX.md \
        docs/FEATURE_MATRIX.html \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_composite_draw_matches" ]; then
    echo "Public/generated composite widget surfaces must use clean names such as ThemeSettings, TabBar, ModalFrame, and ShowToast:"
    echo "$public_composite_draw_matches"
    exit 1
fi

public_text_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${legacy_ui_fragment}[A-Za-z0-9_]*\b" \
        include/ui_text.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_draw_matches" ]; then
    echo "Public text headers must expose clean text names, not DrawUI* internals:"
    echo "$public_text_draw_matches"
    exit 1
fi

public_text_layout_matches="$(
    rg -n '\b(UITextLayout|UITextElement|UITextElementType|ParseUITextLayout|ReflowUITextLayout|GetUITextLayoutHeight|FreeUITextLayout|DrawUITextLayout)\b' \
        include/ui_text_layout.h \
        docs/API.md \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_layout_matches" ]; then
    echo "Public text layout APIs must use TextLayout/TextElement names without legacy UIText prefixes:"
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
    echo "Public platform text input callbacks must use TextInputPlatformCallback names without legacy UIText prefixes:"
    echo "$public_text_platform_matches"
    exit 1
fi

public_text_input_matches="$(
    rg -n '\b(UITextInputStyle|UITextInputFilter|UITextEdit|EditUIText|GetUITextAreaSelection|SetUITextAreaSelection|UITextInput)\b' \
        include/ui_controls.h \
        docs/API.md \
        examples \
        src/ui/ui_node_registry.c \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_text_input_matches" ]; then
    echo "Public text input APIs must use TextInputStyle/TextEdit/EditText names without legacy UIText prefixes:"
    echo "$public_text_input_matches"
    exit 1
fi

public_tree_draw_matches="$(
    rg -n "\bDraw[A-Za-z0-9_]*${legacy_ui_fragment}[A-Za-z0-9_]*\b" \
        include/ui_tree.h \
        --glob '!vendor/**' \
        --glob '!build/**' || true
)"

if [ -n "$public_tree_draw_matches" ]; then
    echo "Public retained tree headers must expose clean lifecycle names, not DrawUI* internals:"
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

kryc_name='kry''c'
kryc_tool_matches="$(
    find . \
        -path './.git' -prune -o \
        -path './build' -prune -o \
        -path './vendor' -prune -o \
        -name "$kryc_name" -print
)"

if [ -n "$kryc_tool_matches" ]; then
    echo "Do not add a kryc tool. Kryon uses k2g for Go and k2c for C:"
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
    echo "Do not document or reference kryc in user-facing/runtime surfaces; use k2g and k2c:"
    echo "$kryc_text_matches"
    exit 1
fi

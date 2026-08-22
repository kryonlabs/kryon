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
        go/kryon include/ui_tree.h src/ui/ui_node_registry.c examples tests/k2g_syntax_test.sh docs/RUNTIME_PARITY.md docs/FEATURE_MATRIX.md \
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

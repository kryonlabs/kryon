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

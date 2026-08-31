#!/bin/sh
set -eu

usage()
{
    cat >&2 <<EOF
usage: subset-fonts.sh OUT_DIR SOURCE_FONT_DIR NAME_PREFIX CORPUS...

Create app-sized subsets of Kryon's bundled Noto fonts.

OUT_DIR          directory for generated font files
SOURCE_FONT_DIR directory containing NotoSans*.ttf/otf sources
NAME_PREFIX     app/product name inserted into output filenames
CORPUS          files or directories with text to keep in the subsets
EOF
    exit 2
}

if [ "$#" -lt 4 ]; then
    usage
fi

out_dir=$1
source_dir=$2
name_prefix=$3
shift 3

pyftsubset=${PYFTSUBSET:-}
if [ -n "$pyftsubset" ]; then
    command -v "$pyftsubset" >/dev/null 2>&1 || {
        echo "PYFTSUBSET points to a missing executable: $pyftsubset" >&2
        exit 1
    }
    run_pyftsubset()
    {
        "$pyftsubset" "$@"
    }
elif command -v pyftsubset >/dev/null 2>&1; then
    run_pyftsubset()
    {
        pyftsubset "$@"
    }
elif python3 -m fontTools.subset --help >/dev/null 2>&1; then
    run_pyftsubset()
    {
        python3 -m fontTools.subset "$@"
    }
else
    echo "fonttools subsetter is required. Install fonttools or set PYFTSUBSET." >&2
    exit 1
fi

tmp=${TMPDIR:-/tmp}/kryon-font-corpus.$$
cleanup()
{
    rm -f "$tmp"
}
trap cleanup EXIT INT HUP TERM

mkdir -p "$out_dir"
: > "$tmp"

append_corpus()
{
    path=$1

    if [ -f "$path" ]; then
        cat "$path" >> "$tmp"
        printf '\n' >> "$tmp"
    elif [ -d "$path" ]; then
        find "$path" -type f | LC_ALL=C sort | while IFS= read -r file; do
            cat "$file" >> "$tmp"
            printf '\n' >> "$tmp"
        done
    else
        echo "Missing font corpus input: $path" >&2
        exit 1
    fi
}

for corpus in "$@"; do
    append_corpus "$corpus"
done

subset_font()
{
    src=$1
    out=$2

    if [ ! -f "$src" ]; then
        echo "Missing source font: $src" >&2
        exit 1
    fi

    run_pyftsubset "$src" \
        --output-file="$out" \
        --text-file="$tmp" \
        --layout-features='*' \
        --glyph-names \
        --symbol-cmap \
        --legacy-cmap \
        --notdef-glyph \
        --notdef-outline \
        --recommended-glyphs \
        --name-IDs='*' \
        --name-legacy \
        --name-languages='*' \
        --no-hinting
}

subset_font "$source_dir/NotoSans-Regular.ttf" "$out_dir/NotoSans-${name_prefix}-Regular.ttf"
subset_font "$source_dir/NotoSansSC-Regular.otf" "$out_dir/NotoSansSC-${name_prefix}-Regular.otf"
subset_font "$source_dir/NotoSansJP-Regular.otf" "$out_dir/NotoSansJP-${name_prefix}-Regular.otf"
subset_font "$source_dir/NotoSansKR-Regular.otf" "$out_dir/NotoSansKR-${name_prefix}-Regular.otf"
subset_font "$source_dir/NotoSansTC-Regular.otf" "$out_dir/NotoSansTC-${name_prefix}-Regular.otf"

ls -lh "$out_dir"/NotoSans*"${name_prefix}"-Regular.*

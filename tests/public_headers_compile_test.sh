#!/usr/bin/env sh
set -eu

if [ "$#" -gt 0 ]; then root="$1"; shift; else root="."; fi
if [ "$#" -gt 0 ]; then build_dir="$1"; shift; else build_dir="build/header-check"; fi
if [ "$#" -gt 0 ]; then cc_cmd="$1"; shift; else cc_cmd="${CC:-cc}"; fi
if [ "$#" -gt 0 ]; then cppflags="$1"; shift; else cppflags=""; fi
if [ "$#" -gt 0 ]; then cflags="$1"; shift; else cflags=""; fi

cd "$root"

work_dir="$build_dir/tests/public-headers"
rm -rf "$work_dir"
mkdir -p "$work_dir"
generated_include="$work_dir/generated/include"
generated_src="$work_dir/generated/src"

python3 scripts/embed-icon-sheets.py icons "$generated_src/ui/ui_icon_assets.c" \
    --types-output "$generated_include/ui_icon_types.h" \
    --names-output "$generated_src/ui/ui_icon_names.c"

status=0

headers="$*"
if [ -z "$headers" ]; then
    headers="include/*.h"
fi

for header in $headers; do
    name=$(basename "$header")
    case "$name" in
        kryon_plan9.h)
            # This shim intentionally depends on Plan 9 libc replacement
            # headers that are not part of the normal hosted C include path.
            continue
            ;;
    esac

    source="$work_dir/${name%.h}.c"
    object="$work_dir/${name%.h}.o"
    log="$work_dir/${name%.h}.log"

    {
        printf '#include "kryon.h"\n'
        printf '#include "%s"\n' "$name"
        printf 'int main(void) { return 0; }\n'
    } > "$source"

    if ! $cc_cmd $cppflags $cflags -I"$generated_include" -Iinclude -c "$source" -o "$object" >"$log" 2>&1; then
        echo "Public header does not compile for a normal consumer after kryon.h: $name" >&2
        cat "$log" >&2
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "public headers compile ok"
fi

exit "$status"

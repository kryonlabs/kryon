#!/usr/bin/env sh
set -eu

root="${1:-.}"
build_dir="${2:-build/header-check}"
cc_cmd="${3:-${CC:-cc}}"
cppflags="${4:-}"
cflags="${5:-}"

cd "$root"

work_dir="$build_dir/tests/public-headers"
rm -rf "$work_dir"
mkdir -p "$work_dir"

status=0

for header in include/*.h; do
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

    if ! $cc_cmd $cppflags $cflags -Iinclude -c "$source" -o "$object" >"$log" 2>&1; then
        echo "Public header does not compile for a normal consumer after kryon.h: $name" >&2
        cat "$log" >&2
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "public headers compile ok"
fi

exit "$status"

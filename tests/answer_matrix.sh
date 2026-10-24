# Source/saved C, C++, Go and portable Answer=42 matrix.
"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/app.zi
        root=$work
        module_path=$repo/src/ui
    else
        source=$work/ir/app.zir
        root=$work/ir
        module_path=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$module_path" --module-path "$ziran_root/std" \
        --entry app:Answer -o "$work/$input.zx" "$source"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" \
            --root "$root" --module-path "$module_path" --module-path "$ziran_root/std" \
            -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$ziran_root/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" \
                -I"$output" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cp "$work/answer_test.go" "$output/$answer_go_file"
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

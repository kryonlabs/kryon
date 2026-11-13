# Source/saved C, Go and portable Answer=42 matrix.
mv "$work/app.zi" "$work/test_case.zi"
sed -i 's/App_Answer/TestCase_Answer/g' "$work/answer_test.go"
"$ziran" ir --root "$work" --project --module-path $work \
    -o "$work/ir" "$work/test_case.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/test_case.zi
        root=$work
        module_path="--project --module-path $work"
    else
        source=$work/ir/test_case.zir
        root=$work/ir
        module_path="--module-path $work/ir"
    fi
    "$ziran" bundle --root "$root" $module_path  \
        --entry test_case:Answer -o "$work/$input.zx" "$source"
    test "$("$ziran" run "$work/$input.zx")" = 42
    for target in c go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" \
            --root "$root" $module_path  \
            -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "test_case.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$ziran_root/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        else
            cp "$work/answer_test.go" "$output/$answer_go_file"
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

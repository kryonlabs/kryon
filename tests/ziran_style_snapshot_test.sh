#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/style_snapshot_behavior.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for input in "$source" "$work/ir/style_snapshot_behavior.zir"; do
    "$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
        --module-path "$ziran_root/std" --entry style_snapshot_behavior:Answer \
        -o "$work/style.zx" "$input"
    test "$("$ziran" run "$work/style.zx")" = 42
    for target in c go; do
        output=$work/$target
        "$ziran" build "--target=$target" --root "$repo/tests" \
            --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
            --entry style_snapshot_behavior:Answer -o "$output" "$input"
        case $target in
            c)
                cat > "$output/main.c" <<'C'
#include "style_snapshot_behavior.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
                "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$output" "$output"/*.c -o "$work/run"
                "$work/run"
                ;;
            go)
                cat > "$output/main.go" <<'GO'
package ziran
import "testing"
func TestSnapshot(t *testing.T) { if StyleSnapshotBehavior_Answer() != 42 { t.Fatal("wrong snapshot") } }
GO
                mv "$output/main.go" "$output/main_test.go"
                (cd "$output" && GO111MODULE=off go test)
                ;;
        esac
    done
done

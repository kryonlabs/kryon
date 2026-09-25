#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#import "instance"
#import "tree"

#program_export
Answer :: () -> s32 {
    partial: TreeCommitPlan = TreeCommitFor(true, 10, 9)
    if !partial.keep_committed || partial.reconcile || partial.route_input {
        return 0
    }
    complete: TreeCommitPlan = TreeCommitFor(true, 10, 10)
    if complete.keep_committed || !complete.reconcile || !complete.route_input {
        return 0
    }
    inactive: TreeCommitPlan = TreeCommitFor(false, 10, 0)
    if inactive.keep_committed || !inactive.reconcile || !inactive.route_input {
        return 0
    }
    empty: TreeCommitPlan = TreeCommitFor(true, 0, 0)
    if empty.keep_committed || !empty.reconcile || !empty.route_input {
        return 0
    }
    invalid: TreeCommitPlan = TreeCommitFor(true, 10, -1)
    if invalid.keep_committed || !invalid.reconcile || !invalid.route_input {
        return 0
    }
    if !InstanceExpired(-1) || InstanceExpired(0) ||
        InstanceExpired(12) || !InstanceExpired(13) { return 0 }
    return 42
}
EOF

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        module_dir=$repo/src/ui
    else
        module=$work/ir/app.zir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --root "$work" \
            --module-path "$module_dir" -o "$output" "$module"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/tree_test.go" <<'GO'
package ziran
import "testing"
func TestTreeCommit(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("tree commit") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done

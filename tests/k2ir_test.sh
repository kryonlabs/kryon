#!/bin/sh
set -eu

k2ir=${1:-$(ls build/$(uname -s | tr [:upper:] [:lower:])-*/bin/k2ir build/*/bin/k2ir 2>/dev/null | head -1)}
root=${2:-.}
work=${TMPDIR:-build}/kryon-k2ir-test.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

if [ ! -f "$k2ir" ]; then
    echo "k2ir not found: $k2ir" >&2
    exit 1
fi

mkdir -p "$work/src" "$work/out"
cat > "$work/src/app.kry" <<'EOF'
#module "demo.app"
#import "kryon.h"
ui :: #import "src/ui/panel"

WEB :: #defined(PLATFORM_WEB)
platform_ping :: (value: int, tag: const char*) -> int #extern

state {
    click_count: int = 0
}

Counter :: (app: App*) {
    Text("Count")
    if Button("Increment") {
        app->click_count += 1
    }
}
EOF

"$k2ir" --root "$work" -o "$work/out" "$work/src/app.kry"
kir=$work/out/src/app.kir
test -f "$kir"
grep -Fq 'kir 1' "$kir"
grep -Fq 'module demo.app source src/app.kry span src/app.kry:1:1' "$kir"
grep -Fq 'import header kryon.h target kryon.h' "$kir"
grep -Fq 'import module ui target src/ui/panel' "$kir"
grep -Fq 'import extern platform_ping target platform_ping' "$kir"
grep -Fq 'signature platform_ping :: (value: int, tag: const char*) -> int #extern' "$kir"
grep -Fq 'state click_count type int init 0' "$kir"
grep -Fq 'function Counter args app: App* return void' "$kir"
grep -Fq 'stmt expr widget Text("Count") text Text("Count")' "$kir"
grep -Fq 'stmt if widget  text if Button("Increment") {' "$kir"
grep -Fq 'stmt assign widget  text app->click_count += 1' "$kir"
if grep -Fq 'function WEB' "$kir"; then
    echo "top-level #defined binding was emitted as a function" >&2
    exit 1
fi

echo "k2ir ok"

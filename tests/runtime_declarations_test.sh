#!/bin/sh
# Generated public declarations must match the canonical runtime modules.
set -eu
build=${1:-build/linux-x86_64}
root=$(pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/kryon-runtime-declarations.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

python3 scripts/embed-runtime-declarations.py "$work/runtime_declarations.generated.h"
cmp "$work/runtime_declarations.generated.h" cmd/kir/runtime_declarations.generated.h

"$build/bin/k2c" --strict --no-main --root . -o "$work/c" runtime/*.kry
"$build/bin/k2go" --strict --no-main --runtime-implementation --pkg kryon \
    --root . -o "$work/go" runtime/*.kry
gofmt -w "$work/go"/*.go
cmp "$work/c/runtime/button_props.h" include/ui_button_props.generated.h
for source in runtime/*.kry; do
    name=$(basename "$source" .kry)
    cmp "$work/go/$name.go" "go/kryon/$name.go"
done

# Preserve the C struct tag as well as the typedef when moving a public record.
cat > "$work/consumer.c" <<'C'
#include "ui_button_props.generated.h"
int main(void) {
    struct ButtonProps value = {0};
    ButtonProps *props = &value;
    props->disabled = true;
    return props->disabled ? 0 : 1;
}
C
${CC:-cc} -Iinclude -I"$build/generated/include" "$work/consumer.c" -o "$work/consumer"
"$work/consumer"
# A relocated compiler resolves native props without a source checkout nearby.
cp "$build/bin/k2go" "$work/k2go"
cat > "$work/props.kry" <<'KRY'
#module "props"
Read :: () -> i32 {
    props: ButtonProps = (ButtonProps){(Rectangle){1, 2, 3, 4}, "label", 17}
    return props.font
}
Change :: (props: ButtonProps) {
    props.font = 99
}
ReadZero :: () -> i32 {
    props: ButtonProps
    props.font = 18
    Change(props)
    return props.font
}
KRY
(cd "$work" && ./k2go --no-main --pkg props --root . -o app props.kry)
cat > "$work/app/go.mod" <<MOD
module props

go 1.25.0
require github.com/waozixyz/kryon/go/kryon v0.0.0
replace github.com/waozixyz/kryon/go/kryon => "$root/go/kryon"
MOD
cat > "$work/app/props_test.go" <<'GO'
package props
import "testing"
func TestProps(t *testing.T) {
    if Props_Read() != 17 || Props_ReadZero() != 18 {
        t.Fatal("positional native props did not follow the declaration")
    }
}
GO
(cd "$work/app" && GOWORK=off go test -mod=mod ./...)
cp "$build/bin/k2js" "$work/k2js"
(cd "$work" && ./k2js --no-main --runtime ./kryon-runtime.js --root . -o js props.kry)
cp web/*.js "$work/js/"
printf '{"type":"module"}\n' > "$work/js/package.json"
cat > "$work/js/check.mjs" <<'JS'
import { Props_Read, Props_ReadZero } from "./props.js";
if (Props_Read() !== 17 || Props_ReadZero() !== 18) {
    throw new Error("native props lost their field order, zero value, or copy semantics");
}
JS
node "$work/js/check.mjs"
echo "runtime declarations match generated C and Go interfaces"

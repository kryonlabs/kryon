#!/bin/sh
# Generated public declarations must match the canonical runtime modules.
set -eu
build=${1:-build/linux-x86_64}
work=$(mktemp -d "${TMPDIR:-/tmp}/kryon-runtime-declarations.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

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
echo "runtime declarations match generated C and Go interfaces"

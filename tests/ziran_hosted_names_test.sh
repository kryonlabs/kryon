#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work="$repo/build/scratch/hosted-names"
mkdir -p "$work/c"
"$ziran" build --target=c --no-main --entry hosted_names_test:Check --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    -o "$work/c" "$repo/tests/hosted_names_test.zi"
cat > "$work/c/main.c" <<'C'
#include "hosted_names_test.h"
int main(void) { return Check(); }
C
includes=$(find "$work/c" -type d -printf ' -iquote %p')
# shellcheck disable=SC2046,SC2086
"${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections -I"$ziran_root/include" $includes \
    $(find "$work/c" -name '*.c') -Wl,--gc-sections -lm -o "$work/test"
"$work/test"
echo 'Hosted app/widget module names: canonical imports accepted, unrelated capabilities refused'

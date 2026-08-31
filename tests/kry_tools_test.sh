#!/bin/sh
set -eu

root=${1:-.}
work=${TMPDIR:-/tmp}/kryon-tools-test.$$

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

mkdir -p "$work/locales"

cat > "$work/messy.kry" <<'EOF'
#import "kryon.h"
state {
count:int=0
}
App::() #ui {
Screen root:{
bounds={0,0,320,240}
Button {
id=1
label=t("app.save")
style=ButtonStylePrimary
font=Text16
bounds={8,8,96,28}
}
}
}
EOF

sh "$root/scripts/kry-fmt.sh" "$work/messy.kry"
grep -Fq 'App :: () #ui {' "$work/messy.kry"
grep -Fq '    count:int = 0' "$work/messy.kry"
grep -Fq '        Button {' "$work/messy.kry"
sh "$root/scripts/kry-fmt.sh" --check "$work/messy.kry"

cat > "$work/locales/en.txt" <<'EOF'
[app.save]
Save
---
EOF

cat > "$work/locales/es.txt" <<'EOF'
[app.save]
Guardar
---
EOF

sh "$root/scripts/kry-locale-check.sh" "$work/messy.kry" -- \
    "$work/locales/en.txt" "$work/locales/es.txt"

cat > "$work/locales/es-bad.txt" <<'EOF'
[app.save]
Save
---
EOF

if sh "$root/scripts/kry-locale-check.sh" "$work/messy.kry" -- \
    "$work/locales/en.txt" "$work/locales/es-bad.txt" 2>"$work/locale.err"; then
    echo "locale-check accepted copied English text" >&2
    exit 1
fi
grep -Fq 'copies English text' "$work/locale.err"

echo "kry tools ok"

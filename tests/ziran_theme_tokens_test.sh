#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/scripts/export_theme_tokens.zi

"$ziran" ir --root "$repo/scripts" --module-path "$ziran_root/std" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/scripts" --module-path "$ziran_root/std" \
    --entry export_theme_tokens:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry export_theme_tokens:main -o "$work/saved.zx" \
    "$work/ir/export_theme_tokens.zir"
cmp "$work/source.zx" "$work/saved.zx"

"$ziran" run --target=c --entry export_theme_tokens:main \
    --root "$repo/scripts" "$source" > "$work/native.css"
"$ziran" run "$work/source.zx" | sed '$d' > "$work/source-zx.css"
"$ziran" run "$work/saved.zx" | sed '$d' > "$work/saved-zx.css"
cmp "$work/native.css" "$work/source-zx.css"
cmp "$work/source-zx.css" "$work/saved-zx.css"

output=$work/native.css
for token in \
    '--kryon-background: #ffffffff;' \
    '--kryon-focus-gap: 3px;' \
    '--kryon-control-height-small: 30px;' \
    '--kryon-control-height-medium: 36px;' \
    '--kryon-control-height-large: 44px;' \
    '--kryon-disabled-opacity: 0.550;' \
    '--kryon-shadow-raised-2-opacity: 0.035;' \
    '--kryon-shadow-floating-2-opacity: 0.060;' \
    '--kryon-series-6: #eb6834ff;' \
    '--kryon-motion-theme: 320ms;'; do
    rg -q -F -- "$token" "$output" || {
        echo "missing canonical token: $token" >&2
        exit 1
    }
done

dark=$(awk "/^\[data-theme='dark'\]/{seen=1} seen && /^--kryon-background:/{print; exit}" "$output")
test "$dark" = '--kryon-background: #121212ff;'
reduced=$(awk '/@media \(prefers-reduced-motion: reduce\)/{seen=1} seen && /^--kryon-motion-multiplier:/{print; exit}' "$output")
test "$reduced" = '--kryon-motion-multiplier: 0;'

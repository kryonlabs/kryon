# Source/saved-IR C, Go and ZX execution for standalone main tests.
# The caller supplies source, work, repo, ziran and its host bindings in $@.
# Text tests compile the C embedding directly; retained-frame tests select
# the compiler's executable path. Every configured target keeps both forms.
matrix_name=${source##*/}
matrix_name=${matrix_name%.zi}
case "$matrix_bundles/$matrix_c/$matrix_forms" in
    before/driver/both|before/none/both|after-native/cc/both|before/linked/source) ;;
    *) return 2 ;;
esac

matrix_input() {
    matrix_module=$source
    matrix_root=$repo/tests
    matrix_modules=$repo/src/ui
    if test "$1" = saved; then
        matrix_module=$work/ir/$matrix_name.zir
        matrix_root=$work/ir
        matrix_modules=$work/ir
    fi
}

matrix_bundle() {
    if test "$matrix_bundles" = before && test "$matrix_form" = saved; then
        "$ziran" bundle --root "$matrix_root" --module-path "$matrix_modules" "$@" \
            --entry "$matrix_name:main" -o "$work/$matrix_form.zx" "$matrix_module"
    else
        "$ziran" bundle --root "$matrix_root" --module-path "$matrix_modules" \
            --module-path "$matrix_std" "$@" --entry "$matrix_name:main" \
            -o "$work/$matrix_form.zx" "$matrix_module"
    fi
}

matrix_run_bundle() {
    if test "$matrix_forms" = source; then
        matrix_result=$("$ziran" run "$work/$matrix_form.zx")
        printf '%s bundle: %s\n' "$matrix_form" "$matrix_result"
        test "$matrix_result" = 0
    else
        test "$("$ziran" run "$work/$matrix_form.zx")" = 0
    fi
}

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$matrix_std" -o "$work/ir" "$source"
if test "$matrix_bundles" = before; then
    for matrix_form in source saved; do
        matrix_input "$matrix_form"
        matrix_bundle "$@"
    done
    cmp "$work/source.zx" "$work/saved.zx"
    for matrix_form in source saved; do matrix_run_bundle; done
fi

for matrix_form in source saved; do
    if test "$matrix_form" = saved && test "$matrix_forms" = source; then continue; fi
    matrix_input "$matrix_form"
    matrix_output=$work/$matrix_form-c
    if test "$matrix_forms" = source; then matrix_output=$work/c; fi
    if test "$matrix_c" = driver; then
        "$ziran" build --target=c --entry "$matrix_name:main" --exe \
            --root "$matrix_root" --module-path "$matrix_modules" \
            --module-path "$matrix_std" -o "$matrix_output" "$matrix_module"
        "$matrix_output/$matrix_name"
    elif test "$matrix_c" = cc; then
        "$ziran" build --target=c --no-main --entry "$matrix_name:main" \
            --root "$matrix_root" --module-path "$matrix_modules" \
            --module-path "$matrix_std" -o "$matrix_output" "$matrix_module"
        "${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections -I"$matrix_output" \
            "$matrix_output"/*.c -Wl,--gc-sections -o "$matrix_output/run"
        "$matrix_output/run"
    elif test "$matrix_c" = linked; then
        "$ziran" build --target=c --root "$matrix_root" --module-path "$matrix_modules" \
            --module-path "$matrix_std" -o "$matrix_output" "$matrix_module"
        "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -I"$ziran_root/include" -I"$matrix_output" "$matrix_output"/*.c \
            -o "$matrix_output/$matrix_name"
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            "$matrix_output/$matrix_name"
        printf 'c %s: passed\n' "$matrix_report"
    else
        test "$matrix_c" = none
    fi
    matrix_output=$work/$matrix_form-go
    if test "$matrix_forms" = source; then matrix_output=$work/go; fi
    "$ziran" build --target=go --pkg main --exe --entry "$matrix_name:main" "$@" \
        --root "$matrix_root" --module-path "$matrix_modules" \
        --module-path "$matrix_std" -o "$matrix_output" "$matrix_module"
    if test "$matrix_forms" = source; then
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            GO111MODULE=off go run "$matrix_output"/*.go
        printf 'go %s: passed\n' "$matrix_report"
    else
        GO111MODULE=off go run "$matrix_output"/*.go
    fi
    if test "$matrix_bundles" = after-native; then
        matrix_bundle "$@"
        matrix_run_bundle
    fi
done
if test "$matrix_bundles" = after-native; then cmp "$work/source.zx" "$work/saved.zx"; fi

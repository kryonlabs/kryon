#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/component-lifecycle"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export HARMONY_DESKTOP_RECOVERY=0
for name in component_lifecycle_test component_actions_test; do
    extension=zi
    set --
    if test "$name" = component_actions_test; then
        extension=tsx
        # The actual Box widget submits real tree nodes. This state/lifetime
        # check binds its device-only raster effects to the existing test host.
        set -- --bind-host ziran_widget_noop_host --bind-host ziran_mesh_noop_host
    fi
    source="$repo/tests/$name.$extension"
    output="$work/$name"
    mkdir -p "$output"
    # Save checked modules before entry pruning. Both forms bind providers later.
    "$ziran" ir --root "$repo/tests" $kryon_ui_args -o "$output/ir" "$source"
    for form in source saved; do
        input=$source
        root=$repo/tests
        module_args=$kryon_ui_args
        if test "$form" = saved; then input=$output/ir/$name.zir; root=$output/ir; module_args="--module-path $output/ir"; fi
        "$ziran" bundle --root "$root" $module_args "$@" --entry "$name:main" -o "$output/$form.zx" "$input"
        answer=$("$ziran" run "$output/$form.zx")
        printf '%s\n' "$answer"
        test "$(printf '%s\n' "$answer" | tail -1)" = 0
        for target in c go; do
            package=
            if test "$target" = go; then package='--pkg main'; fi
            "$ziran" build --target="$target" --exe $package --root "$root" $module_args "$@" \
                --entry "$name:main" -o "$output/$form-$target" "$input"
            if test "$target" = go; then GO111MODULE=off go run "$output/$form-go/"*.go
            else "$output/$form-c/$name"; fi
        done
    done
    cmp "$output/source.zx" "$output/saved.zx"
done
echo 'Component lifecycle, owned presentation and authored actions source/saved ZIR: C, Go and ZX passed'

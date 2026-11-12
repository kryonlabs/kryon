#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
set -- --bind pointer_input:PollPointer=hosted_ui_provider:Pointer \
    --bind text:TextHostedHost=hosted_ui_provider:Text \
    --bind image:ImageHostedHost=hosted_ui_provider:Image \
    --bind button:ButtonHostedHost=hosted_ui_provider:Button \
    --bind textarea:TextAreaHostedHost=hosted_ui_provider:TextArea \
    --bind scroll:ScrollHostedHost=hosted_ui_provider:Scroll \
    --bind tree:EndHostedHost=hosted_ui_provider:End \
    --bind checkbox:CheckboxHostedHost=hosted_ui_provider:Checkbox \
    --bind toggle:ToggleHostedHost=hosted_ui_provider:Toggle \
    --bind box:BoxHostedHost=hosted_ui_provider:Box \
    --bind textfield:TextFieldHostedHost=hosted_ui_provider:TextField \
    --bind tabs:TabBarHostedHost=hosted_ui_provider:TabBar \
    --bind segmented:SegmentedControlHostedHost=hosted_ui_provider:SegmentedControl \
    --bind fieldset:FieldsetHostedHost=hosted_ui_provider:Fieldset \
    --bind toggle_measure:ToggleSizeHostedHost=hosted_ui_provider:ToggleSize \
    --bind shape:LineHostedHost=hosted_ui_provider:Line \
    --bind shape:CircleHostedHost=hosted_ui_provider:Circle \
    --bind shape:RingHostedHost=hosted_ui_provider:Ring \
    --bind shape:TriangleHostedHost=hosted_ui_provider:Triangle \
    --bind slider:SliderHostedHost=hosted_ui_provider:Slider \
    --bind slider:DiscreteSliderHostedHost=hosted_ui_provider:DiscreteSlider \
    --bind progress:ProgressHostedHost=hosted_ui_provider:Progress \
    --bind overlay:DismissibleOverlayHostedHost=hosted_ui_provider:DismissibleOverlay \
    --bind overlay:PopupHostedHost=hosted_ui_provider:Popup \
    --bind titlebar:TitleBarHostedHost=hosted_ui_provider:TitleBar \
    --bind dropdown:DropdownHostedHost=hosted_ui_provider:Dropdown \
    --bind modal:ActionModalHostedHost=hosted_ui_provider:ActionModal
"$ziran" bundle --root "$repo/tests" $kryon_project_args \
     --define KRYON_HOSTED_UI "$@" \
    --entry hosted_ui:main -o "$work/ui.zx" \
    "$repo/tests/hosted_ui.zi" "$repo/tests/hosted_ui_provider.zi"
test "$("$ziran" run "$work/ui.zx")" = 42
printf '%s\n' 'Hosted portable Text, Image, Button, TextArea, Scroll, SegmentedControl, Fieldset, ToggleSize, shapes, sliders, Progress and TitleBar contracts passed'
for target in c; do
    executable=
    if test "$target" = c; then executable=--exe; fi
    "$ziran" build --target="$target" $executable --root "$repo/tests" \
        $kryon_project_args \
        --define KRYON_HOSTED_UI --entry hosted_ui:main \
        -o "$work/$target" "$repo/tests/hosted_ui.zi" "$repo/tests/hosted_ui_provider.zi"
    binary=$work/$target/hosted_ui
    status=0
    "$binary" || status=$?
    test "$status" = 42
done
printf '%s\n' 'Hosted widget contracts also passed native C'

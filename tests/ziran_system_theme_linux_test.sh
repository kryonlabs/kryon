#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/system_theme_linux_behavior.zi
"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
    --module-path "$repo/../ziran/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/theme-test"

# Fixture themes and configuration; HOME and the XDG paths point at them so the
# real desktop never leaks in.
config=$work/config
data=$work/data
mkdir -p "$config/xfce4/xfconf/xfce-perchannel-xml" "$config/gtk-3.0" \
    "$data/themes/Fake-Dark/gtk-3.0" "$data/themes/Fake-Light/gtk-3.0" "$work/home"
cat > "$data/themes/Fake-Dark/gtk-3.0/gtk.css" <<'CSS'
@define-color theme_fg_color #C3C7D1;
@define-color theme_bg_color #161925;
@define-color theme_selected_bg_color #c50ed2;
CSS
cat > "$data/themes/Fake-Light/gtk-3.0/gtk.css" <<'CSS'
@define-color theme_fg_color #2e3436;
@define-color theme_bg_color #f6f5f4;
CSS
run() {
    env -u DISPLAY -u WAYLAND_DISPLAY -u GTK_THEME HOME="$work/home" \
        XDG_CONFIG_HOME="$config" XDG_DATA_HOME="$data" SCENARIO="$1" \
        "$work/theme-test"
}

cat > "$config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml" <<'XML'
<channel name="xsettings" version="1.0">
  <property name="Net" type="empty">
    <property name="IconThemeName" type="string" value="Icons"/>
    <property name="ThemeName" type="string" value="Fake-Dark"/>
  </property>
</channel>
XML
run xfce-dark

rm "$config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml"
printf '[Settings]\ngtk-theme-name=Fake-Light\n' > "$config/gtk-3.0/settings.ini"
run gtk-light

rm "$config/gtk-3.0/settings.ini"
GTK_THEME=Missing:dark env -u DISPLAY HOME="$work/home" XDG_CONFIG_HOME="$config" \
    XDG_DATA_HOME="$data" SCENARIO=env-dark "$work/theme-test"

run none

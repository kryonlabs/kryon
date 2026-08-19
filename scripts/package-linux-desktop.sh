#!/bin/sh
set -eu

die() {
    printf '%s\n' "package-linux-desktop: $*" >&2
    exit 1
}

[ $# -eq 2 ] || die "usage: package-linux-desktop.sh OUT_DIR BINARY"
out_dir=$1
binary=$2
[ -f "$binary" ] || die "binary not found: $binary"

project_file=${KRYON_PROJECT_FILE:-project.kryon}

project_value() {
    key=$1
    [ -f "$project_file" ] || return 0
    awk -v want="$key" '
    {
        line = $0
        sub(/#.*/, "", line)
        if(line ~ "^[[:space:]]*" want "[[:space:]]+\"") {
            sub("^[[:space:]]*" want "[[:space:]]+\"", "", line)
            sub("\"[[:space:]]*$", "", line)
            print line
            exit
        }
    }' "$project_file"
}

first_value() {
    for key in "$@"; do
        value=$(project_value "$key" || true)
        if [ -n "$value" ]; then
            printf '%s\n' "$value"
            return 0
        fi
    done
    return 0
}

xml_escape() {
    sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' \
        -e 's/"/\&quot;/g' -e "s/'/\&apos;/g"
}

desktop_escape() {
    sed -e 's/\\/\\\\/g' -e 's/;/\\;/g'
}

find_icon() {
    for candidate in \
        "${APP_ICON:-}" \
        "$(first_value icon app_icon linux_icon || true)" \
        "icons/${app_name}.png" \
        "assets/${app_name}.png" \
        "assets/icon.png" \
        "icon.png"; do
        [ -n "$candidate" ] || continue
        [ -f "$candidate" ] && { printf '%s\n' "$candidate"; return 0; }
    done
    return 0
}

app_name=${APP_NAME:-$(first_value name app_name || true)}
[ -n "$app_name" ] || app_name=app
app_id=${APP_ID:-$(first_value app_id id linux_app_id || true)}
[ -n "$app_id" ] || app_id=$app_name
app_title=${APP_TITLE:-$(first_value title display_name name || true)}
[ -n "$app_title" ] || app_title=$app_name
app_summary=${APP_SUMMARY:-$(first_value summary comment description || true)}
[ -n "$app_summary" ] || app_summary="$app_title"
app_version=${APP_VERSION:-$(first_value version || true)}
[ -n "$app_version" ] || app_version=0.0.0
app_license=${APP_LICENSE:-$(first_value license || true)}
[ -n "$app_license" ] || app_license=LicenseRef-proprietary
app_website=${APP_WEBSITE:-$(first_value website homepage url || true)}
app_categories=${APP_CATEGORIES:-$(first_value categories linux_categories || true)}
[ -n "$app_categories" ] || app_categories=Utility
app_mime_types=${APP_MIME_TYPES:-$(first_value mime_types mime_type linux_mime_types || true)}
app_url_schemes=${APP_URL_SCHEMES:-$(first_value url_schemes url_scheme linux_url_schemes || true)}
app_wm_class=${APP_WM_CLASS:-$(first_value wm_class startup_wm_class || true)}
[ -n "$app_wm_class" ] || app_wm_class=$app_id
icon_name=${APP_ICON_NAME:-$(first_value icon_name linux_icon_name || true)}
[ -n "$icon_name" ] || icon_name=$app_id

case "$app_categories" in
    *';') ;;
    *) app_categories="${app_categories};" ;;
esac

desktop_mimes=
for mime in $app_mime_types; do
    desktop_mimes="${desktop_mimes}${mime};"
done
for scheme in $app_url_schemes; do
    desktop_mimes="${desktop_mimes}x-scheme-handler/${scheme};"
done

rm -rf "$out_dir"
mkdir -p "$out_dir/usr/bin" \
    "$out_dir/usr/share/applications" \
    "$out_dir/usr/share/metainfo"
cp "$binary" "$out_dir/usr/bin/$app_name"
chmod 755 "$out_dir/usr/bin/$app_name"

desktop_file="$out_dir/usr/share/applications/$app_id.desktop"
{
    printf '%s\n' '[Desktop Entry]'
    printf '%s\n' 'Type=Application'
    printf 'Name=%s\n' "$(printf '%s' "$app_title" | desktop_escape)"
    printf 'Comment=%s\n' "$(printf '%s' "$app_summary" | desktop_escape)"
    printf 'Exec=%s %%U\n' "$app_name"
    printf 'Icon=%s\n' "$icon_name"
    printf '%s\n' 'Terminal=false'
    printf 'Categories=%s\n' "$app_categories"
    printf 'StartupWMClass=%s\n' "$app_wm_class"
    if [ -n "$desktop_mimes" ]; then
        printf 'MimeType=%s\n' "$desktop_mimes"
    fi
} > "$desktop_file"

icon_file=$(find_icon || true)
if [ -n "$icon_file" ]; then
    mkdir -p "$out_dir/usr/share/icons/hicolor/256x256/apps"
    cp "$icon_file" "$out_dir/usr/share/icons/hicolor/256x256/apps/$icon_name.png"
fi

meta_file="$out_dir/usr/share/metainfo/$app_id.metainfo.xml"
{
    printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>'
    printf '%s\n' '<component type="desktop-application">'
    printf '  <id>%s</id>\n' "$(printf '%s' "$app_id" | xml_escape)"
    printf '  <name>%s</name>\n' "$(printf '%s' "$app_title" | xml_escape)"
    printf '  <summary>%s</summary>\n' "$(printf '%s' "$app_summary" | xml_escape)"
    printf '  <metadata_license>CC0-1.0</metadata_license>\n'
    printf '  <project_license>%s</project_license>\n' "$(printf '%s' "$app_license" | xml_escape)"
    if [ -n "$app_website" ]; then
        printf '  <url type="homepage">%s</url>\n' "$(printf '%s' "$app_website" | xml_escape)"
    fi
    printf '  <launchable type="desktop-id">%s.desktop</launchable>\n' "$(printf '%s' "$app_id" | xml_escape)"
    printf '  <releases><release version="%s"/></releases>\n' "$(printf '%s' "$app_version" | xml_escape)"
    printf '%s\n' '</component>'
} > "$meta_file"

if [ -n "$app_mime_types" ]; then
    mkdir -p "$out_dir/usr/share/mime/packages"
    mime_file="$out_dir/usr/share/mime/packages/$app_id.xml"
    {
        printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>'
        printf '%s\n' '<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">'
        for mime in $app_mime_types; do
            printf '  <mime-type type="%s"><comment>%s document</comment></mime-type>\n' \
                "$(printf '%s' "$mime" | xml_escape)" \
                "$(printf '%s' "$app_title" | xml_escape)"
        done
        printf '%s\n' '</mime-info>'
    } > "$mime_file"
fi

archive="$(dirname "$out_dir")/$app_name-linux-desktop.tar.gz"
rm -f "$archive"
tar -C "$out_dir" -czf "$archive" .
printf '%s\n' "Created $out_dir"
printf '%s\n' "Created $archive"

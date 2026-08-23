#!/bin/sh
set -eu

usage()
{
    cat >&2 <<'EOF'
Usage:
  scripts/sync-icons.sh [--group NAME] [--flat] DEST_DIR [ICON ...]

Examples:
  vendor/kryon/scripts/sync-icons.sh --group platforms --flat web-assets/icons \
      appimage debian flatpak snap freebsd
  vendor/kryon/scripts/sync-icons.sh web-assets/kryon-icons platforms/appimage pfp/bambus

Copies checked-in PNGs from Kryon's icons/ source tree. With --group platforms,
ICON names are resolved below icons/platforms. With --flat, copied files are
written as DEST_DIR/<basename>.png instead of preserving the group path.
EOF
    exit 2
}

group=
flat=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --group)
            [ "$#" -ge 2 ] || usage
            group=$2
            shift 2
            ;;
        --flat)
            flat=1
            shift
            ;;
        --help|-h)
            usage
            ;;
        --)
            shift
            break
            ;;
        -*)
            usage
            ;;
        *)
            break
            ;;
    esac
done

[ "$#" -ge 1 ] || usage
dest=$1
shift

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
icon_root=$root_dir/icons

if [ -n "$group" ]; then
    icon_root=$icon_root/$group
fi

[ -d "$icon_root" ] || {
    printf 'sync-icons: icon source not found: %s\n' "$icon_root" >&2
    exit 1
}

mkdir -p "$dest"

copy_icon()
{
    rel=$1
    case "$rel" in
        *.png) ;;
        *) rel=$rel.png ;;
    esac

    src=$icon_root/$rel
    [ -f "$src" ] || {
        printf 'sync-icons: icon not found: %s\n' "$src" >&2
        exit 1
    }

    if [ "$flat" -eq 1 ]; then
        out=$dest/$(basename "$src")
    elif [ -n "$group" ]; then
        out=$dest/$group/$rel
    else
        out=$dest/$rel
    fi

    mkdir -p "$(dirname -- "$out")"
    cp "$src" "$out"
    printf '%s\n' "$out"
}

if [ "$#" -eq 0 ]; then
    find "$icon_root" -path '*/review/*' -prune -o -type f -name '*.png' -print |
        LC_ALL=C sort |
        while IFS= read -r src; do
            rel=${src#"$icon_root"/}
            copy_icon "$rel" >/dev/null
        done
else
    for icon in "$@"; do
        copy_icon "$icon" >/dev/null
    done
fi

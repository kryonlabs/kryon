#!/bin/sh
set -eu

usage()
{
    cat >&2 <<'EOF'
Usage:
  scripts/sync-icons.sh DEST_DIR [SHEET ...]

Examples:
  vendor/kryon/scripts/sync-icons.sh web-assets/icons
  vendor/kryon/scripts/sync-icons.sh web-assets/icons ui platforms payments

Copies finished PNG spritesheets and their JSON manifests from Kryon's icons/
directory. With no SHEET arguments, all seven sheets are copied.
EOF
    exit 2
}

case "${1:-}" in
    ''|--help|-h) usage ;;
esac

dest=$1
shift

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
icon_root=$root_dir/icons

mkdir -p "$dest"

copy_sheet()
{
    sheet=$1
    case "$sheet" in
        *[!A-Za-z0-9_-]*|'')
            printf 'sync-icons: invalid sheet name: %s\n' "$sheet" >&2
            exit 1
            ;;
    esac

    for extension in png json; do
        source=$icon_root/$sheet.$extension
        [ -f "$source" ] || {
            printf 'sync-icons: sheet not found: %s\n' "$source" >&2
            exit 1
        }
        cp "$source" "$dest/$sheet.$extension"
    done
}

if [ "$#" -eq 0 ]; then
    set -- ui pfp platforms payments language tiles logos
fi

for sheet in "$@"; do
    copy_sheet "$sheet"
done

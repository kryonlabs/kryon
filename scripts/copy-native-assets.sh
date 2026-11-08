#!/bin/sh
# Build/install tooling: publish a copied snapshot, preserving binary bytes.
set -eu
source_dir=$1
destination=$2
prefix=$3
sh "$(dirname "$0")/make-install-dirs.sh" "$(dirname "$destination")"
if [ -z "$source_dir" ]; then
    # Only this product's managed snapshot is removed when assets are disabled.
    rm -rf "$destination"
    exit 0
fi
staging=$destination.new
previous=$destination.old
rm -rf "$staging"
mkdir -p "$staging"
cp -R "$source_dir/." "$staging/"
printf '%s' "$prefix" > "$staging/.kryon-prefix"
# Assets are data, including the prefix marker. Source modes and caller umask
# must not make a shared/system installation unreadable to its users.
find "$staging" -type d -exec chmod 755 {} +
find "$staging" -type f -exec chmod 644 {} +
rm -rf "$previous"
if [ -e "$destination" ]; then mv "$destination" "$previous"; fi
if ! mv "$staging" "$destination"; then
    if [ -e "$previous" ]; then mv "$previous" "$destination"; fi
    exit 1
fi
rm -rf "$previous"

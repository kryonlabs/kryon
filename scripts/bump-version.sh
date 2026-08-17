#!/bin/sh
set -eu

header=${KRYON_VERSION_FILE:-include/kryon_version.h}
current=$(sed -n 's/^#define KRYON_VERSION_STRING "v\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)".*/\1/p' "$header")
test -n "$current" || { echo "cannot read semantic version from $header" >&2; exit 1; }

IFS=. read -r major minor patch <<EOF
$current
EOF

case ${1:-patch} in
    patch) patch=$((patch + 1)) ;;
    minor) minor=$((minor + 1)); patch=0 ;;
    major) major=$((major + 1)); minor=0; patch=0 ;;
    v[0-9]*.[0-9]*.[0-9]*)
        explicit=${1#v}
        IFS=. read -r major minor patch <<EOF
$explicit
EOF
        ;;
    *) echo "usage: $0 [patch|minor|major|vMAJOR.MINOR.PATCH]" >&2; exit 2 ;;
esac

case "$major.$minor.$patch" in
    *[!0-9.]*|.*|*.|*..*) echo "invalid version" >&2; exit 2 ;;
esac

version="v$major.$minor.$patch"
tmp="$header.tmp"
sed \
    -e "s/^#define KRYON_VERSION_MAJOR .*/#define KRYON_VERSION_MAJOR $major/" \
    -e "s/^#define KRYON_VERSION_MINOR .*/#define KRYON_VERSION_MINOR $minor/" \
    -e "s/^#define KRYON_VERSION_PATCH .*/#define KRYON_VERSION_PATCH $patch/" \
    -e "s/^#define KRYON_VERSION_STRING .*/#define KRYON_VERSION_STRING \"$version\"/" \
    "$header" > "$tmp"
mv "$tmp" "$header"
printf '%s\n' "$version"

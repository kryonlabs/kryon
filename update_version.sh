#!/bin/sh
set -eu
cd "$(dirname "$0")"
version=$(sed -n 's/^## \[\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\].*/\1/p' CHANGELOG.md | head -n 1)
test -n "$version"
printf '%s\n' "$version" > VERSION

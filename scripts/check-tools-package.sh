#!/bin/sh
set -eu

archive=${1:?usage: check-tools-package.sh ARCHIVE}
test -f "$archive"
listing=$(mktemp "${TMPDIR:-/tmp}/kryon-tools-list.XXXXXX")
trap 'rm -f "$listing"' EXIT HUP INT TERM
tar -tzf "$archive" > "$listing"
root=$(sed -n '1s,/.*,,p' "$listing")
test -n "$root"

for file in VERSION manifest.json bin/k2c bin/k2g bin/k2js bin/k2ir bin/k2b bin/kt \
            bin/kryon bin/kry-fmt.sh bin/kry-locale-check.sh \
            bin/kryon-preview bin/krb-run bin/krb-sdl \
            web/kryon-runtime.js web/kryon-runtime.d.ts web/kryon-runtime.ts; do
    grep -qx "$root/$file" "$listing"
done

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/kryon-tools-check.XXXXXX")
trap 'rm -f "$listing"; rm -rf "$tmpdir"' EXIT HUP INT TERM
tar -xzf "$archive" -C "$tmpdir"
for tool in "$tmpdir/$root"/bin/*; do
    test -x "$tool"
done
printf 'tools package ok: %s\n' "$archive"

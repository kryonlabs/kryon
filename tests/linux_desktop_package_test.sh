#!/bin/sh
set -eu

root=$(cd "${1:-.}" && pwd)
tmp=${TMPDIR:-/tmp}/kryon-linux-desktop-package-test
rm -rf "$tmp"
mkdir -p "$tmp"
cd "$tmp"

mkdir -p icons
printf '%s\n' '#!/bin/sh' 'exit 0' > demo-bin
chmod 755 demo-bin
printf 'png' > icons/demo.png
cat > project.kryon <<'PROJECT'
name "demo"
app_id "xyz.waozi.demo"
title "Demo App"
summary "Desktop package test"
version "1.2.3"
license "MIT"
website "https://example.test/demo"
categories "Utility;Development;"
mime_types "text/x-demo application/x-demo"
url_schemes "demo"
PROJECT

APP_NAME=demo sh "$root/scripts/package-linux-desktop.sh" "$tmp/out" "$tmp/demo-bin" >/dev/null

test -x "$tmp/out/usr/bin/demo"
test -f "$tmp/out/usr/share/applications/xyz.waozi.demo.desktop"
test -f "$tmp/out/usr/share/metainfo/xyz.waozi.demo.metainfo.xml"
test -f "$tmp/out/usr/share/icons/hicolor/256x256/apps/xyz.waozi.demo.png"
test -f "$tmp/out/usr/share/mime/packages/xyz.waozi.demo.xml"
grep -Fq 'Name=Demo App' "$tmp/out/usr/share/applications/xyz.waozi.demo.desktop"
grep -Fq 'MimeType=text/x-demo;application/x-demo;x-scheme-handler/demo;' "$tmp/out/usr/share/applications/xyz.waozi.demo.desktop"
grep -Fq '<launchable type="desktop-id">xyz.waozi.demo.desktop</launchable>' "$tmp/out/usr/share/metainfo/xyz.waozi.demo.metainfo.xml"
test -f "$tmp/demo-linux-desktop.tar.gz"

printf '%s\n' "linux desktop package tests passed"

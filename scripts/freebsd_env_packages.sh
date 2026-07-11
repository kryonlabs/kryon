#!/bin/sh
set -eu

printf "UNAME: %s\n\n" "$(uname -a)"

printf "ENVIRONMENT VARIABLES:\n"
env

printf "\nINSTALLED PACKAGES (pkg info):\n"
if command -v pkg >/dev/null 2>&1; then
    pkg info
else
    echo "pkg not found on PATH"
fi

REQUIRED_PKGS="sdl2 libdrm gbm egl glesv2 libcurl"
printf "\nPKG-CONFIG STATUS FOR REQUIRED PACKAGES:\n"
for p in $REQUIRED_PKGS; do
    printf "\n== %s ==\n" "$p"
    if command -v pkg-config >/dev/null 2>&1; then
        pkg-config --modversion $p 2>/dev/null || echo "pkg-config metadata not found for $p"
    else
        echo "pkg-config not installed"
    fi
done

printf "\nFREEBSD pkg ABI: \n"
if command -v pkg >/dev/null 2>&1; then
    pkg config ABI 2>/dev/null || true
fi

printf "\nDone.\n"

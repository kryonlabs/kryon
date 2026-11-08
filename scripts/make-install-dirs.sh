#!/bin/sh
# Create public product directories without changing existing prefix boundaries.
set -eu
make_install_dir() (
    if [ -d "$1" ]; then exit 0; fi
    make_install_dir "$(dirname "$1")"
    # -m supplies the mode explicitly, including under a private caller umask.
    if ! mkdir -m 755 "$1"; then test -d "$1"; fi
)
for directory do make_install_dir "$directory"; done

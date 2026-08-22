#!/bin/sh
set -eu

root=${1:-.}

cd "$root"

if [ "${CI:-}" = "true" ]; then
    python3 scripts/feature-matrix-html.py --check
else
    python3 scripts/feature-matrix-html.py
    python3 scripts/feature-matrix-html.py --check
fi

echo "feature matrix docs ok"

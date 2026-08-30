#!/bin/sh
set -eu

root=${1:-.}
build=${2:-build/linux-x86_64}

cd "$root"
python3 scripts/conformance-matrix.py --check
python3 scripts/conformance-matrix.py --verify-widget-coverage
python3 scripts/conformance-matrix.py --verify-pipelines --build-dir "$build"
sh tests/k2js_runtime_snapshot_test.sh . "$build" "$build/bin/k2js"
python3 scripts/conformance-matrix.py --verify-krb-visuals --build-dir "$build"
python3 scripts/conformance-matrix.py --verify-krb-web-visuals --build-dir "$build"

echo "conformance matrix ok"

#!/bin/sh
set -eu

root=${1:-.}

cd "$root"
python3 scripts/conformance-matrix.py --check
python3 scripts/conformance-matrix.py --verify-widget-coverage
python3 scripts/conformance-matrix.py --verify-pipelines
python3 scripts/conformance-matrix.py --verify-krb-visuals
python3 scripts/conformance-matrix.py --verify-krb-web-visuals

echo "conformance matrix ok"

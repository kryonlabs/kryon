#!/bin/sh
set -eu

root=${1:-.}

cd "$root"
python3 scripts/conformance-matrix.py --check
python3 scripts/conformance-matrix.py --verify-pipelines

echo "conformance matrix ok"

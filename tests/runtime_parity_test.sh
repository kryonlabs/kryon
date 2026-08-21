#!/bin/sh
set -eu

root=${1:-.}

cd "$root"
export GOCACHE="${GOCACHE:-${TMPDIR:-/tmp}/kryon-runtime-parity-go-cache}"

if [ "${CI:-}" = "true" ]; then
    go run tools/runtime_parity_doc.go
else
    go run tools/runtime_parity_doc.go -write
    go run tools/runtime_parity_doc.go
fi
echo "runtime parity ok"

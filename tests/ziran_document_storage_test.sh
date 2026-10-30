#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ZIRAN_ROOT="$ziran_root" ZIRAN_BIN="$ziran" exec python3 "$repo/tests/document_storage_test.py"

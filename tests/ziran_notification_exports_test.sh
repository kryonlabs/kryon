#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
export ZIRAN_ROOT="$ziran_root"
python3 "$repo/tests/android_notification_exports_test.py"

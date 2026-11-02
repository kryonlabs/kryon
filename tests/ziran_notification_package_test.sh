#!/bin/sh
# Exercise the real Android scaffold through dependency exports, without a
# backend module-path that would conceal a missing public package export.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/work/notification-package-test"
git -C "$repo" check-ignore -q "$work"
mkdir -p "$work/src"
python3 - "$repo" "$work" "$ziran_root" <<'PY'
import json
from pathlib import Path
import sys
repo, work, toolchain = map(Path, sys.argv[1:])
source = (repo / 'templates/droid/src/android_glue.zi').read_text()
(work / 'src/android_glue.zi').write_text(source.replace(
    '__ACTIVITY_SLASH__', 'org/kryonlabs/notifications/test/MainActivity'))
(work / 'ziran.toml').write_text('''[package]
name = "notification_consumer"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
''')
(work / 'ziran.local.toml').write_text('[overrides]\n' + ''.join(
    f'{name} = {json.dumps(str(path))}\n'
    for name, path in (('kryon', repo), ('ziran', toolchain))))
PY
cd "$work"
"$ziran" lock --offline
"$ziran" check --project --offline --define ANDROID_BUILD --define __aarch64__ src/android_glue.zi
echo 'Android notification scaffold package exports local test passed'

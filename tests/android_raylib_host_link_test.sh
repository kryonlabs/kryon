#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER GDK_DISPLAY
export HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0
output="$repo/build/scratch/android-raylib-host-link"
mkdir -p "$output"
case $(uname -m) in
    x86_64|amd64) architecture=__x86_64__ ;;
    aarch64|arm64) architecture=__aarch64__ ;;
    armv7*|armv8l) architecture=__arm__ ;;
    i?86) architecture=__i386__ ;;
    *) echo 'Unsupported host architecture for Android ABI link test' >&2; exit 1 ;;
esac
"$ziran" build --project --module-path "$repo/tests" --root "$repo/tests" \
    --target=c --no-main --define ANDROID_BUILD --define KRYON_ANDROID_HOST \
    --define KRYON_RAYLIB_HOST --define "$architecture" \
    -o "$output/c" "$repo/tests/android_raylib_host_link.zi"
# Link the real generated providers together. Relocatable linking checks the
# public ABI without requiring Android OS calls or opening a window.
python3 - "$output" "$ziran_root" <<'PY'
from pathlib import Path
import os
import shlex
import subprocess
import sys

output, ziran_root = map(Path, sys.argv[1:])
generated = output / 'c'
compiler = shlex.split(os.environ.get('CC', 'cc'))
includes = ['-I' + str(ziran_root / 'include')]
for folder in sorted({path.parent for path in generated.rglob('*.h')}):
    includes.append('-iquote' + str(folder))
objects = []
for module in ('android_touch', 'android_clipboard', 'raylib_runtime'):
    sources = [path for path in generated.rglob('*.c')
               if path.stem == module or path.stem.endswith('_' + module)]
    if len(sources) != 1:
        raise SystemExit(f'Expected one generated {module} provider, got {len(sources)}')
    obj = output / (module + '.o')
    subprocess.run([*compiler, '-std=c11', '-O1', *includes, '-c', str(sources[0]),
                    '-o', str(obj)], check=True)
    objects.append(str(obj))
subprocess.run([*compiler, '-r', *objects, '-o', str(output / 'host.o')], check=True)
expected = {'PollPointer', 'PollSecondaryPointer', 'SystemClipboardOpenHost',
            'SystemClipboardByteHost', 'SystemClipboardWriteHost'}
found = set()
with subprocess.Popen(['nm', '--defined-only', '--extern-only', '--format=posix',
                       str(output / 'host.o')], stdout=subprocess.PIPE, text=True) as process:
    for line in process.stdout:
        symbol = line.split()[0]
        if symbol in expected:
            found.add(symbol)
    if process.wait() != 0:
        raise SystemExit('Could not inspect the linked Android host ABI')
if found != expected:
    raise SystemExit(f'Missing selected host exports: {sorted(expected - found)}')
print('Android raylib host: touch and clipboard providers link with one selected ABI')
PY

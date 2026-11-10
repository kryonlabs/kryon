#!/usr/bin/env python3
"""Exercise the desktop and Android-native surface boundary without a display."""
import os
from pathlib import Path
import shutil
import subprocess

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

environment = dict(os.environ)
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'):
    environment.pop(key, None)

def run(command):
    subprocess.run(list(map(str, command)), cwd=ROOT, env=environment, check=True)

work = ROOT / 'build/scratch/android-surface-platform'
for platform in ('desktop', 'android'):
    generated = work / platform
    if generated.exists():
        shutil.rmtree(generated)
    defines = ['--define', 'ANDROID_BUILD'] if platform == 'android' else []
    run([ZIRAN, 'build', '--project', '--target=c', *defines,
         '--entry', 'android_surface_platform_test:main', '--root', ROOT / 'tests',
         '-o', generated, ROOT / 'tests/android_surface_platform_test.zi'])
    sources = sorted(generated.rglob('*.c'))
    host_stubs = []
    if platform == 'android':
        stubs = generated / 'host_stubs.c'
        stubs.write_text('''#include <stdarg.h>
int ALooper_pollAll(int timeout, int *fd, int *events, void **data)
{ (void)timeout; (void)fd; (void)events; (void)data; return 0; }
void ANativeWindow_acquire(void *window) { (void)window; }
void ANativeWindow_release(void *window) { (void)window; }
int ANativeWindow_getFormat(void *window) { (void)window; return 1; }
int ANativeWindow_setBuffersGeometry(void *window, int width, int height, int format)
{ (void)window; (void)width; (void)height; (void)format; return 1; }
int ANativeWindow_lock(void *window, void *bounds) { (void)window; (void)bounds; return 1; }
int ANativeWindow_unlockAndPost(void *window) { (void)window; return 1; }
void ANativeActivity_finish(void *activity) { (void)activity; }
''')
        host_stubs.append(stubs)
    includes = [ZIRAN_ROOT / 'include', generated,
                *sorted({p.parent for p in generated.rglob('*.h')})]
    run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-ffunction-sections',
         '-fdata-sections', '-Wl,--gc-sections',
         *['-I' + str(p) for p in includes], *sources, *host_stubs,
         '-lm', '-o', generated / 'test'])
    run([generated / 'test'])
    print(platform + ' surface boundary behavior passed', flush=True)

# Link the same Android branch with the NDK for every supported Android ABI.
# Window functions are test-host definitions; no device or display is used.
host_stubs = []
sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'Android/Sdk')))
ndks = sorted((sdk / 'ndk').glob('*/toolchains/llvm/prebuilt/linux-x86_64'))
if not ndks:
    raise RuntimeError('Android NDK required for surface platform link tests')
clang = ndks[-1] / 'bin/clang'
for target in ('aarch64-linux-android24', 'armv7a-linux-androideabi24',
               'i686-linux-android24', 'x86_64-linux-android24'):
    run([clang, '--target=' + target, '-std=c11', '-O1', '-ffunction-sections',
         '-fdata-sections', '-Wl,--gc-sections',
         *['-I' + str(p) for p in includes], *sources,
         '-landroid', '-llog', '-lm', '-o', generated / target])
    print(target + ' surface boundary linked', flush=True)

"""Resolve the pinned SDL3 SDK and run the opt-in backend's local tests.

Run through build-queue, normally with make sdl3-test. The consumer fixture
contains package metadata and a generated import smoke test; library sources
stay in the canonical checkouts.
"""
import fcntl
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys
import tomllib

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT


def main():
    if sys.platform != 'linux' or platform.machine() != 'x86_64':
        raise ValueError('native-sdl3 currently supports Linux x86_64 GNU only')
    work = ROOT / 'work/sdl3-integration'
    subprocess.run(['git', 'check-ignore', '-q', str(work)], cwd=ROOT, check=True)
    work.mkdir(parents=True, exist_ok=True)
    with (work / 'lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        env = os.environ.copy()
        for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY',
                    'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
            env.pop(key, None)
        env.update(TMPDIR=str(work), YUE_DESKTOP_RECOVERY='0', MAKEFLAGS='-j1',
                   CMAKE_BUILD_PARALLEL_LEVEL='1', ZIRAN_PAR_THREADS='1',
                   ZIRAN_BIN=str(ZIRAN), ZIRAN_ROOT=str(ZIRAN_ROOT))
        manifest = tomllib.loads((ROOT / 'ziran.toml').read_text())
        toolchain = manifest['toolchain']
        # A self artifact is requested as an ordinary dependency. Overrides
        # select the exact current checkouts without copying any source tree.
        (work / 'ziran.toml').write_text(
            '[package]\nname = "sdl3_integration"\n'
            'entry = "src/imports.zi"\nmodule_roots = ["src"]\n[toolchain]\n'
            f'git = {json.dumps(toolchain["git"])}\n'
            f'ref = {json.dumps(toolchain["ref"])}\n'
            '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n')
        (work / 'ziran.local.toml').write_text(
            '[overrides]\n' + ''.join(f'{name} = {json.dumps(str(path))}\n'
                                     for name, path in (('kryon', ROOT), ('ziran', ZIRAN_ROOT))))
        # Exercise real dependency exports from a consumer-owned entry. This
        # fixture is generated test input, never a copy of Kryon's sources.
        probe = work / 'src/imports.zi'
        probe.parent.mkdir(exist_ok=True)
        probe.write_text('''Presenter :: #import "kryon/pixmap_sdl3";
Sink :: #import "kryon/pixmap_sdl3_gpu";
main :: () -> s32 {
    if Presenter.CreatePixmapSdl3Renderer(null) != null ||
        Presenter.PixmapSdl3DescribeRenderer(null).valid || Sink.PixmapSdl3Open(null) { return 1 }
    if Sink.PixmapSdl3Draw() { return 2 }
    Sink.PixmapSdl3Close()
    return 0
}
''')
        subprocess.run([str(ZIRAN), 'lock'], cwd=work, env=env, check=True)
        sys.path.insert(0, str(ZIRAN_ROOT / 'scripts'))
        from ziran_artifacts import request
        artifact = request('kryon:native-sdl3', 'linux-x86_64-gnu',
                           project=work, ziran=ZIRAN, env=env)
        env['KRYON_SDL3_ARTIFACT'] = str(artifact.output('lib/libSDL3.a').parent.parent)
        print('Verified native-sdl3 SDK:', env['KRYON_SDL3_ARTIFACT'], flush=True)
        output = ROOT / 'build/scratch/sdl3-package-import'
        output.mkdir(parents=True, exist_ok=True)
        link_env = dict(env, LDLIBS=shlex.join(artifact.link_flags(env=env)))
        subprocess.run([str(ZIRAN), 'build', '--project', '--target=c',
                        '--entry', 'imports:main', '--exe', '-o', str(output), str(probe)],
                       cwd=work, env=link_env, check=True)
        subprocess.run([str(output / 'imports')], cwd=work, env=env, check=True)
        print('SDL3 package-qualified exports compile, link and run', flush=True)
        for test in ('backend_comparison_scene_test.sh', 'sdl3_geometry_sink_test.sh',
                     'sdl3_renderer_host_test.sh'):
            subprocess.run(['sh', str(ROOT / 'tests' / test)], cwd=ROOT, env=env, check=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit('SDL3 integration local tests failed: ' + str(error))

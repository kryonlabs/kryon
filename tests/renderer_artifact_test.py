"""Real package-provider outputs, native renderer link/run, optional clean reuse."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/renderer-artifacts'
TOOL = '5461202417d48e2b31122354419179b262d7530c'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clean', action='store_true', help='use published pinned provider; require identical descriptors')
    parser.add_argument('--revision', default='master')
    parser.add_argument('--cross', action='store_true')
    parser.add_argument('--cross-only', action='store_true', help='exercise installed cross toolchains only')
    args = parser.parse_args()
    subprocess.run(['git', 'check-ignore', str(WORK / 'sentinel')], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    WORK.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
        env.pop(key, None)
    env.update(TMPDIR=str(WORK), YUE_DESKTOP_RECOVERY='0', MAKEFLAGS='-j1',
               CMAKE_BUILD_PARALLEL_LEVEL='1', EMCC_CORES='1', ZIRAN_PAR_THREADS='1')
    ziran = os.environ.get('ZIRAN_BIN', 'ziran')
    log = (WORK / 'test.log').open('w')

    def call(command, cwd, extra=None, success=True):
        result = subprocess.run(list(map(str, command)), cwd=cwd, env={**env, **(extra or {})},
                                stdout=subprocess.PIPE, stderr=log, text=True)
        log.flush()
        assert (result.returncode == 0) == success, (command, result.returncode, str(WORK / 'test.log'))
        return result.stdout.strip()

    apps = []
    for name in ('one', 'two'):
        app = WORK / ('clean-' + name if args.clean else name)
        app.mkdir(exist_ok=True)
        (app / 'src').mkdir(exist_ok=True)
        (app / 'ziran.toml').write_text(f'''[package]
name = "{name}"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "{TOOL}"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "{args.revision}"
''')
        local = app / 'ziran.local.toml'
        if args.clean:
            local.unlink(missing_ok=True)
        else:
            local.write_text(f'[overrides]\nkryon = "{ROOT}"\n')
        call([ziran, 'lock'], app)
        apps.append(app)

    def artifact(recipe, target, extra=None, success=True, app=apps[0]):
        output = call([ziran, 'pkg', 'artifact', 'kryon:' + recipe, '--platform', target, *(['--locked'] if args.clean else [])], app, extra, success)
        if not success:
            return
        descriptor = Path(output)
        records = dict(line.split('\t', 1) for line in descriptor.read_text().splitlines())
        assert records['platform'] == target and records['artifact'] == recipe
        for path, digest in records.items():
            if path in ('schema', 'package', 'artifact', 'platform', 'key'):
                continue
            file = descriptor.parent / path
            assert file.resolve().is_relative_to(descriptor.parent.resolve())
            assert hashlib.file_digest(file.open('rb'), 'sha256').hexdigest() == digest
        meta = json.loads((descriptor.parent / 'renderer.json').read_text())
        assert meta['platform'] == target
        assert (descriptor.parent / meta['resources']['font']).stat().st_size > 100000
        print('PASS descriptor/hash/resources', recipe, target, flush=True)
        return descriptor, meta

    if not args.cross_only:
        first, meta = artifact('renderer-raylib', 'linux-x86_64-gnu')
        second, _ = artifact('renderer-raylib', 'linux-x86_64-gnu', app=apps[1])
        assert (first == second) == args.clean, 'clean consumers must share a descriptor; dirty builds must isolate'
        artifact('renderer-desktop', 'linux-x86_64-gnu')
    web, webmeta = artifact('renderer-web', 'emscripten-wasm32-emscripten')
    manifest = apps[0] / 'ziran.toml'
    original = manifest.read_text()
    manifest.write_text(original + '\n[tool.kryon.renderer]\nandroid_api = "22"\n')
    varied, _ = artifact('renderer-web', 'emscripten-wasm32-emscripten')
    assert varied != web, 'resolved option variation must change the descriptor'
    manifest.write_text(original)
    artifact('renderer-raylib', 'emscripten-wasm32-emscripten', success=False)
    artifact('renderer-web', 'linux-x86_64-gnu', success=False)
    app = apps[0]
    toolchain = Path(call([ziran, 'pkg', 'path', 'ziran', *(['--locked'] if args.clean else [])], app))
    if not args.cross_only:
        # Compile the actual maintained Kryon renderer through ordinary imports.
        copy = app / 'src/probe.zi'
        shutil.copyfile(ROOT / 'tests/renderer_artifact_probe.zi', copy)
        generated = app / 'generated'
        call([ziran, 'build', '--target=c', '-o', generated, copy], app)
        binary = app / 'probe'
        sources = sorted(generated.rglob('*.c'))
        includes = sorted({p.parent for p in sources})
        call(['cc', '-O2', '-ffunction-sections', '-fdata-sections', '-I' + str(toolchain / 'include'),
              *['-iquote' + str(p) for p in includes], *meta['compile_flags'], *sources,
              *[first.parent / p for p in meta['archives']], '-Wl,--gc-sections', *meta['link_flags'], '-o', binary], app)
        call(['xvfb-run', '-a', binary], app, {'KRYON_FONT_PATH': str(first.parent / meta['resources']['font'])})
        print('PASS actual Kryon Raylib renderer compiled/linked/ran on private Xvfb', flush=True)
    if args.cross or args.cross_only:
        websource = app / 'src/renderer_web_probe.zi'
        shutil.copyfile(ROOT / 'tests/renderer_web_probe.zi', websource)
        webcode = app / 'web-code'
        call([ziran, 'build', '--target=c', '--define', 'PLATFORM_WEB', '-o', webcode, websource], app)
        emcc = shutil.which('emcc') or str(Path.home() / 'emsdk/upstream/emscripten/emcc')
        preload = webmeta['preload_files'][0]
        call([emcc, '-O2', '-I' + str(toolchain / 'include'),
              *['-iquote' + str(p) for p in sorted({p.parent for p in webcode.rglob('*.c')})],
              *sorted(webcode.rglob('*.c')), '--js-library', web.parent / webmeta['js_libraries'][0],
              '--preload-file', str(web.parent / preload['path']) + '@' + preload['mount'],
              *webmeta['link_flags'], '-o', app / 'web-probe.js'], app)
        print('PASS maintained Canvas renderer Emscripten compile/link', flush=True)
        cross = app / 'cross'
        foreign = app / 'src/renderer_foreign_probe.zi'
        shutil.copyfile(ROOT / 'tests/renderer_foreign_probe.zi', foreign)
        call([ziran, 'build', '--target=c', '-o', cross, foreign], app)

        def cross_link(descriptor, metadata, compiler, suffix, shared=False):
            call([compiler, '-O2', '-fPIC', '-ffunction-sections', '-fdata-sections',
                  '-I' + str(toolchain / 'include'), '-iquote' + str(cross), *sorted(cross.rglob('*.c')),
                  *[descriptor.parent / p for p in metadata['archives']],
                  '-Wl,--gc-sections', *metadata['link_flags'],
                  *(['-shared', '-Wl,--no-undefined'] if shared else []), '-o', app / ('foreign-probe' + suffix)], app)
            print('PASS real foreign renderer cross compile/link', metadata['platform'], flush=True)

        mingw = shutil.which('x86_64-w64-mingw32-gcc') or str(Path.home() / '.nix-profile/bin/x86_64-w64-mingw32-gcc')
        ar = str(Path(mingw).with_name('x86_64-w64-mingw32-ar'))
        win, wm = artifact('renderer-raylib', 'windows-x86_64-mingw', {'CC': mingw, 'AR': ar})
        cross_link(win, wm, mingw, '.exe')
        ndk = Path.home() / 'Android/Sdk/ndk/28.2.13676358'
        with (app / 'ziran.toml').open('a') as stream:
            stream.write(f'\n[tool.kryon.renderer]\nandroid_ndk = "{ndk}"\nandroid_api = "21"\n')
        for arch, prefix in [('aarch64', 'aarch64-linux-android'), ('armv7', 'armv7a-linux-androideabi')]:
            compiler = ndk / f'toolchains/llvm/prebuilt/linux-x86_64/bin/{prefix}21-clang'
            android, am = artifact('renderer-raylib', f'android-{arch}-bionic', {'CC': str(compiler)})
            cross_link(android, am, compiler, '-' + arch + '.so', shared=True)
    mode = 'cross compile/link only' if args.cross_only else ('clean two-consumer same-descriptor reuse' if args.clean else 'dirty isolation only')
    print('PASS renderer provider suite; ' + mode, flush=True)


if __name__ == '__main__':
    main()

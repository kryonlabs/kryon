"""Build tooling for dependency-owned foreign renderers (no generated Zi graph)."""
import argparse
import json
import os
from pathlib import Path, PurePosixPath
import shlex
import shutil
import subprocess
import sys
import tempfile


SUPPORTED_RENDERERS = {
    'renderer-desktop': ('linux-x86_64-gnu', 'android-aarch64-bionic', 'android-armv7-bionic'),
    'renderer-desktop-windows': ('windows-x86_64-mingw',),
    'renderer-web': ('emscripten-wasm32-emscripten',),
}


def renderer_metadata(artifact):
    """Resolve renderer.json through a verified Ziran Artifact consumer."""
    meta = artifact.metadata('renderer.json')
    if meta['platform'] not in SUPPORTED_RENDERERS.get(meta['artifact'], ()):
        raise ValueError('unsupported renderer artifact/platform')

    def text(value):
        return isinstance(value, str) and bool(value) and all(
            ord(char) >= 32 and ord(char) != 127 for char in value)

    def strings(field, default=None):
        values = meta.get(field, default)
        if not isinstance(values, list) or not all(text(value) for value in values):
            raise ValueError('invalid renderer metadata: ' + field)
        return values

    for field in ('compile_flags', 'link_flags', 'defines'):
        strings(field)
    if 'runtime_archive' in meta:
        meta['runtime_archive'] = str(artifact.output(meta['runtime_archive']))
    meta['shared_libraries'] = [str(artifact.output(name)) for name in strings('shared_libraries', [])]
    meta['archives'] = [str(artifact.output(name)) for name in strings('archives')]
    meta['js_libraries'] = [str(artifact.output(name)) for name in strings('js_libraries', [])]
    directories = [artifact.path(name) for name in strings('include_directories')]
    if not all(path.is_dir() for path in directories):
        raise ValueError('renderer include directory is missing')
    meta['include_directories'] = list(map(str, directories))
    resources = meta.get('resources')
    if (not isinstance(resources, dict) or not {'font', 'font_license'} <= resources.keys()
            or not all(text(key) and text(value) for key, value in resources.items())):
        raise ValueError('invalid renderer metadata: resources')
    meta['resources'] = {key: str(artifact.output(name)) for key, name in resources.items()}
    preloads = meta.get('preload_files', [])
    if not isinstance(preloads, list):
        raise ValueError('invalid renderer metadata: preload_files')
    resolved = []
    for item in preloads:
        if (not isinstance(item, dict) or not text(item.get('path')) or not text(item.get('mount'))):
            raise ValueError('invalid renderer preload')
        mount = item['mount']
        if (not mount.startswith('/') or '@' in mount or '\\' in mount
                or '..' in PurePosixPath(mount).parts or str(PurePosixPath(mount)) != mount):
            raise ValueError('invalid renderer preload mount')
        resolved.append(dict(path=str(artifact.output(item['path'])), mount=mount))
    meta['preload_files'] = resolved
    return meta


def renderer_link(meta, *, preloads=True):
    """Ordered linker argv from renderer_metadata, including web resources."""
    flags = [*meta['archives'], *meta['link_flags']]
    for library in meta.get('shared_libraries', []):
        # Retain the lazy codec in DT_NEEDED so application packaging tools copy
        # it. The first path supports the development artifact; ORIGIN supports
        # installed AppDirs independently of cwd and system codec packages.
        flags.extend(['-Wl,--no-as-needed', library, '-Wl,--as-needed'])
        if not meta['platform'].startswith('android-'):
            flags.extend(['-Wl,-rpath,' + str(Path(library).parent), '-Wl,-rpath,$ORIGIN/../lib'])
    for path in meta['js_libraries']:
        flags.extend(['--js-library', path])
    if preloads:
        for item in meta['preload_files']:
            flags.extend(['--preload-file', item['path'] + '@' + item['mount']])
    return flags


def run(args, **kwargs):
    print('+ ' + shlex.join(map(str, args)), file=sys.stderr)
    return subprocess.run(list(map(str, args)), check=True, stdout=sys.stderr, **kwargs)


def capture(args, **kwargs):
    return subprocess.check_output(list(map(str, args)), text=True, **kwargs).strip()


def options():
    return dict(line.split('=', 1) for line in
                Path(os.environ['ZIRAN_TOOL_OPTIONS']).read_text().splitlines() if '=' in line)


def locked_provider(lock, package_id, local=False):
    """Find Kryon's saved dependency edges when a local override changes its ID."""
    exact = [package for package in lock['packages'] if package['id'] == package_id]
    if len(exact) == 1:
        return exact[0]
    if not local:
        raise ValueError('resolved provider identity is absent from ziran.lock')
    root_ids = set(lock.get('root', {}).get('dependencies', {}).values())
    rooted = [package for package in lock['packages']
              if package.get('name') == 'kryon' and package['id'] in root_ids]
    if len(rooted) == 1:
        return rooted[0]
    candidates = [package for package in lock['packages'] if package.get('name') == 'kryon']
    if len(candidates) == 1:
        return candidates[0]
    raise ValueError('local Kryon provider is ambiguous in ziran.lock')


def copy(source, output):
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, output)


def mingw_library_flags(cc):
    # Nix MinGW records its immutable propagated CRT/thread libraries beside
    # the compiler wrapper. Ordinary MinGW installations need no extra flags.
    support = Path(shutil.which(cc) or cc).resolve().parent.parent / 'nix-support'
    manifest = support / 'propagated-target-target-deps'
    pending = manifest.read_text().split() if manifest.is_file() else []
    seen, flags = set(), []
    while pending:
        path = Path(pending.pop())
        if path in seen:
            continue
        seen.add(path)
        if (path / 'lib').is_dir():
            flags.append('-L' + str(path / 'lib'))
        propagated = path / 'nix-support/propagated-build-inputs'
        if propagated.is_file():
            pending.extend(propagated.read_text().split())
    return flags


def verify_locked_commit(source, commit, local, name):
    # A local override is a live checkout that pkg path already validated;
    # only locked builds must match the provider edge's exact commit.
    if not local and capture(['git', '-C', source, 'rev-parse', 'HEAD']) != commit:
        raise ValueError(f'resolved {name} does not match the provider locked edge')


def build(name, target, stage, root):
    if name == 'native-sdl3':
        if target != 'linux-x86_64-gnu':
            raise ValueError(f'{name} does not support {target}')
        project = Path(os.environ['ZIRAN_PROJECT_ROOT'])
        toolchain = Path(os.environ['ZIRAN_TOOLCHAIN_ROOT'])
        lock = json.loads((project / 'ziran.lock').read_text())
        provider = locked_provider(lock, os.environ['ZIRAN_PACKAGE_ID'],
                                   (project / 'ziran.local.toml').exists())
        dependency = next(p for p in lock['packages']
                          if p['id'] == provider['dependencies']['sdl3'])
        locked = [] if (project / 'ziran.local.toml').exists() else ['--locked']
        source = Path(capture([toolchain / 'build/bin/ziran', 'pkg', 'path',
                               dependency['name'], *locked], cwd=project))
        verify_locked_commit(source, dependency['commit'], not locked, 'SDL3')
        run(['sh', root / 'scripts/build-sdl3.sh', target, stage, source])
        return
    if name not in SUPPORTED_RENDERERS:
        raise ValueError('unknown renderer artifact: ' + name)
    if target not in SUPPORTED_RENDERERS[name]:
        raise ValueError(f'{name} does not support {target}')
    opts = options()
    toolchain = Path(os.environ['ZIRAN_TOOLCHAIN_ROOT'])
    meta = dict(schema=1, artifact=name, platform=target, compile_flags=[],
                link_flags=[], defines=[], archives=[], include_directories=[],
                resources={'font': 'resources/kryon-font.ttf',
                           'font_license': 'resources/LICENSE-Liberation'})
    copy(root / 'assets/fonts/LiberationSans-Regular.ttf', stage / meta['resources']['font'])
    copy(root / 'assets/fonts/LICENSE-Liberation', stage / meta['resources']['font_license'])
    if name == 'renderer-desktop-windows':
        from windows_desktop import artifact
        meta['archives'] = artifact(stage)
        meta['defines'] = ['_WIN32', 'PLATFORM_DESKTOP']
        meta['link_flags'] = ['-lcomdlg32', '-luser32', '-lshell32', '-lkernel32', '-lm']
        meta['runtime_archive'] = 'runtime.tar'
        (stage / 'renderer.json').write_text(json.dumps(meta, indent=2) + '\n')
        return
    # Resolve the provider's locked dependency edge, never a consumer's
    # potentially shadowing top-level alias. pkg path remains the public API.
    project = Path(os.environ['ZIRAN_PROJECT_ROOT'])
    lock = json.loads((project / 'ziran.lock').read_text())
    provider = locked_provider(lock, os.environ['ZIRAN_PACKAGE_ID'],
                               (project / 'ziran.local.toml').exists())
    # pkg path selects names; reject an ambiguous/shadowed result explicitly.
    locked = [] if (project / 'ziran.local.toml').exists() else ['--locked']
    def locked_source(name, owner=None):
        edge = (owner or provider)['dependencies'][name]
        dependency = next(p for p in lock['packages'] if p['id'] == edge)
        source = Path(capture([toolchain / 'build/bin/ziran', 'pkg', 'path',
                               dependency['name'], *locked], cwd=project))
        # pkg path validates an explicit local development identity itself.
        # Committed dependencies additionally retain the exact provider edge.
        if 'commit' in dependency:
            verify_locked_commit(source, dependency['commit'], not locked, name)
        return source
    avif_source = locked_source('libavif') if name != 'renderer-web' else None
    aom_source = locked_source('libaom') if name != 'renderer-web' else None
    avif_package = locked_source('avif') if name != 'renderer-web' else None
    # dav1d, the default AV1 engine, is pinned by the avif package's own edge.
    avif_entry = next((p for p in lock['packages'] if p['id'] == provider['dependencies'].get('avif')), None)
    dav1d_source = locked_source('dav1d', avif_entry) if name != 'renderer-web' else None
    if name != 'renderer-web':
        webp_source = locked_source('libwebp')
        webp_package = locked_source('webp')
        audio_package = locked_source('audio')
        audio_entry = next(p for p in lock['packages'] if p['id'] == provider['dependencies']['audio'])
        opus_source = locked_source('libopus', audio_entry)
        samplerate_source = locked_source('libsamplerate', audio_entry)
        opus_config = [f'-DOPUS_DIR={opus_source}', f'-DSAMPLERATE_DIR={samplerate_source}']
        copy(webp_source / 'COPYING', stage / 'resources/LICENSE-libwebp')
        copy(webp_source / 'PATENTS', stage / 'resources/PATENTS-libwebp')
        copy(opus_source / 'COPYING', stage / 'resources/LICENSE-opus')

    def codec_arguments(work, cc):
        return [f'KRYON_DIR={root}', f'CODEC_CC={cc}', 'CODEC_JOBS=2',
                f'WEBP_PACKAGE={webp_source}', f'WEBP_CODEC_PACKAGE={webp_package}',
                f'WEBP_BUILD={work}/webp', f'AVIF_CODEC_PACKAGE={avif_package}',
                f'AVIF_PACKAGE={avif_source}', f'AOM_PACKAGE={aom_source}',
                f'DAV1D_PACKAGE={dav1d_source}', f'AVIF_BUILD={work}/avif-codec',
                f'AUDIO_CODEC_PACKAGE={audio_package}', f'OPUS_PACKAGE={opus_source}',
                f'SAMPLERATE_PACKAGE={samplerate_source}', f'OPUS_BUILD={work}/audio']

    if name == 'renderer-web':
        copy(toolchain / 'web/ziran_web.js', stage / 'web/ziran_web.js')
        meta['link_flags'] = ['-sUSE_ZLIB=1', '-lidbfs.js', '-sFORCE_FILESYSTEM=1',
                              '-sEXPORTED_RUNTIME_METHODS=FS,FS_createPath,FS_createDataFile,addRunDependency,removeRunDependency',
                              '-sALLOW_MEMORY_GROWTH=1', '-sASYNCIFY', '-sENVIRONMENT=web',
                              '-sSTACK_SIZE=8388608', '-sASYNCIFY_STACK_SIZE=1048576']
        meta['js_libraries'] = ['web/ziran_web.js']
        meta['preload_files'] = [{'path': meta['resources']['font'], 'mount': '/kryon-font.ttf'}]
        meta['defines'] = ['PLATFORM_WEB']
    elif name == 'renderer-desktop' and target.startswith('android-'):
        ndk = Path(opts.get('renderer.android_ndk', ''))
        api = opts.get('renderer.android_api', '21')
        if not api.isdecimal() or int(api) < 21 or not (ndk / 'build/cmake/android.toolchain.cmake').is_file():
            raise ValueError('Android requires renderer.android_ndk and android_api >= 21')
        arch, abi = ('aarch64', 'arm64-v8a') if 'aarch64' in target else ('arm', 'armeabi-v7a')
        cc = os.environ.get('CC', 'cc')
        triple = capture([cc, '-dumpmachine'])
        if arch not in triple or 'android' not in triple:
            raise ValueError('CC must target the selected Android architecture')
        with tempfile.TemporaryDirectory(prefix='.build-', dir=stage) as temporary:
            work = Path(temporary)
            run(['cmake', '-S', root / 'cmake/desktop-codecs', '-B', work,
                 '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_SHARED_LIBS=OFF',
                 '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
                 '-DCMAKE_SHARED_LINKER_FLAGS=-Wl,--build-id=none',
                 f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',
                 f'-DANDROID_ABI={abi}', f'-DANDROID_PLATFORM=android-{api}',
                 f'-DWEBP_DIR={webp_source}', f'-DKRYON_WEBP_PACKAGE_DIR={webp_package}',
                 f'-DKRYON_AVIF_PACKAGE_DIR={avif_package}', f'-DAVIF_DIR={avif_source}',
                 f'-DAOM_DIR={aom_source}', f'-DDAV1D_DIR={dav1d_source}',
                 f'-DAUDIO_DIR={audio_package}', *opus_config])
            run(['cmake', '--build', work, '--target', 'kryonwebp', 'avif_static',
                 'aom', 'opus', 'kryonavif', '--parallel', '1'])
            for archive in ('libkryonwebp.a', 'libavif.a', 'libaom.a', 'libopus.a'):
                copy(next(work.rglob(archive)), stage / 'lib' / archive)
            copy(next(work.rglob('libkryonavif.so')), stage / 'lib/libkryonavif.so')
        meta['archives'] = ['lib/libkryonwebp.a', 'lib/libavif.a', 'lib/libaom.a', 'lib/libopus.a']
        meta['shared_libraries'] = ['lib/libkryonavif.so']
        meta['defines'] = ['ANDROID_BUILD']
        meta['link_flags'] = ['-lm']
    elif name == 'renderer-desktop':
        packages = ['sdl2', 'cairo', 'freetype2', 'pangocairo', 'fontconfig', 'gio-2.0']
        meta['compile_flags'] = shlex.split(capture(['pkg-config', '--cflags', *packages]))
        meta['link_flags'] = shlex.split(capture(['pkg-config', '--libs', *packages])) + ['-ldl', '-lm', '-lpthread']
        cc = os.environ.get('CC', 'cc')
        triple = capture([cc, '-dumpmachine'])
        if 'x86_64' not in triple or 'linux' not in triple:
            raise ValueError('CC must target Linux x86_64')
        with tempfile.TemporaryDirectory(prefix='.build-', dir=stage) as temporary:
            work = Path(temporary)
            run(['make', '-j1', '-f', root / 'mk/codecs.mk', *codec_arguments(work, cc),
                 'codec-archives', str(work / 'avif-codec/libkryonavif.so')])
            copy(work / 'webp/libkryonwebp.a', stage / 'lib/libkryonwebp.a')
            copy(work / 'avif-codec/avif/libavif.a', stage / 'lib/libavif.a')
            copy(work / 'avif-codec/aom/libaom.a', stage / 'lib/libaom.a')
            copy(work / 'audio/libopus/libopus.a', stage / 'lib/libopus.a')
            copy(work / 'avif-codec/libkryonavif.so', stage / 'lib/libkryonavif.so')
        meta['archives'] = ['lib/libkryonwebp.a', 'lib/libavif.a', 'lib/libaom.a', 'lib/libopus.a']
        meta['shared_libraries'] = ['lib/libkryonavif.so']
    if name != 'renderer-web':
        copy(avif_source / 'LICENSE', stage / 'resources/LICENSE-libavif')
        copy(aom_source / 'LICENSE', stage / 'resources/LICENSE-libaom')
        copy(aom_source / 'PATENTS', stage / 'resources/PATENTS-libaom')
        copy(dav1d_source / 'COPYING', stage / 'resources/LICENSE-dav1d')
    (stage / 'renderer.json').write_text(json.dumps(meta, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('name')
    parser.add_argument('--platform', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('root', type=Path)
    args = parser.parse_args()
    try:
        build(args.name, args.platform, args.output, args.root)
    except (ValueError, KeyError, StopIteration, OSError, subprocess.CalledProcessError) as error:
        print(f'kryon renderer artifact: {error}', file=sys.stderr)
        sys.exit(2)

"""Build tooling for dependency-owned foreign renderers (no generated Zi graph)."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile


def run(args, **kwargs):
    print('+ ' + shlex.join(map(str, args)), file=sys.stderr)
    return subprocess.run(list(map(str, args)), check=True, stdout=sys.stderr, **kwargs)


def capture(args, **kwargs):
    return subprocess.check_output(list(map(str, args)), text=True, **kwargs).strip()


def options():
    return dict(line.split('=', 1) for line in
                Path(os.environ['ZIRAN_TOOL_OPTIONS']).read_text().splitlines() if '=' in line)


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


def build(name, target, stage, root):
    if name not in ('renderer-raylib', 'renderer-desktop', 'renderer-web'):
        raise ValueError('unknown renderer artifact: ' + name)
    supported = {'renderer-raylib': ('linux-x86_64-gnu', 'windows-x86_64-mingw',
                                    'android-aarch64-bionic', 'android-armv7-bionic'),
                 'renderer-desktop': ('linux-x86_64-gnu',),
                 'renderer-web': ('emscripten-wasm32-emscripten',)}
    if target not in supported[name]:
        raise ValueError(f'{name} does not support {target}')
    opts = options()
    toolchain = Path(os.environ['ZIRAN_TOOLCHAIN_ROOT'])
    meta = dict(schema=1, artifact=name, platform=target, compile_flags=[],
                link_flags=[], defines=[], archives=[], include_directories=[],
                resources={'font': 'resources/kryon-font.ttf',
                           'font_license': 'resources/LICENSE-Liberation'})
    copy(root / 'assets/fonts/LiberationSans-Regular.ttf', stage / meta['resources']['font'])
    copy(root / 'assets/fonts/LICENSE-Liberation', stage / meta['resources']['font_license'])
    if name == 'renderer-web':
        copy(toolchain / 'web/ziran_web.js', stage / 'web/ziran_web.js')
        meta['link_flags'] = ['-sUSE_ZLIB=1', '-lidbfs.js', '-sFORCE_FILESYSTEM=1',
                              '-sEXPORTED_RUNTIME_METHODS=FS,FS_createPath,FS_createDataFile,addRunDependency,removeRunDependency',
                              '-sALLOW_MEMORY_GROWTH=1', '-sASYNCIFY', '-sENVIRONMENT=web',
                              '-sSTACK_SIZE=8388608', '-sASYNCIFY_STACK_SIZE=1048576']
        meta['js_libraries'] = ['web/ziran_web.js']
        meta['preload_files'] = [{'path': meta['resources']['font'], 'mount': '/kryon-font.ttf'}]
        meta['defines'] = ['PLATFORM_WEB']
    elif name == 'renderer-desktop':
        packages = ['sdl2', 'cairo', 'freetype2', 'pangocairo', 'fontconfig', 'gio-2.0']
        meta['compile_flags'] = shlex.split(capture(['pkg-config', '--cflags', *packages]))
        meta['link_flags'] = shlex.split(capture(['pkg-config', '--libs', *packages])) + ['-lm', '-lpthread']
    else:
        # Resolve the provider's locked dependency edge, never a consumer's
        # potentially shadowing top-level alias. pkg path remains the public API.
        project = Path(os.environ['ZIRAN_PROJECT_ROOT'])
        lock = json.loads((project / 'ziran.lock').read_text())
        provider = next(p for p in lock['packages'] if p['id'] == os.environ['ZIRAN_PACKAGE_ID'])
        edge = provider['dependencies']['raylib']
        dependency = next(p for p in lock['packages'] if p['id'] == edge)
        # pkg path selects names; reject an ambiguous/shadowed result explicitly.
        locked = [] if (project / 'ziran.local.toml').exists() else ['--locked']
        source = Path(capture([toolchain / 'build/bin/ziran', 'pkg', 'path', dependency['name'], *locked], cwd=project))
        if capture(['git', '-C', source, 'rev-parse', 'HEAD']) != dependency['commit']:
            raise ValueError('resolved Raylib does not match the provider locked edge')
        cc = os.environ.get('CC', 'cc')
        triple = capture([cc, '-dumpmachine'])
        if target == 'linux-x86_64-gnu' and ('x86_64' not in triple or 'linux' not in triple):
            raise ValueError('CC must target Linux x86_64')
        if target == 'windows-x86_64-mingw' and ('x86_64' not in triple or 'mingw' not in triple):
            raise ValueError('CC must target Windows x86_64 MinGW')
        headers = ['raylib.h', 'rlgl.h', 'raymath.h', 'rcamera.h']
        for header in headers:
            copy(source / 'src' / header, stage / 'include' / header)
        copy(source / 'LICENSE', stage / 'resources/LICENSE-raylib')
        meta.update(archives=['lib/libraylib.a'], include_directories=['include'])
        # Intermediates are disposable and kept inside staging; only final files
        # remain when the generic driver validates and publishes the descriptor.
        with tempfile.TemporaryDirectory(prefix='.build-', dir=stage) as temporary:
            work = Path(temporary)
            if target == 'linux-x86_64-gnu':
                flags = capture(['pkg-config', '--cflags', 'sdl2', 'libdrm', 'gbm', 'egl', 'glesv2'])
                run(['make', '-j1', '-f', root / 'mk/renderer-artifact.mk',
                     'RAYLIB_JOBS=1',
                     f'KRYON_DIR={root}', f'RAYLIB_PACKAGE={source}', f'RAYLIB_BUILD={work}',
                     f'RAYLIB_CC={cc}', f'RAYLIB_AR={os.environ.get("AR", "ar")}',
                     f'RAYLIB_CFLAGS={flags} -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_TTF=1',
                     str(work / 'libraylib.a')])
                archive = work / 'libraylib.a'
                meta['compile_flags'] = shlex.split(flags)
                meta['link_flags'] = shlex.split(capture(['pkg-config', '--libs', 'sdl2', 'libdrm', 'gbm', 'egl', 'glesv2', 'fontconfig'])) + ['-ldl', '-lpthread', '-lm']
                meta['defines'] = ['PLATFORM_DESKTOP_SDL', 'GRAPHICS_API_OPENGL_ES2']
            else:
                config = ['-DCMAKE_BUILD_TYPE=Release', '-DBUILD_SHARED_LIBS=OFF', '-DBUILD_EXAMPLES=OFF']
                if target == 'windows-x86_64-mingw':
                    compiler = Path(shutil.which(cc) or cc)
                    prefix = compiler.name.removesuffix('gcc')
                    libraries = mingw_library_flags(cc)
                    config += ['-DCMAKE_SYSTEM_NAME=Windows', f'-DCMAKE_C_COMPILER={cc}',
                               '-DCMAKE_RC_COMPILER=' + str(compiler.with_name(prefix + 'windres')),
                               '-DCMAKE_AR=' + os.environ.get('AR', str(compiler.with_name(prefix + 'ar'))),
                               '-DCMAKE_RANLIB=' + str(compiler.with_name(prefix + 'ranlib')),
                               '-DCMAKE_EXE_LINKER_FLAGS=' + ' '.join(libraries),
                               '-DPLATFORM=RGFW', '-DOPENGL_VERSION=1.1',
                               '-DOPENGL_opengl_LIBRARY=opengl32', '-DOPENGL_gl_LIBRARY=opengl32',
                               '-DCMAKE_C_FLAGS=-O2 -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_TTF=1']
                    cmake_source = source
                    meta['defines'] = ['_WIN32', 'PLATFORM_DESKTOP_RGFW', 'GRAPHICS_API_OPENGL_11']
                    meta['link_flags'] = libraries + ['-lopengl32', '-lgdi32', '-lwinmm', '-luser32', '-lshell32', '-lole32', '-luuid', '-lm']
                else:
                    ndk = Path(opts.get('renderer.android_ndk', ''))
                    api = opts.get('renderer.android_api', '21')
                    if not api.isdecimal() or int(api) < 21 or not (ndk / 'build/cmake/android.toolchain.cmake').is_file():
                        raise ValueError('Android requires renderer.android_ndk and android_api >= 21')
                    arch, abi, macro = ('aarch64', 'arm64-v8a', '__aarch64__') if 'aarch64' in target else ('arm', 'armeabi-v7a', '__arm__')
                    if arch not in triple or 'android' not in triple:
                        raise ValueError('CC must target the selected Android architecture')
                    cmake_source = root / 'cmake/renderer-artifact'
                    config += [f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',
                               f'-DANDROID_ABI={abi}', f'-DANDROID_PLATFORM=android-{api}',
                               f'-DRAYLIB_DIR={source}/src']
                    meta['defines'] = ['ANDROID_BUILD', macro]
                    meta['link_flags'] = ['-Wl,--wrap=fopen', '-lEGL', '-lGLESv2', '-landroid', '-llog', '-lOpenSLES', '-lm', '-ldl']
                run(['cmake', '-S', cmake_source, '-B', work, *config])
                run(['cmake', '--build', work, '--target', 'raylib', '--parallel', '1'])
                archive = next(work.rglob('libraylib.a'))
            copy(archive, stage / 'lib/libraylib.a')
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

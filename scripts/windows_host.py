"""Build and optionally install the Windows library used by direct native apps."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

from renderer_artifact import mingw_library_flags
from windows_desktop import LIBRARIES, ROOT, prepare


def build(ziran, cc):
    work = ROOT / 'build/windows-desktop'
    sdk = prepare(work / 'sdk')
    generated = work / 'kryon-desktop'
    subprocess.run([ziran / 'build/bin/ziran', 'build', '--project', '--target=c',
                    '--target-os', 'windows', '--target-cpu', 'x64', '--define', '_WIN32',
                    '--define', 'PLATFORM_DESKTOP', '--no-main', '--prune-stale',
                    '--root', ROOT / 'src/backend', '-o', generated,
                    ROOT / 'src/backend/zx_window_host.zi'], cwd=ROOT, check=True)
    library = work / 'kryon-desktop.dll'
    temporary = library.with_suffix('.dll.new')
    try:
        subprocess.run([cc, *mingw_library_flags(cc), '-std=c11', '-O2', '-static-libgcc',
                        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                        '-shared', '-Wl,--export-all-symbols',
                        '-I' + str(ziran / 'include'), '-I' + str(generated),
                        *sorted(generated.glob('*.c')),
                        *[sdk / 'lib' / ('lib' + name + '.dll.a') for name in LIBRARIES],
                        '-lcomdlg32', '-luser32', '-lshell32', '-lkernel32', '-lm',
                        '-o', temporary], cwd=ROOT, check=True)
        temporary.replace(library)
    finally:
        temporary.unlink(missing_ok=True)
    return library, sdk


def install(library, sdk, prefix):
    destination = prefix / 'lib/kryon/windows'
    destination.mkdir(parents=True, exist_ok=True)
    for source in [library, *sorted((sdk / 'bin').glob('*.dll'))]:
        target = destination / source.name
        temporary = target.with_suffix(target.suffix + '.new')
        shutil.copyfile(source, temporary)
        temporary.replace(target)
    # Keep upstream licenses and fontconfig data with the installed runtime.
    for name in ['etc', 'share']:
        shutil.copytree(sdk / name, destination / name, dirs_exist_ok=True)
    registry = prefix / 'share/ziran/hosts'
    registry.mkdir(parents=True, exist_ok=True)
    registration = registry / 'kryon-desktop-windows.library'
    temporary = registration.with_suffix('.library.new')
    temporary.write_text(str((destination / library.name).resolve()) + '\n')
    temporary.replace(registration)
    print('Registered Windows native host:', registration, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ziran', type=Path, required=True)
    parser.add_argument('--cc', default=os.environ.get('WINDOWS_CC', 'x86_64-w64-mingw32-gcc'))
    parser.add_argument('--prefix', type=Path)
    args = parser.parse_args()
    library, sdk = build(args.ziran.resolve(), args.cc)
    if args.prefix:
        install(library, sdk, args.prefix.expanduser().resolve())


if __name__ == '__main__':
    main()

"""Pinned MinGW desktop libraries, shared by the artifact and Wine tests.

MSYS2 packages remain in the ignored download cache. Only import libraries,
DLLs, licenses and fontconfig data are published; no dependency source trees.
"""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
LIBRARIES = ['SDL2', 'cairo', 'pangocairo-1.0', 'pango-1.0', 'fontconfig',
             'freetype', 'gobject-2.0', 'glib-2.0', 'gdk_pixbuf-2.0', 'opus']


def prepare(destination):
    destination = Path(destination)
    identity = hashlib.sha256((ROOT / 'scripts/windows-desktop-packages.json').read_bytes()).hexdigest()
    stamp = destination / '.packages.sha256'
    if stamp.is_file() and stamp.read_text() == identity and all((destination / 'lib' / ('lib' + lib + '.dll.a')).is_file() for lib in LIBRARIES):
        return destination
    cache = ROOT / 'build/downloads/windows-desktop'
    cache.mkdir(parents=True, exist_ok=True)
    packages = json.loads((ROOT / 'scripts/windows-desktop-packages.json').read_text())

    def download(package):
        path = cache / package['file']
        def valid():
            if not path.is_file():
                return False
            with path.open('rb') as stream:
                return hashlib.file_digest(stream, 'sha256').hexdigest() == package['sha256']
        if not valid():
            temporary = path.with_suffix('.part')
            with urllib.request.urlopen('https://repo.msys2.org/mingw/mingw64/' + package['file'], timeout=60) as source, temporary.open('wb') as target:
                shutil.copyfileobj(source, target)
            temporary.replace(path)
            if not valid():
                path.unlink()
                raise ValueError('MSYS2 checksum mismatch: ' + package['file'])
        return path

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as workers:
        archives = list(workers.map(download, packages))
    destination.mkdir(parents=True, exist_ok=True)
    # tar handles zstd on build hosts whose Python predates tarfile's support.
    for archive in archives:
        names = subprocess.check_output(['tar', 'tf', archive], text=True).splitlines()
        selected = [n for n in names if not n.endswith('/') and
                    (n.startswith('mingw64/bin/') and n.endswith('.dll') or
                     n.startswith('mingw64/lib/') and Path(n).name in
                     {'lib' + lib + '.dll.a' for lib in LIBRARIES} or
                     n.startswith(('mingw64/share/licenses/', 'mingw64/etc/fonts/',
                                   'mingw64/share/fontconfig/')))]
        if any('..' in Path(n).parts or n.startswith('/') for n in selected):
            raise ValueError('unsafe MSYS2 archive path')
        if selected:
            subprocess.run(['tar', 'xf', archive, '--strip-components=1', '-C', destination,
                            '--', *selected], check=True)
    for lib in LIBRARIES:
        if not (destination / 'lib' / ('lib' + lib + '.dll.a')).is_file():
            raise ValueError('missing Windows import library: ' + lib)
    stamp.write_text(identity)
    return destination


def artifact(stage):
    stage = Path(stage)
    sdk = prepare(stage / '.sdk')
    (stage / 'lib').mkdir(exist_ok=True)
    for lib in LIBRARIES:
        shutil.copyfile(sdk / 'lib' / ('lib' + lib + '.dll.a'), stage / 'lib' / ('lib' + lib + '.dll.a'))
    with tarfile.open(stage / 'runtime.tar', 'w') as archive:
        for name in ['bin', 'etc', 'share']:
            archive.add(sdk / name, arcname=name)
    shutil.rmtree(sdk)
    return ['lib/lib' + lib + '.dll.a' for lib in LIBRARIES]


if __name__ == '__main__':
    import sys
    prepare(sys.argv[1])

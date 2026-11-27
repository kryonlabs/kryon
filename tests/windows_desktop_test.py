"""Cross-link SDL/Cairo desktop hosts and check Wine windows on private Xvfb."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, kryon_test_project
sys.path.insert(0, str(ROOT / 'scripts'))
from renderer_artifact import mingw_library_flags
from windows_desktop import LIBRARIES

WORK = ROOT / 'build/windows-desktop'
CC = os.environ.get('WINDOWS_CC', 'x86_64-w64-mingw32-gcc')
ENV = os.environ.copy()
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
            'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'WINEPREFIX',
            'WINEARCH', 'WINEDLLPATH', 'WINELOADER', 'WINEDLLOVERRIDES'):
    ENV.pop(key, None)
ENV.update(HARMONY_DESKTOP_RECOVERY='0', YUE_DESKTOP_RECOVERY='0')


def run(command, **kwargs):
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=ENV, **kwargs)
    if result.returncode:
        raise SystemExit(f'{Path(str(command[0])).name} failed: {result.returncode}')


def compile_source(source, name, *, library=False, options=(), sources=(), libraries=()):
    generated = WORK / name
    run([ZIRAN, 'build', '--project', '--target=c', '--target-os', 'windows',
         '--target-cpu', 'x64', '--define', '_WIN32', '--define', 'PLATFORM_DESKTOP',
         '--prune-stale', '--root', Path(source).parent, *(['--no-main'] if library else ['--entry', Path(source).stem + ':main']),
         *options, '-o', generated, source, *sources])
    binary = WORK / (name + ('.dll' if library else '.exe'))
    run([CC, *mingw_library_flags(CC), '-std=c11', '-O2', '-static-libgcc',
         '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
         *(['-shared', '-Wl,--export-all-symbols', '-Wl,--out-implib,' + str(WORK / 'libkryon-desktop.dll.a')] if library else []),
         '-I' + str(ZIRAN_ROOT / 'include'), '-I' + str(generated),
         *sorted(generated.glob('*.c')),
         *[WORK / 'sdk/lib' / ('lib' + lib + '.dll.a') for lib in LIBRARIES], *libraries,
         '-lcomdlg32', '-luser32', '-lshell32', '-lkernel32', '-lm', '-o', binary])
    imports = subprocess.check_output(['x86_64-w64-mingw32-objdump', '-p', binary], text=True)
    assert 'raylib' not in imports.lower(), f'{binary.name}: raylib dependency'
    return binary


def test_window(binary, title, *, widgets=False, services=False, size=(320, 160),
                text=False, background=(248, 250, 252)):
    from Xlib import X, display, protocol
    from PIL import Image
    with (WORK / (binary.stem + '.log')).open('wb') as log:
        process = subprocess.Popen(['wine', str(binary)], env=ENV, stdout=log, stderr=subprocess.STDOUT)
        connection = display.Display()
        try:
            window = None
            deadline = time.monotonic() + 40
            while process.poll() is None and time.monotonic() < deadline:
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', '^' + title + '$'],
                                       env=ENV, capture_output=True, text=True)
                if found.returncode == 0:
                    for ident in found.stdout.split():
                        candidate = connection.create_resource_object('window', int(ident))
                        geometry = candidate.get_geometry()
                        if (geometry.width, geometry.height) == size:
                            window = candidate
                            break
                if window is not None:
                    break
                time.sleep(.1)
            assert window is not None, f'{binary.name}: no {size} {title} window; see its log'
            # Wait for a complete SDL presentation, never the owner's desktop.
            time.sleep(.4)
            raw = window.get_image(0, 0, *size, X.ZPixmap, 0xffffffff)
            image = Image.frombytes('RGB', size, raw.data, 'raw', 'BGRX')
            image.save(WORK / (binary.stem + '.png'))
            actual = image.getpixel((size[0] - 10, size[1] - 10))
            assert actual == background, f'{binary.name}: clear colour {actual}, expected {background}'
            if widgets or text:
                dark = lambda box: sum(sum(image.getpixel((x, y))) < 450
                                       for y in range(box[1], box[3]) for x in range(box[0], box[2]))
                assert dark((20, 15, 300, 53)) > 80, 'Text glyphs missing'
            if widgets:
                assert dark((30, 70, 210, 118)) > 40, 'Button glyphs missing'
                assert any(image.getpixel((30, y)) != background for y in range(75, 113)), 'Button surface missing'
            if services:
                found = subprocess.check_output(['xdotool', 'search', '--onlyvisible', '--name', '^Kryon Windows Secondary$'], env=ENV, text=True)
                secondary = connection.create_resource_object('window', int(found.split()[0]))
                geometry = secondary.get_geometry()
                assert (geometry.width, geometry.height) == (160, 100)
                pixels = secondary.get_image(0, 0, 160, 100, X.ZPixmap, 0xffffffff)
                picture = Image.frombytes('RGB', (160, 100), pixels.data, 'raw', 'BGRX')
                picture.save(WORK / 'secondary.png')
                assert picture.getpixel((80, 50)) == (20, 80, 140), 'secondary Cairo surface'
            message = protocol.event.ClientMessage(window=window, client_type=connection.intern_atom('WM_PROTOCOLS'),
                data=(32, [connection.intern_atom('WM_DELETE_WINDOW'), X.CurrentTime, 0, 0, 0]))
            window.send_event(message, event_mask=X.NoEventMask)
            connection.flush()
            assert process.wait(timeout=10) == 0, 'window close did not exit cleanly'
            print('PASS Wine pixels and close:', binary.name, flush=True)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            connection.close()


def test_dialog():
    with (WORK / 'dialog.log').open('wb') as log:
        process = subprocess.Popen(['wine', str(WORK / 'dialog.exe')], env=ENV, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 20
            ident = None
            while time.monotonic() < deadline and process.poll() is None:
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', '^Kryon Windows File Dialog$'], env=ENV, capture_output=True, text=True)
                if found.returncode == 0:
                    ident = found.stdout.split()[0]
                    break
                time.sleep(.1)
            assert ident, 'native Windows file dialog did not open'
            subprocess.run(['xdotool', 'windowfocus', '--sync', ident, 'key', 'Escape'], env=ENV, check=True)
            assert process.wait(timeout=10) == 0, 'native file dialog cancellation failed'
            print('PASS Wine native file dialog cancellation', flush=True)
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)


def check_zx_host():
    registrations = WORK / 'hosts'
    registrations.mkdir(exist_ok=True)
    (registrations / 'kryon-desktop.library').write_text(str(WORK / 'kryon-desktop.dll') + '\n')
    for name in ('window', 'hello'):
        run([ZIRAN, 'bundle', '--project', '--root', ROOT / 'examples', '--entry', name + ':main',
             '-o', WORK / (name + '.zx'), ROOT / 'examples' / (name + '.zi')])
        result = subprocess.run([ZIRAN_ROOT / 'build/bin/zx2native', '--target', 'x86_64',
                                 '--platform', 'windows', '-o', WORK / (name + '-zx.exe'), WORK / (name + '.zx')],
                                cwd=ROOT, env={**ENV, 'ZIRAN_HOST_PATH': str(registrations)}, capture_output=True, text=True)
        (WORK / (name + '-zx-windows-host.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr[:2000]
    print('PASS Windows ZX host link', flush=True)
    return True


def sdk_artifact():
    consumer = WORK / 'consumer'
    kryon_test_project(consumer)
    with (WORK / 'artifact.log').open('w') as log:
        result = subprocess.run([ZIRAN, 'pkg', 'artifact', 'kryon:renderer-desktop-windows',
                                 '--platform', 'windows-x86_64-mingw'], cwd=consumer, env=ENV,
                                text=True, stdout=subprocess.PIPE, stderr=log)
    assert result.returncode == 0, 'Windows renderer artifact failed; see artifact.log'
    descriptor = Path(result.stdout.strip())
    sys.path.insert(0, str(ZIRAN_ROOT / 'scripts'))
    from ziran_artifacts import Artifact
    from renderer_artifact import renderer_metadata, renderer_link
    artifact = Artifact(descriptor)
    meta = renderer_metadata(artifact)
    assert len(renderer_link(meta)) >= len(LIBRARIES)
    shutil.copyfile(meta['resources']['font'], WORK / 'kryon-font.ttf')
    shutil.copyfile(meta['resources']['font_license'], WORK / 'LICENSE-Liberation')
    sdk = WORK / 'sdk'
    (sdk / 'lib').mkdir(parents=True, exist_ok=True)
    for archive in meta['archives']:
        shutil.copyfile(archive, sdk / 'lib' / Path(archive).name)
    import tarfile
    with tarfile.open(meta['runtime_archive']) as archive:
        archive.extractall(sdk, filter='data')
    print('PASS Windows renderer artifact descriptor, hashes, and consumer metadata', flush=True)


def private(zx_linked):
    ENV.update(DISPLAY=os.environ['DISPLAY'], XAUTHORITY=os.environ['XAUTHORITY'], WINEPREFIX=str(WORK / 'wine-prefix'),
               WINEARCH='win64', WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=',
               KRYON_FONT_PATH='Z:' + str(WORK / 'kryon-font.ttf'),
               SDL_RENDER_DRIVER='software')
    # Wine 10 on this host cannot safely inherit TMPDIR during server startup.
    ENV.pop('TMPDIR', None)
    servers = [shutil.which('wineserver'), '/usr/lib/x86_64-linux-gnu/wine/wineserver', '/usr/lib/wine/wineserver']
    server = next(Path(p) for p in servers if p and Path(p).is_file())
    try:
        run(['wine', 'wineboot.exe', '--init'], timeout=90)
        test_window(WORK / 'window.exe', 'Window')
        # App.Run uses the widget theme's white canvas; bare windows retain
        # the desktop host's neutral clear colour.
        test_window(WORK / 'widgets.exe', 'Kryon Windows Text Button', widgets=True,
                    background=(255, 255, 255))
        test_window(WORK / 'frame.exe', 'windows_desktop_frame', size=(960, 600),
                    text=True, background=(255, 255, 255))
        test_window(WORK / 'services.exe', 'Kryon Windows Services', services=True)
        if zx_linked:
            test_window(WORK / 'window-zx.exe', 'Window')
            test_window(WORK / 'hello-zx.exe', 'Hello', text=True,
                        background=(255, 255, 255))
        for form in ('source', 'saved'):
            run(['wine', WORK / ('frame-snapshot-' + form + '.exe')], timeout=25)
        print('PASS Wine source/saved owned frame snapshots and camera fade', flush=True)
        test_dialog()
        run(['wine', WORK / 'images.exe'], timeout=25)
        print('PASS Wine PNG/JPEG/WebP straight RGBA and invalid images', flush=True)
        run(['wine', WORK / 'assets.exe'], timeout=25)
        left = sorted((WORK / 'wine-prefix/drive_c/users').rglob('kry*.tmp'))
        assert not left, f'asset files stayed after the program ended: {left}'
        print('PASS Wine text in a font given as bytes, a picture given as bytes, Winsock connect', flush=True)
        ENV.update(SDL_AUDIODRIVER='disk', SDL_DISKAUDIOFILE='Z:' + str(WORK / 'audio.raw'), SDL_DISKAUDIODELAY='10')
        (WORK / 'audio.raw').unlink(missing_ok=True)
        run(['wine', WORK / 'audio.exe'], timeout=25)
        with (WORK / 'audio.raw').open('rb') as stream:
            samples = struct.unpack('<' + 'f' * (min(1048576, (WORK / 'audio.raw').stat().st_size) // 4), stream.read(1048576))
        assert any(v > .2 for v in samples) and any(v < -.2 for v in samples), 'SDL/media audio did not mix PCM'
        print('PASS Wine SDL sink and media_audio_desktop PCM', flush=True)
    finally:
        subprocess.run([server, '-k'], env=ENV, check=True)
        subprocess.run([server, '-w'], env=ENV, check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--private', action='store_true')
    parser.add_argument('--zx-linked', action='store_true')
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    if args.private:
        private(args.zx_linked)
        return
    sdk_artifact()
    from PIL import Image
    fixtures = WORK / 'fixtures'
    fixtures.mkdir(exist_ok=True)
    rgba = Image.new('RGBA', (2, 1))
    rgba.putdata([(173, 61, 219, 97), (19, 203, 41, 255)])
    rgba.save(fixtures / 'rgba.png')
    rgba.save(fixtures / 'rgba.webp', lossless=True)
    Image.new('RGB', (3, 2), (20, 80, 140)).save(fixtures / 'photo.jpg', quality=100)
    (fixtures / 'invalid.png').write_bytes(b'not an image')
    for path in (WORK / 'sdk/bin').glob('*.dll'):
        shutil.copyfile(path, WORK / path.name)
    compile_source(ROOT / 'examples/window.tsx', 'window')
    compile_source(ROOT / 'tests/windows_desktop_widgets.zi', 'widgets')
    compile_source(ROOT / 'tests/windows_desktop_frame.zi', 'frame', options=[
        '--module-path', ROOT / 'tests'])
    compile_source(ROOT / 'tests/windows_desktop_audio.zi', 'audio')
    compile_source(ROOT / 'tests/windows_desktop_services.zi', 'services')
    compile_source(ROOT / 'tests/windows_file_dialog.zi', 'dialog')
    compile_source(ROOT / 'tests/windows_desktop_images.zi', 'images')
    compile_source(ROOT / 'tests/windows_desktop_assets.zi', 'assets', libraries=['-lws2_32'])
    snapshot = ROOT / 'tests/desktop_frame_texture_test.zi'
    saved = WORK / 'frame-snapshot-ir'
    run([ZIRAN, 'ir', '--project', '--root', snapshot.parent,
         '--target-os', 'windows', '--target-cpu', 'x64',
         '--define', '_WIN32', '--define', 'PLATFORM_DESKTOP', '-o', saved, snapshot])
    compile_source(snapshot, 'frame-snapshot-source')
    compile_source(saved / 'desktop_frame_texture_test.zir', 'frame-snapshot-saved')
    zx_source = ROOT / 'src/backend/zx_window_host.zi'
    zx_linked = False
    if zx_source.is_file():
        compile_source(zx_source, 'kryon-desktop', library=True)
        zx_linked = check_zx_host()
    else:
        print('Windows ZX host: this revision has no external desktop-player source', flush=True)
    run(['xvfb-run', '-a', '-n', '400', '-s', '-screen 0 1024x768x24',
         sys.executable, __file__, '--private', *(['--zx-linked'] if zx_linked else [])], timeout=240)


if __name__ == '__main__':
    main()

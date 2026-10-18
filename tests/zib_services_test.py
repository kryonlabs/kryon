"""Run one service-bearing ZIB on native and browser players, with local HTTP."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import argparse
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import threading
import time
import wave

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from raylib_project_test import png_pixels, rgb

parser = argparse.ArgumentParser()
parser.add_argument('--runner', choices=['wine'])
parser.add_argument('--player', type=Path, default=ROOT / 'build/zib-raylib/zib')
parser.add_argument('--skip-browser', action='store_true')
parser.add_argument('--android-serial', help='Only the disposable KryonZib emulator; never a physical device')
args = parser.parse_args()
if args.android_serial and (args.runner or args.skip_browser):
    parser.error('--android-serial selects Android instead of native/browser execution')

output = ROOT / 'build/scratch/zib-services'
project = output / 'project'
(project / 'src').mkdir(parents=True, exist_ok=True)
(project / 'assets').mkdir(exist_ok=True)
(project / 'src/app.zi').write_text((ROOT / 'tests/zib/services.zi').read_text())
tone = project / 'assets/tone.wav'
with wave.open(str(tone), 'wb') as sound:
    sound.setnchannels(1)
    sound.setsampwidth(2)
    sound.setframerate(8000)
    sound.writeframes(b''.join(struct.pack('<h', int(1500 * math.sin(i * math.tau * 220 / 8000)))
                               for i in range(24000)))


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        found = self.path in ('/text', '/tone.wav')
        self.send_response(200 if found else 404)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(tone.read_bytes() if self.path == '/tone.wav' else b'portable response')

    def log_message(self, *_):
        pass


server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
base = f'http://127.0.0.1:{server.server_port}'
(project / 'src/service_urls.zi').write_text('ServiceKeepOpen :: () -> bool { return ' + str(bool(args.android_serial)).lower() + ' }\n' + ''.join(
    f'{name} :: {json.dumps(base + suffix)};\n'
    for name, suffix in [('TEXT_URL', '/text'), ('AUDIO_URL', '/tone.wav'), ('MISSING_URL', '/missing')]))
(project / 'ziran.toml').write_text('''[package]
name = "service_probe"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon.profiles.portable]
backend = "desktop"
codegen = "zib"
assets = "assets"
''')
(project / 'ziran.local.toml').write_text(
    '[overrides]\n' + ''.join(f'{name} = {json.dumps(str(path))}\n'
                             for name, path in [('kryon', ROOT), ('ziran', ZIRAN_ROOT)]))
env = os.environ.copy()
for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER'):
    env.pop(key, None)
env.update(YUE_DESKTOP_RECOVERY='0', ZIRAN_ROOT=str(ZIRAN_ROOT))
env['PATH'] = str(ROOT / 'build/bin') + os.pathsep + env['PATH']
(output / 'asound.conf').write_text('pcm.!default { type null }\nctl.!default { type hw card 0 }\n')
env['ALSA_CONFIG_PATH'] = str(output / 'asound.conf')
env['PULSE_SERVER'] = 'unix:/nonexistent-kryon-test-pulse'
env['WINEPREFIX'] = str(output / 'wine')
env['WINEDEBUG'] = '-all'


def android_services(bundle):
    assert args.android_serial.startswith('emulator-'), 'Physical devices are never test targets'
    adb = Path(env.get('ANDROID_HOME', str(Path.home() / 'Android/Sdk'))) / 'platform-tools/adb'
    command = [str(adb), '-s', args.android_serial]

    def run(*arguments):
        return subprocess.run([*command, *map(str, arguments)], env=env, check=True,
                              capture_output=True, timeout=40)

    assert run('shell', 'getprop', 'ro.kernel.qemu').stdout.strip() == b'1'
    assert run('emu', 'avd', 'name').stdout.splitlines()[0].strip() == b'KryonZib'
    package = 'com.kryon.zibtest'
    transport = '/data/local/tmp/kryon-services.zib'
    port = f'tcp:{server.server_port}'
    apk = ROOT / 'build/zib-android-test/droid/app/build/outputs/apk/debug/app-debug.apk'
    run('install', '-r', apk)
    try:
        run('shell', 'wm', 'size', '960x600')
        run('shell', 'wm', 'density', '160')
        run('reverse', port, port)
        run('push', bundle, transport)
        run('shell', 'am', 'start', '-W', '-n', package + '/.MainActivity')
        pid = run('shell', 'pidof', package).stdout.decode().strip()
        run('shell', 'am', 'start', '-W', '-n', package + '/.Zib',
            '-a', 'android.intent.action.VIEW', '-t', 'application/vnd.ziran.bundle',
            '-d', 'file://' + transport)
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            foreground = run('shell', 'dumpsys', 'activity', 'activities').stdout
            assert any(package.encode() in line for line in foreground.splitlines()
                       if b'mResumedActivity' in line or b'topResumedActivity' in line)
            image = output / 'android-services.png'
            image.write_bytes(run('exec-out', 'screencap', '-p').stdout)
            color = rgb(png_pixels(image), 160, 160)
            if color == (20, 180, 80):
                return
            if color == (220, 30, 40):
                break
            time.sleep(.2)
        diagnostic = run('logcat', '-d', '--pid=' + pid, '-v', 'brief').stdout.decode(errors='replace')
        (output / 'android.log').write_text(diagnostic)
        raise AssertionError('Android service fixture did not pass; see its scoped app log')
    finally:
        for arguments in [('reverse', '--remove', port), ('shell', 'rm', '-f', transport),
                          ('shell', 'wm', 'size', 'reset'), ('shell', 'wm', 'density', 'reset'),
                          ('shell', 'am', 'force-stop', package), ('uninstall', package)]:
            subprocess.run([*command, *arguments], env=env, capture_output=True, timeout=20)


try:
    for command in ([ZIRAN, 'lock', '--offline'],
                    [ZIRAN, 'tool', 'kryon', 'build', '--profile', 'portable']):
        subprocess.run(list(map(str, command)), cwd=project, env=env, check=True, timeout=300)
    bundle = project / 'build/service_probe-portable.zib'
    if args.android_serial:
        android_services(bundle)
    else:
        path = 'Z:' + str(bundle).replace('/', '\\') if args.runner else str(bundle)
        native = ([args.runner] if args.runner else []) + [str(args.player), path]
        result = subprocess.run(['xvfb-run', '-a', *native],
                                env=env, text=True, capture_output=True, timeout=60)
        (output / 'native.log').write_text(result.stdout + result.stderr)
        assert result.returncode == 0 and 'ZIB media, HTTP and clock passed' in result.stdout, result.stdout + result.stderr
    if not args.skip_browser and not args.android_serial:
        subprocess.run(['node', str(ROOT / 'tests/zib_canvas_browser.mjs'),
                        str(ROOT / 'build/zib-canvas/player.html'), 'services', str(bundle)],
                       env=env, check=True, timeout=90)
finally:
    server.shutdown()
    server.server_close()
platform = 'Android' if args.android_serial else ('Windows/Wine' if args.runner else 'native')
if not args.skip_browser and not args.android_serial:
    platform += '/browser'
print(platform + ': packed and HTTP audio, pause/resume/stop, HTTP success/failure, clock and resource rejection passed')

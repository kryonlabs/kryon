"""The reusable WebKit island: a staged page round-trip through the zx player.

Builds the webview probe bundle against this checkout, then runs it on a
private Xvfb display: the island child starts, maps its window into the
player, reports ready, answers a spin command with rendered frames, and is
reaped on close. Skipped where WebKitGTK is unavailable."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/test/webview-island'


def webkit_available():
    probe = subprocess.run(['/usr/bin/python3', '-c',
        "import gi; gi.require_version('WebKit2', '4.1');"
        "import gi.repository.WebKit2"], capture_output=True)
    return probe.returncode == 0


def children(pid):
    try:
        return [int(value) for value in
                Path(f'/proc/{pid}/task/{pid}/children').read_text().split()]
    except (FileNotFoundError, ProcessLookupError):
        return []


def island_pid(player_pid):
    for pid in children(player_pid):
        try:
            if Path(f'/proc/{pid}/comm').read_text().strip() == 'python3':
                return pid
        except (FileNotFoundError, ProcessLookupError):
            pass
    return None


def wait_until(predicate, timeout, player=None, log=None):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = predicate()
        if result:
            return result
        if player is not None and player.poll() is not None:
            raise AssertionError(
                f'Player exited early with {player.returncode}:\n{log_text(log)}')
        time.sleep(0.1)
    raise AssertionError(f'Timed out after {timeout}s:\n{log_text(log)}')


def log_text(path):
    try:
        return path.read_text(errors='replace')
    except FileNotFoundError:
        return ''


def drive(bundle, player):
    assert os.environ.get('KRYON_PRIVATE_XVFB') == '1'
    assert not os.environ.get('WAYLAND_DISPLAY')
    WORK.mkdir(parents=True, exist_ok=True)
    home = WORK / 'home'
    home.mkdir(exist_ok=True)
    log = WORK / 'probe.log'
    env = dict(os.environ,
        HOME=str(home), XDG_CONFIG_HOME=str(WORK / 'config'),
        XDG_CACHE_HOME=str(WORK / 'cache'),
        LIBGL_ALWAYS_SOFTWARE='1', GALLIUM_DRIVER='llvmpipe',
        WEBKIT_DISABLE_DMABUF_RENDERER='1', YUE_DESKTOP_RECOVERY='0')
    with log.open('w') as stream:
        player_process = subprocess.Popen([str(player), str(bundle)],
            stdout=stream, stderr=subprocess.STDOUT, env=env, cwd=str(WORK))
        try:
            island = wait_until(lambda: island_pid(player_process.pid),
                25, player_process, log)
            arguments = Path(f'/proc/{island}/cmdline').read_bytes()
            assert b'kryon-island.py' in arguments, 'The island child is not the staged script'
            window = wait_until(lambda: subprocess.run(
                    ['xdotool', 'search', '--onlyvisible', '--name', 'Kryon webview island'],
                    capture_output=True, text=True, env=env).stdout.strip() or None,
                45, player_process, log)
            wait_until(lambda: 'probe: webview island passed' in log_text(log),
                90, player_process, log)
            text = log_text(log)
            assert 'probe: island failed to open' not in text, text
            assert 'probe: island reported an error' not in text, text
            assert 'probe: island timed out' not in text, text
            frames = [int(match) for match in
                      re.findall(r'"frames":(\d+)', text)]
            assert frames and max(frames) > 0, \
                f'The ack state reported no rendered frames: {text}'
            ready = [json.loads(line.split('probe: event ', 1)[1])
                     for line in text.splitlines() if 'probe: event ' in line]
            kinds = {event.get('type') for event in ready}
            assert 'ready' in kinds and 'ack' in kinds, kinds
            spin = [event for event in ready
                    if event.get('command') == 'spin']
            assert spin and spin[0].get('state', {}).get('mode') == 'scene', spin
            assert spin[0]['state'].get('speed') == 3, spin
            code = player_process.wait(timeout=30)
            assert code == 0, f'Probe exited {code}: {log_text(log)}'
            assert island_pid(player_process.pid) is None and not children(player_process.pid), \
                'The island child leaked after close'
            assert not Path(f'/proc/{island}').exists(), 'The island child leaked'
            print('Webview island: staged scene ready, spin ack with rendered '
                  f'frames, window {window}, child reaped: passed', flush=True)
            (WORK / 'receipt.json').write_text(json.dumps(dict(
                state='verified', island_protocol=1, page='scene',
                frames=max(frames), checks=['child spawn', 'island window',
                    'ready event', 'spin ack', 'reaped on close']), indent=2))
        finally:
            if player_process.poll() is None:
                player_process.terminate()
                try:
                    player_process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    player_process.kill()
                    player_process.wait(timeout=10)
    return 0


def build_probe():
    from toolchain import ZIRAN, fixture_identity, fixture_overrides
    shutil.rmtree(WORK, ignore_errors=True)
    project = WORK / 'probe'
    (project / 'src').mkdir(parents=True)
    shutil.copyfile(ROOT / 'tests/webview_probe.zi', project / 'src/app.zi')
    (project / 'ziran.toml').write_text(
        '[package]\nname = "webview_probe"\nentry = "src/app.zi"\n'
        'module_roots = ["src"]\nbridge_modules = ["app"]\ntool = "kryon"\n'
        + fixture_identity()
        + '[tool.kryon]\ndefault_profile = "portable"\n'
          '[tool.kryon.profiles.portable]\nbackend = "desktop"\ncodegen = "zx"\n')
    (project / 'ziran.local.toml').write_text(fixture_overrides())
    def run(args, ok=True):
        result = subprocess.run([str(value) for value in args], cwd=project,
                                capture_output=True, text=True)
        if ok:
            assert result.returncode == 0, result.stdout + result.stderr
        return result
    run([ZIRAN, 'lock'])
    run([ZIRAN, 'tool', 'kryon', 'build', 'portable'])
    bundle = project / 'build/webview_probe-portable.zx'
    assert bundle.is_file() and bundle.stat().st_size > 0, 'The probe built no bundle'
    player = ROOT / 'build/bin/zx'
    assert player.is_file(), 'Build the zx player first (make zx-player)'
    staged = player.parent / 'webview-assets/scene.html'
    assert staged.is_file(), 'Stage the island assets first (make zx-player)'
    return bundle, player


def main():
    if len(sys.argv) > 1 and sys.argv[1] == '--drive':
        return drive(Path(sys.argv[2]), Path(sys.argv[3]))
    if not webkit_available():
        print('WebKitGTK 4.1 is unavailable: skipping the island test', flush=True)
        return 0
    bundle, player = build_probe()
    env = os.environ.copy()
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY',
                'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'GDK_DISPLAY',
                'ICEAUTHORITY'):
        env.pop(key, None)
    env['KRYON_PRIVATE_XVFB'] = '1'
    subprocess.run(['dbus-run-session', '--', 'xvfb-run', '-a', '-s',
                    '-screen 0 1024x768x24 -nolisten tcp',
                    sys.executable, str(Path(__file__).resolve()),
                    '--drive', str(bundle), str(player)],
                   env=env, check=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())

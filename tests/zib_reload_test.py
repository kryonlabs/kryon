"""Replace a verified local pointer without replacing its private-display host."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

from toolchain import ROOT, ZIRAN

OUTPUT = ROOT / 'build/zib-reload-test'


def run(args, **kwargs):
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def wait_for(predicate):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError('private player did not reach expected state')


def publish(name, sequence):
    data = (OUTPUT / (name + '.zib')).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    (OUTPUT / (digest + '.zib')).write_bytes(data)
    pointer = OUTPUT / 'current.json'
    temporary = pointer.with_suffix('.next')
    temporary.write_text(json.dumps(dict(sequence=sequence, sha256=digest)))
    temporary.replace(pointer)


def private_probe():
    log_path = OUTPUT / 'player.log'
    with log_path.open('w') as log:
        player = subprocess.Popen(['stdbuf', '-oL', str(ROOT / 'build/bin/zib'),
                                   '--watch-current', str(OUTPUT / 'current.json')],
                                  stdout=log, stderr=log)
        try:
            wait_for(lambda: 'module one' in log_path.read_text())
            window = subprocess.check_output(['xdotool', 'search', '--pid', str(player.pid)], text=True).strip()
            first_assets = Path(os.readlink(f'/proc/{player.pid}/cwd'))
            assert (first_assets / 'assets/version.txt').read_text() == 'one'
            publish('broken', 2)
            time.sleep(1.3)
            assert player.poll() is None and 'reloaded release 2' not in log_path.read_text()
            publish('unsupported', 3)
            time.sleep(1.3)
            assert player.poll() is None and 'reloaded release 3' not in log_path.read_text()
            publish('two', 4)
            wait_for(lambda: 'module two' in log_path.read_text())
            assert player.poll() is None
            assert subprocess.check_output(['xdotool', 'search', '--pid', str(player.pid)], text=True).strip() == window
            second_assets = Path(os.readlink(f'/proc/{player.pid}/cwd'))
            assert second_assets != first_assets and not first_assets.exists()
            assert (second_assets / 'assets/version.txt').read_text() == 'two'
            publish('one', 1)
            time.sleep(1.3)
            assert log_path.read_text().count('module one') == 1, 'rollback reinitialized old code'
            (OUTPUT / 'current.json').write_text('{"sequence":5,"sha256":"../../escape"}')
            time.sleep(1.3)
            assert player.poll() is None and second_assets.exists()
            print('ZIB reload retains process/window, replaces assets and rejects malformed, unsupported and rollback releases')
        finally:
            player.terminate()
            player.wait(timeout=10)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    environment = {k: v for k, v in os.environ.items() if k not in
                   ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS')}
    environment['YUE_DESKTOP_RECOVERY'] = '0'
    for name in ('one', 'two'):
        source = OUTPUT / (name + '.zi')
        source.write_text('started: bool;\n#program_export\nmain :: () -> s32 {\n'
                          f' if !started {{ print("module {name}\\n"); started = true }}\n return 0\n}}\n')
        assets = OUTPUT / ('assets-' + name)
        assets.mkdir(exist_ok=True)
        (assets / 'version.txt').write_text(name)
        run([ZIRAN, 'bundle', '--root', OUTPUT, '--entry', name + ':main',
             '--asset-dir', 'assets=' + str(assets), '-o', OUTPUT / (name + '.zib'), source], env=environment)
    (OUTPUT / 'broken.zib').write_bytes(b'ZIB\0')
    unsupported = OUTPUT / 'unsupported.zi'
    unsupported.write_text('host_api :: #system_library "host_api";\n'
                           'Missing :: () -> s32 #foreign host_api;\n'
                           '#program_export\nmain :: () -> s32 { return Missing() }\n')
    run([ZIRAN, 'bundle', '--root', OUTPUT, '--entry', 'unsupported:main',
         '-o', OUTPUT / 'unsupported.zib', unsupported], env=environment)
    publish('one', 1)
    run(['xvfb-run', '-a', sys.executable, __file__, '--private-probe'], env=environment, cwd=ROOT)


if __name__ == '__main__':
    if '--private-probe' in sys.argv:
        private_probe()
    else:
        main()

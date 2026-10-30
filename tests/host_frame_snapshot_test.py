#!/usr/bin/env python3
"""Real standard host frame callbacks on private SDL/Xvfb and browser Canvas."""
import argparse
import fcntl
import os
from pathlib import Path
import shlex
import shutil
import subprocess
from toolchain import ZIRAN_ROOT, ZIRAN

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/host-frame-snapshots'
ENV = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY',
             'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_SESSION',
             'XDG_SESSION_ID', 'XDG_SESSION_TYPE', 'KRYON_CAPTURE_PATH', 'KRYON_TEST_DRIVER'):
    ENV.pop(name, None)
ENV['YUE_DESKTOP_RECOVERY'] = '0'
ENV['EMCC_CORES'] = '1'
ENV['EM_CACHE'] = str(ROOT / 'build/emscripten-cache')

def run(args, **kwargs):
    print(shlex.join(map(str, [*args[:7], "..."] if len(args) > 25 else args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, env=ENV, check=True, **kwargs)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('platform', choices=('desktop', 'canvas'))
    args = parser.parse_args()
    run(['git', 'check-ignore', 'work/host-frame-snapshots/test'])
    WORK.mkdir(parents=True, exist_ok=True)
    lock = (WORK / 'lock').open('w')
    fcntl.flock(lock, fcntl.LOCK_EX)
    common = ['--root', ROOT / 'tests', '--module-path', ROOT / 'tests/host_frame_snapshot',
              '--module-path', ROOT / 'src/ui', '--module-path', ROOT / 'src/backend',
              '--module-path', ZIRAN_ROOT / 'std']
    define = ['--define', 'PLATFORM_WEB'] if args.platform == 'canvas' else []
    ir = WORK / (args.platform + '-ir')
    gen = WORK / (args.platform + '-c')
    run([ZIRAN, 'ir', *common, *define, '-o', ir, ROOT / 'tests/host_frame_snapshot_test.zi'])
    for form in ('source', 'saved'):
        print(args.platform + ' ' + form, flush=True)
        source = ROOT / 'tests/host_frame_snapshot_test.zi'
        paths = common if form == 'source' else ['--root', ir]
        input_file = source if form == 'source' else ir / 'host_frame_snapshot_test.zir'
        run([ZIRAN, 'build', '--target=c', *paths, *define,
             '--entry', 'host_frame_snapshot_test:main', '-o', gen, input_file])
        if args.platform == 'desktop':
            libs = subprocess.check_output(['pkg-config', '--libs', 'sdl2', 'cairo', 'freetype2',
                                            'fontconfig', 'pangocairo', 'gio-2.0'], text=True).split()
            run([os.environ.get('CC', 'cc'), '-O1', '-I' + str(ZIRAN_ROOT / 'include'),
                 '-I' + str(gen), *sorted(gen.glob('*.c')), *libs, '-lm', '-o', gen / 'test'])
            run(['timeout', '--kill-after=2s', '45s', 'xvfb-run', '-a', '-n', '390',
                 '-s', '-screen 0 640x480x24', gen / 'test'])
        else:
            shell = WORK / 'shell.html'
            shell.write_text('''<!doctype html><meta charset="utf-8"><body><div id="canvas-frame" style="width:160px;height:120px"><canvas id="canvas" width="160" height="120"></canvas></div><pre id="result">pending</pre><script>var Module={onExit:function(code){code=globalThis.hostFrameSnapshotResult??code;document.getElementById('result').textContent=code?'FAIL '+code:'PASS'},onAbort:function(reason){document.getElementById('result').textContent='ABORT '+reason}};</script>{{{ SCRIPT }}}</body>''')
            run([os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc')),
                 '-O1', '-I' + str(ZIRAN_ROOT / 'include'), '-I' + str(gen),
                 *sorted(gen.glob('*.c')), '--js-library', ZIRAN_ROOT / 'web/ziran_web.js',
                 '-sASYNCIFY', '-sSINGLE_FILE=1', '-sEXIT_RUNTIME=1', '-sENVIRONMENT=web',
                 '--shell-file', shell, '-o', gen / 'test.html'])
            shutil.rmtree(WORK / 'browser-profile', ignore_errors=True)
            run(['node', ROOT / 'tests/canvas_backend_browser.mjs', (gen / 'test.html').as_uri(),
                 WORK / 'browser-profile'], timeout=40)
    print(args.platform + ': actual frame snapshot, legacy queues, hosted guard and cleanup passed')

if __name__ == '__main__':
    main()

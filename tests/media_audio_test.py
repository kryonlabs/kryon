#!/usr/bin/env python3
"""Owned asset audio: real native decoder/device, source + saved IR, hosted VM."""
import argparse
import json
import os
import shutil
import fcntl
from pathlib import Path
import shlex
import subprocess
import wave
from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/game-asset-audio'
ENV = dict(os.environ)
for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'GDK_DISPLAY',
             'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'DESKTOP_SESSION',
             'XDG_SESSION_ID', 'XDG_SESSION_TYPE', 'PIPEWIRE_REMOTE', 'PIPEWIRE_RUNTIME_DIR'):
    ENV.pop(name, None)
ENV.update(YUE_DESKTOP_RECOVERY='0', SDL_AUDIODRIVER='dummy',
           PULSE_SERVER='unix:' + str(WORK / 'no-pulse'),
           JACK_DEFAULT_SERVER='kryon-test-no-server', JACK_NO_START_SERVER='1',
           ALSA_CONFIG_PATH=str(WORK / 'alsa.conf'))

def run(args, env=None, cwd=ROOT):
    print(shlex.join(map(str, [*args[:8], "..."] if len(args) > 25 else args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, env=env or ENV, check=True)

def browser(common):
    source = ROOT / 'tests/media_audio_browser_test.zi'
    gen = WORK / 'browser-c'
    run([ZIRAN, 'build', '--target=c', *common, '--define', 'PLATFORM_WEB',
         '--entry', 'media_audio_browser_test:main', '-o', gen, source])
    shell = WORK / 'shell.html'
    shell.write_text('<!doctype html><meta charset="utf-8"><body><button style="width:80px;height:60px">Enable audio</button><pre id="result">pending</pre><script>var Module={onExit:function(code){document.getElementById("result").textContent=code?"FAIL "+code:"PASS"},onAbort:function(reason){document.getElementById("result").textContent="ABORT "+reason}};</script>{{{ SCRIPT }}}</body>')
    env = dict(ENV, EMCC_CORES='1', EM_CACHE=str(ROOT / 'build/emscripten-cache'))
    run([os.environ.get('EMCC', str(Path.home() / 'emsdk/upstream/emscripten/emcc')),
         '-O1', '-I' + str(ZIRAN_ROOT / 'include'), '-I' + str(gen), *sorted(gen.glob('*.c')),
         '--js-library', ZIRAN_ROOT / 'web/ziran_web.js', '-sASYNCIFY', '-sSINGLE_FILE=1',
         '-sEXIT_RUNTIME=1', '-sENVIRONMENT=web', '--embed-file', str(WORK / 'tone.wav') + '@tone.wav',
         '--shell-file', shell, '-o', gen / 'test.html'], env=env)
    shutil.rmtree(WORK / 'browser-profile', ignore_errors=True)
    run(['node', ROOT / 'tests/media_audio_browser_test.mjs', (gen / 'test.html').as_uri(), WORK / 'browser-profile'])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--browser', action='store_true')
    options = parser.parse_args()
    run(['git', 'check-ignore', 'work/game-asset-audio/tone.wav'])
    WORK.mkdir(parents=True, exist_ok=True)
    lock = (WORK / 'lock').open('w')
    fcntl.flock(lock, fcntl.LOCK_EX)
    # Miniaudio uses ALSA, not SDL. A private ALSA configuration exposes only
    # the null PCM; Pulse/JACK destinations are also private and unreachable.
    (WORK / 'alsa.conf').write_text('pcm.!default { type null }\nctl.!default { type null }\n')
    with wave.open(str(WORK / 'tone.wav'), 'wb') as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(8000)
        output.writeframes(b'\0\0' * 8000 * 2)
    with wave.open(str(WORK / 'short.wav'), 'wb') as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(8000)
        output.writeframes(b'\0\0' * 160)
    common = ['--root', ROOT / 'tests', '--module-path', ROOT / 'src/ui',
              '--module-path', ROOT / 'src/backend', '--module-path', ZIRAN_ROOT / 'std']
    ir = WORK / 'ir'
    gen = WORK / 'c'
    run(['python3', ROOT / 'scripts/generate-widget-codecs.py',
         '--source', ROOT / 'src/ui/media_audio.zi', '--root', 'MediaAudioCommand',
         '--root', 'MediaAudioResult', '--import', '"std/bundle_host"',
         '--import', '"std/byte_text_linux"', '--import', '"hosted_values"',
         '--import', '"media_audio"', '--output', ROOT / 'src/backend/media_audio_codec.zi', '--check'])
    source = ROOT / 'tests/media_audio_test.zi'
    run([ZIRAN, 'ir', *common, '-o', ir, source])
    for form in ('source', 'saved'):
        print('native ' + form, flush=True)
        paths = common if form == 'source' else ['--root', ir]
        file = source if form == 'source' else ir / 'media_audio_test.zir'
        run([ZIRAN, 'build', '--target=c', *paths, '--entry', 'media_audio_test:main', '-o', gen, file])
        libs = subprocess.check_output(['pkg-config', '--libs', 'sdl2', 'libdrm', 'gbm', 'egl', 'glesv2'], text=True).split()
        run([os.environ.get('CC', 'cc'), '-O1', '-ffunction-sections', '-fdata-sections',
             '-Wl,--gc-sections', '-I' + str(ZIRAN_ROOT / 'include'), '-I' + str(gen),
             *sorted(gen.glob('*.c')), Path(os.environ.get('RAYLIB_A', ROOT / 'build/raylib-ziran/libraylib.a')),
             Path(os.environ.get('ZIRAN_BUILD_DIR', ZIRAN_ROOT / 'build')) / 'libziran.a', *libs, '-ldl', '-lpthread', '-lm', '-o', gen / 'test'])
        run(['timeout', '--kill-after=2s', '25s', gen / 'test'])
    print('native asset audio source/saved: passed', flush=True)
    unavailable = WORK / 'alsa-unavailable.conf'
    unavailable.write_text('pcm.!default { type kryon_test_no_driver }\n')
    run([ZIRAN, 'build', '--target=c', *common, '--define', 'AUDIO_DEVICE_UNAVAILABLE_TEST',
         '--entry', 'media_audio_test:main', '-o', gen, ROOT / 'tests/media_audio_test.zi'])
    run([os.environ.get('CC', 'cc'), '-O1', '-ffunction-sections', '-fdata-sections',
         '-Wl,--gc-sections', '-I' + str(ZIRAN_ROOT / 'include'), '-I' + str(gen),
         *sorted(gen.glob('*.c')), Path(os.environ.get('RAYLIB_A', ROOT / 'build/raylib-ziran/libraylib.a')),
         Path(os.environ.get('ZIRAN_BUILD_DIR', ZIRAN_ROOT / 'build')) / 'libziran.a', *libs,
         '-ldl', '-lpthread', '-lm', '-o', gen / 'test'])
    run(['timeout', '--kill-after=2s', '25s', gen / 'test'], env=dict(ENV, ALSA_CONFIG_PATH=str(unavailable)))
    print('actual native device failure: unavailable passed', flush=True)
    assets = WORK / 'assets/test'
    assets.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(WORK / 'tone.wav', assets / 'tone.wav')
    # This is a tiny external consumer fixture, not a source checkout. All
    # dependency implementation still comes from the canonical repositories.
    consumer = WORK / 'consumer'
    (consumer / 'src').mkdir(parents=True, exist_ok=True)
    guest = consumer / 'src/media_audio_guest.zi'
    shutil.copyfile(ROOT / 'tests/media_audio_guest.zi', guest)
    (consumer / 'ziran.toml').write_text('[package]\nname = "audio_consumer"\nmodule_roots = ["src"]\n[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\nref = "master"\n[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\nref = "master"\n')
    raylib = subprocess.check_output([str(ZIRAN), 'pkg', 'path', 'raylib'], cwd=ROOT, text=True).strip()
    (consumer / 'ziran.local.toml').write_text('[overrides]\n' + ''.join(
        name + ' = ' + json.dumps(str(path)) + '\n'
        for name, path in [('kryon', ROOT), ('ziran', ZIRAN_ROOT), ('raylib', raylib)]))
    run([ZIRAN, 'lock', '--offline'], cwd=consumer)
    host = ROOT / 'tests/media_audio_host_test.zi'
    for fake in (False, True):
        define = ['--define', 'AUDIO_UNAVAILABLE_TEST'] if fake else []
        run([ZIRAN, 'ir', '--project', '--offline', '--root', consumer / 'src', '--define', 'KRYON_HOSTED_UI', *define, '-o', WORK / 'guest-ir', guest], cwd=consumer)
        for form in ('source', 'saved'):
            paths = ['--project', '--offline', '--root', consumer / 'src'] if form == 'source' else ['--root', WORK / 'guest-ir']
            source = guest if form == 'source' else WORK / 'guest-ir/media_audio_guest.zir'
            run([ZIRAN, 'bundle', *paths, '--define', 'KRYON_HOSTED_UI', *define,
                 '--entry', 'media_audio_guest:main', '--asset-dir', 'assets=' + str(WORK / 'assets'),
                 '-o', WORK / 'guest.zx', source], cwd=consumer)
            run([ZIRAN, 'build', '--target=c', *common, *define,
                 '--entry', 'media_audio_host_test:main', '-o', WORK / 'host-c', host])
            host_gen = WORK / 'host-c'
            run([os.environ.get('CC', 'cc'), '-O1', '-ffunction-sections', '-fdata-sections',
                 '-Wl,--gc-sections', '-I' + str(ZIRAN_ROOT / 'include'), '-I' + str(host_gen),
                 *sorted(host_gen.glob('*.c')), Path(os.environ.get('RAYLIB_A', ROOT / 'build/raylib-ziran/libraylib.a')),
                 Path(os.environ.get('ZIRAN_BUILD_DIR', ZIRAN_ROOT / 'build')) / 'libziran.a', *libs, '-ldl', '-lpthread', '-lm', '-o', host_gen / 'test'])
            run(['timeout', '--kill-after=2s', '25s', host_gen / 'test'])
    print('hosted source/saved: real device + unavailable contract passed', flush=True)
    if options.browser:
        browser(common)



if __name__ == '__main__':
    main()

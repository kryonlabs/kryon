#!/usr/bin/env python3
"""Actual Opus transport with private null audio and a muted private browser."""
import argparse
import os
from pathlib import Path
import shutil
import shlex
import subprocess
from toolchain import ZIRAN, ZIRAN_ROOT, JS_RUNTIME
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/opus-audio'
AUDIO = Path(os.environ['AUDIO_PACKAGE']) if 'AUDIO_PACKAGE' in os.environ else Path(subprocess.check_output([str(ZIRAN), 'pkg', 'path', 'audio'], cwd=ROOT, text=True).strip())
ENV = dict(os.environ)
for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','GDK_DISPLAY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','DESKTOP_SESSION','XDG_SESSION_ID','XDG_SESSION_TYPE','PIPEWIRE_REMOTE','PIPEWIRE_RUNTIME_DIR','PULSE_SERVER','ALSA_CONFIG_PATH'):
    ENV.pop(key, None)
ENV.update(YUE_DESKTOP_RECOVERY='0',SDL_AUDIODRIVER='dummy',PULSE_SERVER='unix:'+str(WORK/'no-pulse'),JACK_DEFAULT_SERVER='kryon-test-no-server',JACK_NO_START_SERVER='1',ALSA_CONFIG_PATH=str(WORK/'alsa.conf'))
COMMON = ['--root',ROOT/'tests','--module-path',ROOT/'src/ui','--module-path',ROOT/'src/backend','--module-path','audio='+str(AUDIO/'src')]
FIXTURE = AUDIO/'build/tests/fixtures/encoded-1.opus'

def run(args, env=None):
    print(shlex.join(map(str,args[:8])),flush=True)
    subprocess.run(list(map(str,args)),cwd=ROOT,env=env or ENV,check=True)

def native():
    raylib=ROOT/'build/raylib-ziran/libraylib.a'
    source=raylib.parent/'source/raudio.c'
    run(['cc','-O1','-ffunction-sections','-fdata-sections','-DMA_ENABLE_ONLY_SPECIFIC_BACKENDS','-DMA_ENABLE_NULL','-I'+str(source.parent),'-c',source,'-o',WORK/'null-audio.o'])
    run([ZIRAN,'ir',*COMMON,'-o',WORK/'ir',ROOT/'tests/opus_audio_test.zi'])
    def compile_run(file, paths, module, output, arguments=()):
        gen=WORK/output
        run([ZIRAN,'build','--target=c',*paths,'--entry',module+':main','-o',gen,file])
        run(['cc','-O1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(ZIRAN_ROOT/'include'),'-I'+str(gen),*sorted(gen.glob('*.c')),WORK/'null-audio.o',raylib,ZIRAN_ROOT/'build/libziran.a','-lopus','-lm','-lpthread','-ldl','-o',gen/'test'])
        for args in arguments or [()]:
            run(['timeout','--kill-after=2s','20s',gen/'test',*args])
    for form in ('source','saved'):
        paths=COMMON if form=='source' else ['--root',WORK/'ir']
        file=ROOT/'tests/opus_audio_test.zi' if form=='source' else WORK/'ir/opus_audio_test.zir'
        compile_run(file,paths,'opus_audio_test','native',[(FIXTURE,AUDIO/'build/tests/fixtures/short.opus'),(AUDIO/'build/tests/fixtures/encoded-2.opus',AUDIO/'build/tests/fixtures/short.opus')])
    # Actual VM bridge: Opus content under the existing public guest asset name.
    assets=WORK/'assets/test'
    assets.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(FIXTURE,assets/'tone.wav')
    guest=ROOT/'tests/media_audio_guest.zi'
    guest_common=['--root',ROOT/'tests','--module-path','kryon='+str(ROOT/'src/ui'),'--module-path','kryon='+str(ROOT/'src/backend'),'--define','KRYON_HOSTED_UI']
    run([ZIRAN,'ir',*guest_common,'-o',WORK/'guest-ir',guest])
    for form in ('source','saved'):
        paths=guest_common if form=='source' else ['--root',WORK/'guest-ir','--define','KRYON_HOSTED_UI']
        file=guest if form=='source' else WORK/'guest-ir/media_audio_guest.zir'
        run([ZIRAN,'bundle',*paths,'--entry','media_audio_guest:main','--asset-dir','assets='+str(WORK/'assets'),'-o',WORK/'guest.zx',file])
        compile_run(ROOT/'tests/media_audio_host_test.zi',[*COMMON,'--define','OPUS_AUDIO_TEST'],'media_audio_host_test','host')
    print('native mono/stereo, short loops, source/saved and hosted ZX Opus playback passed',flush=True)

def browser():
    if Path(JS_RUNTIME).name != 'bun':
        raise RuntimeError('Set JS_RUNTIME to the Bun executable for browser test tooling')
    gen=WORK/'browser'
    run([ZIRAN,'build','--target=c',*COMMON,'--define','PLATFORM_WEB','--define','OPUS_AUDIO_TEST','--entry','media_audio_browser_test:main','-o',gen,ROOT/'tests/media_audio_browser_test.zi'])
    shell=WORK/'shell.html'
    shell.write_text('<!doctype html><meta charset="utf-8"><body><button style="width:80px;height:60px">Enable audio</button><pre id="result">pending</pre><script>var Module={onExit:function(code){document.getElementById("result").textContent=code?"FAIL "+code:"PASS"},onAbort:function(reason){document.getElementById("result").textContent="ABORT "+reason}};</script>{{{ SCRIPT }}}</body>')
    env=dict(ENV,EMCC_CORES='1',EM_CACHE=str(ROOT/'build/emscripten-cache'))
    run([os.environ.get('EMCC',str(Path.home()/'emsdk/upstream/emscripten/emcc')),'-O1','-I'+str(ZIRAN_ROOT/'include'),'-I'+str(gen),*sorted(gen.glob('*.c')),'--js-library',ZIRAN_ROOT/'web/ziran_web.js','-sASYNCIFY','-sSINGLE_FILE=1','-sEXIT_RUNTIME=1','-sENVIRONMENT=web','--embed-file',str(FIXTURE)+'@tone.opus','--shell-file',shell,'-o',gen/'test.html'],env=env)
    profile=WORK/'browser-profile'
    shutil.rmtree(profile,ignore_errors=True)
    run([JS_RUNTIME,ROOT/'tests/media_audio_browser_test.mjs',(gen/'test.html').as_uri(),profile])
    print('private browser Opus decode, autoplay, voices, pause and cleanup passed',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--browser',action='store_true')
    parser.add_argument('--browser-only',action='store_true')
    options=parser.parse_args()
    WORK.mkdir(parents=True,exist_ok=True)
    (WORK/'alsa.conf').write_text('pcm.!default { type null }\nctl.!default { type null }\n')
    if not options.browser_only:
        native()
    if options.browser or options.browser_only:
        browser()

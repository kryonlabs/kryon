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
    run([ZIRAN,'ir',*COMMON,'-o',WORK/'guard-ir',ROOT/'tests/opus_audio_guard_test.zi'])
    # An Ogg page whose first packet is a Vorbis identification header.
    page=b'OggS\0\2'+bytes(20)+bytes([1,30])+b'\x01vorbis'+bytes(23)
    (WORK/'other.ogg').write_bytes(page)
    for form in ('source','saved'):
        paths=COMMON if form=='source' else ['--root',WORK/'ir']
        file=ROOT/'tests/opus_audio_test.zi' if form=='source' else WORK/'ir/opus_audio_test.zir'
        compile_run(file,paths,'opus_audio_test','native',[(FIXTURE,AUDIO/'build/tests/fixtures/short.opus'),(AUDIO/'build/tests/fixtures/encoded-2.opus',AUDIO/'build/tests/fixtures/short.opus')])
        paths=COMMON if form=='source' else ['--root',WORK/'guard-ir']
        file=ROOT/'tests/opus_audio_guard_test.zi' if form=='source' else WORK/'guard-ir/opus_audio_guard_test.zir'
        compile_run(file,paths,'opus_audio_guard_test','guard',[(FIXTURE,WORK/'other.ogg')])
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
    print('production provider: native mono/stereo, short loops, content routing, source/saved and hosted ZX Opus playback passed',flush=True)

# raudio's standalone build declares its types through raudio.h, which this
# raylib revision no longer ships. Take them from raylib.h and rename the file
# helpers and log levels that raudio.c then defines for itself.
RAUDIO_HELPERS=('IsFileExtension','GetFileExtension','GetFileName','GetFileNameWithoutExt','LoadFileData','UnloadFileData','SaveFileData','SaveFileText','TraceLogLevel',
    'LOG_ALL','LOG_TRACE','LOG_DEBUG','LOG_INFO','LOG_WARNING','LOG_ERROR','LOG_FATAL','LOG_NONE')

def android(serial):
    """Production provider on an Android device with miniaudio's null backend."""
    ndk=Path(os.environ.get('ANDROID_NDK_HOME',Path.home()/'Android/Sdk/ndk/28.2.13676358'))
    tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin'
    adb=[shutil.which('adb') or str(Path.home()/'Android/Sdk/platform-tools/adb'),'-s',serial]
    abi=subprocess.check_output([*adb,'shell','getprop','ro.product.cpu.abi'],text=True).strip()
    triple,arch={'arm64-v8a':('aarch64-linux-android','__aarch64__'),'armeabi-v7a':('armv7a-linux-androideabi','__arm__'),
        'x86_64':('x86_64-linux-android','__x86_64__'),'x86':('i686-linux-android','__i386__')}[abi]
    out=WORK/'android'/abi
    package=lambda name: subprocess.check_output([str(ZIRAN),'pkg','path',name],cwd=ROOT,text=True).strip()
    # The audio package's own static codec recipe (native/AudioCodecs.cmake).
    run(['cmake','-S',AUDIO/'native','-B',out/'codecs','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),
         '-DANDROID_ABI='+abi,'-DANDROID_PLATFORM=android-21','-DCMAKE_BUILD_TYPE=Release',
         '-DOPUS_DIR='+package('libopus'),'-DSAMPLERATE_DIR='+package('libsamplerate')])
    run(['cmake','--build',out/'codecs','--target','opus','--parallel','2'])
    libopus=next((out/'codecs').rglob('libopus.a'))
    include=out/'include'
    include.mkdir(parents=True,exist_ok=True)
    (include/'raudio.h').write_text('/* Generated by tests/opus_audio_test.py */\n'+''.join(f'#define {name} KryonTest{name}\n' for name in RAUDIO_HELPERS)+
        '#include "raylib.h"\n'+''.join(f'#undef {name}\n' for name in RAUDIO_HELPERS))
    cc=tools/f'{triple}21-clang'
    source=ROOT/'build/raylib-ziran/source'
    run([cc,'-O2','-DRAUDIO_STANDALONE','-DSUPPORT_FILEFORMAT_WAV=1','-DSUPPORT_FILEFORMAT_OGG=1','-DMA_ENABLE_ONLY_SPECIFIC_BACKENDS','-DMA_ENABLE_NULL',
         '-I'+str(include),'-I'+str(source),'-c',source/'raudio.c','-o',out/'null-audio.o'])
    env=dict(ENV,CC=str(cc),CFLAGS='-O2 -Wno-parentheses-equality',LDLIBS=f'{out/"null-audio.o"} {libopus} -lm -ldl')
    programs=[]
    for name in ('opus_audio_test','opus_audio_guard_test'):
        run([ZIRAN,'build','--target=c','--exe','--define','ANDROID_BUILD','--define',arch,*COMMON,'--entry',name+':main','-o',out/name,ROOT/'tests'/(name+'.zi')],env=env)
        programs.append(next(path for path in (out/name).iterdir() if path.is_file() and os.access(path,os.X_OK) and path.suffix==''))
    remote='/data/local/tmp/kryon-opus-test'
    subprocess.run([*adb,'shell',f'rm -rf {remote} && mkdir -p {remote}'],check=True)
    fixtures=[FIXTURE,AUDIO/'build/tests/fixtures/encoded-2.opus',AUDIO/'build/tests/fixtures/short.opus',WORK/'other.ogg']
    for path in [*programs,*fixtures]:
        subprocess.run([*adb,'push',path,f'{remote}/{path.name}'],check=True,stdout=subprocess.DEVNULL)
    script=(f'cd {remote} && chmod 755 {programs[0].name} {programs[1].name} && ./{programs[0].name} encoded-1.opus short.opus && '
            f'./{programs[0].name} encoded-2.opus short.opus && ./{programs[1].name} encoded-1.opus other.ogg; status=$?; cd / && rm -rf {remote}; exit $status')
    print(shlex.join([*adb,'shell','...']),flush=True)
    result=subprocess.run([*adb,'shell',script],capture_output=True,text=True,timeout=120)
    print(''.join(line+'\n' for line in result.stdout.splitlines() if not line.startswith('INFO')),end='',flush=True)
    if result.returncode != 0:
        raise SystemExit(f'Android {abi} Opus playback failed with {result.returncode}: {result.stderr.strip()}')
    print(f'Android {abi} ({serial}): production Opus playback, loop, pause, completion and routing passed on null audio',flush=True)

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
    parser.add_argument('--android',metavar='SERIAL',help='Also run on this USB development device (null audio, no APK)')
    options=parser.parse_args()
    WORK.mkdir(parents=True,exist_ok=True)
    (WORK/'alsa.conf').write_text('pcm.!default { type null }\nctl.!default { type null }\n')
    if not options.browser_only:
        native()
    if options.android:
        android(options.android)
    if options.browser or options.browser_only:
        browser()

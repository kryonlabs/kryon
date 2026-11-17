#!/usr/bin/env python3
"""Actual Opus transport with private null audio and a muted private browser."""
import argparse
import os
from pathlib import Path
import shutil
import shlex
import sys
import subprocess
from toolchain import UI_MODULE_ARGS, ZIRAN, ZIRAN_ROOT, JS_RUNTIME, kryon_test_project
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/opus-audio'
AUDIO = Path(os.environ['AUDIO_PACKAGE']) if 'AUDIO_PACKAGE' in os.environ else Path(subprocess.check_output([str(ZIRAN), 'pkg', 'path', 'audio'], cwd=ROOT, text=True).strip())
ENV = dict(os.environ)
for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','GDK_DISPLAY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','DESKTOP_SESSION','XDG_SESSION_ID','XDG_SESSION_TYPE','PIPEWIRE_REMOTE','PIPEWIRE_RUNTIME_DIR','PULSE_SERVER','ALSA_CONFIG_PATH'):
    ENV.pop(key, None)
ENV.update(YUE_DESKTOP_RECOVERY='0',SDL_AUDIODRIVER='dummy',PULSE_SERVER='unix:'+str(WORK/'no-pulse'),JACK_DEFAULT_SERVER='kryon-test-no-server',JACK_NO_START_SERVER='1',ALSA_CONFIG_PATH=str(WORK/'alsa.conf'))
COMMON = ['--root',ROOT/'tests',*UI_MODULE_ARGS,'--module-path',ROOT/'src/backend','--module-path','audio='+str(AUDIO/'src')]
FIXTURE = AUDIO/'build/tests/fixtures/encoded-1.opus'

def run(args, env=None, directory=ROOT):
    print(shlex.join(map(str,args[:8])),flush=True)
    subprocess.run(list(map(str,args)),cwd=directory,env=env or ENV,check=True)

def native():
    run([ZIRAN,'ir',*COMMON,'-o',WORK/'ir',ROOT/'tests/opus_audio_test.zi'])
    def compile_run(file, paths, module, output, arguments=()):
        gen=WORK/output
        run([ZIRAN,'build','--target=c',*paths,'--entry',module+':main','-o',gen,file])
        run(['cc','-O1','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(ZIRAN_ROOT/'include'),'-I'+str(gen),*sorted(gen.glob('*.c')),*subprocess.check_output(['pkg-config','--libs','sdl2'],text=True).split(),ZIRAN_ROOT/'build/libziran.a','-lopus','-lm','-lpthread','-ldl','-o',gen/'test'])
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
    guest_project=WORK/'guest-project'
    kryon_test_project(guest_project)
    guest=guest_project/'src/media_audio_guest.zi'
    guest.write_bytes((ROOT/'tests/media_audio_guest.zi').read_bytes())
    guest_common=['--project','--root',guest.parent,'--define','KRYON_HOSTED_UI']
    run([ZIRAN,'ir',*guest_common,'-o',WORK/'guest-ir',guest],directory=guest_project)
    for form in ('source','saved'):
        paths=guest_common if form=='source' else ['--root',WORK/'guest-ir','--define','KRYON_HOSTED_UI']
        file=guest if form=='source' else WORK/'guest-ir/media_audio_guest.zir'
        run([ZIRAN,'bundle',*paths,'--entry','media_audio_guest:main','--asset-dir','assets='+str(WORK/'assets'),'-o',WORK/'guest.zx',file],directory=guest_project)
        compile_run(ROOT/'tests/media_audio_host_test.zi',[*COMMON,'--define','OPUS_AUDIO_TEST'],'media_audio_host_test','host')
    print('production provider: native mono/stereo, short loops, content routing, source/saved and hosted ZX Opus playback passed',flush=True)

def android(serial):
    env = dict(ENV, ANDROID_SERIAL=serial)
    run(['sh', ROOT/'tests/android_audio_test.sh'], env=env)


def windows():
    """Production provider cross-built with MinGW and run under a private Wine prefix."""
    cc=os.environ.get('WINDOWS_CC') or shutil.which('x86_64-w64-mingw32-gcc') or '/nix/store/p628y5p3mq9zyc5nd728f6k8m9dh28ym-x86_64-w64-mingw32-gcc-wrapper-15.3.0/bin/x86_64-w64-mingw32-gcc'
    compiler=Path(shutil.which(cc) or cc)
    tool=lambda name: str(compiler.with_name('x86_64-w64-mingw32-'+name))
    sys.path.insert(0,str(ROOT/'scripts'))
    from renderer_artifact import mingw_library_flags
    libraries=mingw_library_flags(str(compiler))
    out=WORK/'windows'
    package=lambda name: subprocess.check_output([str(ZIRAN),'pkg','path',name],cwd=ROOT,text=True).strip()
    # The package's recipe keeps libopus's stack protector, so links add libssp.
    run(['cmake','-S',AUDIO/'native','-B',out/'codecs','-DCMAKE_SYSTEM_NAME=Windows','-DCMAKE_SYSTEM_PROCESSOR=AMD64','-DCMAKE_C_COMPILER='+str(compiler),
         '-DCMAKE_AR='+tool('ar'),'-DCMAKE_RANLIB='+tool('ranlib'),'-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY','-DCMAKE_BUILD_TYPE=Release',
         '-DOPUS_DIR='+package('libopus'),'-DSAMPLERATE_DIR='+package('libsamplerate')])
    run(['cmake','--build',out/'codecs','--target','opus','--parallel','2'])
    libopus=next((out/'codecs').rglob('libopus.a'))
    sys.path.insert(0,str(ROOT/'scripts'))
    from windows_desktop import prepare
    sdk=prepare(ROOT/'build/windows-desktop')
    for dll in (sdk/'bin').glob('*.dll'):
        shutil.copyfile(dll,out/dll.name)
    programs=[]
    for name in ('opus_audio_test','opus_audio_guard_test'):
        gen=out/name
        run([ZIRAN,'build','--target=c','--define','_WIN32','--define','__x86_64__',*COMMON,'--entry',name+':main','-o',gen,ROOT/'tests'/(name+'.zi')])
        run([compiler,*libraries,'-O2','-std=c11','-ffunction-sections','-fdata-sections','-I'+str(ZIRAN_ROOT/'include'),'-I'+str(gen),*sorted(gen.glob('*.c')),
             '-L'+str(sdk/'lib'),'-lSDL2',libopus,'-lssp','-Wl,--gc-sections','-static','-static-libgcc','-lwinmm','-lole32','-lm','-o',out/(name+'.exe')])
        programs.append(out/(name+'.exe'))
    fixtures=[FIXTURE,AUDIO/'build/tests/fixtures/encoded-2.opus',AUDIO/'build/tests/fixtures/short.opus',WORK/'other.ogg']
    for path in fixtures:
        shutil.copyfile(path,out/path.name)
    wine=shutil.which('wine')
    server=shutil.which('wineserver') or next((str(path) for path in (Path(os.path.realpath(wine)).parent/'wineserver',
        Path('/usr/lib/x86_64-linux-gnu/wine/wineserver'),Path('/usr/lib/wine/wineserver')) if os.access(path,os.X_OK)),None)
    if not wine or not server:
        raise SystemExit('Windows Opus test requires wine and its wineserver')
    home=out/'home'
    for directory in ('config','data','cache','runtime','tmp'):
        (home/directory).mkdir(parents=True,exist_ok=True)
    (home/'runtime').chmod(0o700)
    env={key:value for key,value in ENV.items() if not key.startswith(('WINE','XDG_'))}
    env.update(HOME=str(home),WINEPREFIX=str(out/'prefix'),WINESERVER=server,WINEARCH='win64',WINEDEBUG='-all',
        XDG_CONFIG_HOME=str(home/'config'),XDG_DATA_HOME=str(home/'data'),XDG_CACHE_HOME=str(home/'cache'),XDG_RUNTIME_DIR=str(home/'runtime'))
    # Debian Wine 10 frees secure_getenv("TMPDIR") (Debian #1110936): keep it unset for Wine.
    env.pop('TMPDIR',None)
    script=('trap \'"$WINESERVER" -k; "$WINESERVER" -w\' EXIT; WINEDEBUG=err+all "$1" wineboot.exe --init || exit 90; '
        '"$1" ./opus_audio_test.exe encoded-1.opus short.opus && "$1" ./opus_audio_test.exe encoded-2.opus short.opus && '
        '"$1" ./opus_audio_guard_test.exe encoded-1.opus other.ogg')
    print('xvfb-run wine opus_audio_test.exe / opus_audio_guard_test.exe',flush=True)
    subprocess.run(['timeout','--kill-after=5s','300s','xvfb-run','-a','-s','-screen 0 640x480x24','sh','-c',script,'sh',wine],cwd=out,env=env,check=True)
    print('Windows x86_64 (MinGW, Wine): production Opus playback, loop, pause, completion and routing passed on null audio',flush=True)

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
    parser.add_argument('--android',metavar='SERIAL',help='Also run on this USB development device (the actual Android UI audio contract)')
    parser.add_argument('--windows',action='store_true',help='Also cross-build with MinGW and run under a private Wine prefix')
    options=parser.parse_args()
    WORK.mkdir(parents=True,exist_ok=True)
    (WORK/'alsa.conf').write_text('pcm.!default { type null }\nctl.!default { type null }\n')
    if not options.browser_only:
        native()
    if options.android:
        android(options.android)
    if options.windows:
        windows()
    if options.browser or options.browser_only:
        browser()

#!/usr/bin/env python3
"""Real native and IDBFS barriers, including portable AppLocal guests."""
import os
from pathlib import Path
import subprocess
import shutil
from toolchain import ZIRAN, ZIRAN_ROOT
ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/document-storage'
ENV = dict(os.environ, EMCC_CORES='1', CMAKE_BUILD_PARALLEL_LEVEL='1', ZIRAN_PAR_THREADS='1', MAKEFLAGS='-j1', YUE_DESKTOP_RECOVERY='0')
for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','DESKTOP_SESSION','XDG_SESSION_ID'):
    ENV.pop(key, None)
def run(args):
    subprocess.run(list(map(str,args)),cwd=ROOT,env=ENV,check=True)
def main():
    run(['git','check-ignore','work/document-storage/root.txt'])
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK/'data').mkdir(exist_ok=True)
    (WORK/'root.txt').write_text(str(WORK/'data'))
    emcc=os.environ.get('EMCC',str(Path.home()/'emsdk/upstream/emscripten/emcc'))
    ENV['EM_CACHE']=str(ROOT/'build/emscripten-cache')
    run(['make','-j1','-C',ZIRAN_ROOT,'BOOTSTRAP=1','BUILD_DIR='+str(WORK/'runtime'),'CC='+emcc,'AR='+str(Path(emcc).with_name('emar')),'OBJCOPY=true','CFLAGS=-O2',WORK/'runtime/libziran.a'])
    common=['--root',ROOT/'tests','--module-path',ROOT/'src/backend','--module-path',ROOT/'src/ui','--module-path',ZIRAN_ROOT/'std']
    guest=ROOT/'tests/document_storage_guest.zi'
    source=ROOT/'tests/document_storage_test.zi'
    run([ZIRAN,'ir',*common,'-o',WORK/'guest-ir',guest])
    for form in ('source','saved'):
        paths=common if form=='source' else ['--root',WORK/'guest-ir']
        file=guest if form=='source' else WORK/'guest-ir/document_storage_guest.zir'
        run([ZIRAN,'bundle',*paths,'--entry','document_storage_guest:main','-o',WORK/'guest.zx',file])
        for web in (False,True):
            gen=WORK/'c'
            shutil.rmtree(gen,ignore_errors=True)
            define=['--define','PLATFORM_WEB'] if web else []
            run([ZIRAN,'ir',*common,*define,'-o',WORK/'ir',source])
            paths=common if form=='source' else ['--root',WORK/'ir']
            file=source if form=='source' else WORK/'ir/document_storage_test.zir'
            run([ZIRAN,'build','--target=c',*paths,*define,'--entry','document_storage_test:main','-o',gen,file])
            flags=['-O1','-ffunction-sections','-fdata-sections','-I'+str(gen),'-I'+str(ZIRAN_ROOT/'include'),*sorted(gen.glob('*.c'))]
            if not web:
                run([os.environ.get('CC','cc'),*flags,ZIRAN_ROOT/'build/libziran.a','-Wl,--gc-sections','-lm','-lpthread','-o',WORK/'test'])
                run([WORK/'test'])
            else:
                ENV['EM_CACHE']=str(ROOT/'build/emscripten-cache')
                shell=WORK/'shell.html'
                shell.write_text('''<!doctype html><meta charset="utf-8"><pre id="result">pending</pre><script>
var storageTestMode=Number(new URLSearchParams(location.search).get('reload')||0);
var Module={preRun:[()=>{Date.now=()=>1700000000000; if(storageTestMode===2) indexedDB.open=()=>{throw new Error('initial IDB unavailable')};const sync=FS.syncfs; FS.syncfs=(p,cb)=>sync(p,e=>setTimeout(()=>cb(e),40));}],onExit:c=>document.getElementById('result').textContent=c?'FAIL '+c:'PASS',onAbort:e=>document.getElementById('result').textContent='ABORT '+e};
function storageTestFail(){FS.filesystems.IDBFS.dbs['/kryon-apps'].close()}
</script>{{{ SCRIPT }}}''')
                run([os.environ.get('EMCC',str(Path.home()/'emsdk/upstream/emscripten/emcc')),*flags,WORK/'runtime/libziran.a','--js-library',ZIRAN_ROOT/'web/ziran_web.js','-lidbfs.js','-sASYNCIFY','-sSTACK_SIZE=16777216','-sASYNCIFY_STACK_SIZE=1048576','-g2','-sASSERTIONS=1','-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=134217728','-sSINGLE_FILE=1','-sEXIT_RUNTIME=1','-sEXPORTED_RUNTIME_METHODS=FS','-sENVIRONMENT=web','--embed-file',str(WORK/'guest.zx')+'@work/document-storage/guest.zx','--shell-file',shell,'-o',WORK/'test.html'])
                shutil.rmtree(WORK/'browser-profile',ignore_errors=True)
                run(['node',ROOT/'tests/document_storage_test.mjs',(WORK/'test.html').as_uri(),WORK/'browser-profile'])
            print(f'document storage {form} {"browser IDBFS + hosted VM" if web else "native + hosted VM"}: passed',flush=True)
if __name__=='__main__': main()

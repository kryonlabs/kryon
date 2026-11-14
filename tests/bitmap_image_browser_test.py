#!/usr/bin/env python3
"""Actual Canvas/Wasm Image lifecycle in private headless Chromium, run by the configured JS_RUNTIME."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from toolchain import JS_RUNTIME, ZIRAN, ZIRAN_ROOT
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/scratch/bitmap-image-browser'
ENV={k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','GDK_DISPLAY','DESKTOP_SESSION')}
ENV.update(EMCC_CORES='1',YUE_DESKTOP_RECOVERY='0',LIBGL_ALWAYS_SOFTWARE='1',EM_CACHE=str(ROOT/'build/emscripten-cache'))
# A configured runtime also runs emscripten's own JavaScript.
if 'JS_RUNTIME' in os.environ:ENV['EM_NODE_JS']=JS_RUNTIME
EMCC=os.environ.get('EMCC',str(Path.home()/'emsdk/upstream/emscripten/emcc'))
def run(command):
    subprocess.run(list(map(str,command)),env=ENV,cwd=ROOT,check=True)
# Kryon's private headless Chromium driver; it polls the
# page's #result element and has no desktop connection.
def browser():
    profile=Path(tempfile.mkdtemp(prefix='chromium-',dir=WORK))
    run_env=dict(ENV);run_env.pop('CANVAS_TEST_MESH_GPU',None)
    subprocess.run([JS_RUNTIME,str(ROOT/'tests/canvas_backend_browser.mjs'),(WORK/'test.html').as_uri(),str(profile)],env=run_env,cwd=ROOT,check=True,timeout=60)

def main():
    # Stale IR from another toolchain would leak into the saved build.
    shutil.rmtree(WORK,ignore_errors=True)
    WORK.mkdir(parents=True)
    ir=WORK/'ir';source=ROOT/'tests/bitmap_image_test.zi'
    common=['--project','--root',ROOT/'tests','--define','PLATFORM_WEB']
    web=['--target-os','web','--target-cpu','wasm32']
    run([ZIRAN,'ir',*common,'--target=c',*web,'-o',ir,source])
    for form in ('source','saved'):
        generated=WORK/form
        inputs=common if form=='source' else ['--root',ir,'--define','PLATFORM_WEB']
        run([ZIRAN,'build',*inputs,'--target=c',*web,'--no-main','-o',generated,source if form=='source' else ir/'bitmap_image_test.zir'])
        shell=WORK/'shell.html'
        shell.write_text('<!doctype html><html><head><meta charset="utf-8"></head><body style="margin:0"><div id="canvas-frame" style="width:16px;height:16px"><canvas id="canvas" width="16" height="16" style="width:16px;height:16px"></canvas></div><pre id="result">pending</pre>{{{ SCRIPT }}}</body></html>')
        run([EMCC,'-O1','-I'+str(ZIRAN_ROOT/'include'),'-iquote',generated,*sorted(generated.rglob('*.c')),'--js-library',ZIRAN_ROOT/'web/ziran_web.js','-sASYNCIFY','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=16777216','-sSINGLE_FILE=1','-sENVIRONMENT=web','--shell-file',shell,'-o',WORK/'test.html'])
        browser()
        print(f'Managed Image actual Canvas/Wasm {form}: alpha, smooth, updates, allocation failure, release and provider teardown passed',flush=True)
if __name__=='__main__':main()

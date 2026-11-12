#!/usr/bin/env python3
"""Real Raylib WASM shader image host. Caller owns the build-queue slot."""
import functools,http.server,os,subprocess,tempfile,threading
from pathlib import Path
from toolchain import JS_RUNTIME,ZIRAN,ZIRAN_ROOT
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/scratch/shader-web'
ENV={k:v for k,v in os.environ.items() if k not in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER')}
ENV.update(HARMONY_DESKTOP_RECOVERY='0',YUE_DESKTOP_RECOVERY='0',EMCC_CORES='1',EM_CACHE=str(ROOT/'build/emscripten-cache'))
EMCC=os.environ.get('EMCC',str(Path.home()/'emsdk/upstream/emscripten/emcc'))
def run(args):subprocess.run(list(map(str,args)),cwd=ROOT,env=ENV,check=True)
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 run(['python3',ROOT/'tests/image_pixels_fixtures.py'])
 gen=OUT/'c'
 run([ZIRAN,'build','--project','--module-path',ROOT/'tests','--root',ROOT/'tests','--target=c','--define','PLATFORM_WEB','--no-main','-o',gen,ROOT/'tests/shader_host_test.zi'])
 shell=OUT/'shell.html'
 shell.write_text('<!doctype html><html><body><canvas id="canvas" width="64" height="64"></canvas><pre id="result">pending</pre><script>var Module={canvas:document.getElementById("canvas"),onAbort:r=>document.getElementById("result").textContent="FAIL abort "+r};</script>{{{ SCRIPT }}}</body></html>')
 run([EMCC,'-O1','-I'+str(ZIRAN_ROOT/'include'),'-iquote',gen,*sorted(gen.rglob('*.c')),ROOT/'build/raylib-web/libraylib.web.a','--js-library',ZIRAN_ROOT/'web/ziran_web.js','-sUSE_GLFW=3','-sASYNCIFY','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=16777216','-sENVIRONMENT=web','--preload-file',str(ROOT/'build/scratch/image-pixels')+'@/build/scratch/image-pixels','--shell-file',shell,'-o',OUT/'test.html'])
 class Handler(http.server.SimpleHTTPRequestHandler):
  def log_message(self,*args):pass
 server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(OUT)))
 threading.Thread(target=server.serve_forever,daemon=True).start()
 try:
  with tempfile.TemporaryDirectory(prefix='cr-',dir=OUT) as profile:run([JS_RUNTIME,ROOT/'tests/shader_browser.mjs',f'http://127.0.0.1:{server.server_port}/test.html',profile])
 finally:server.shutdown();server.server_close()
if __name__=='__main__':main()

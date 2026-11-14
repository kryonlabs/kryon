"""Compile the public Text API with both actual browser renderers."""
import os
from pathlib import Path
import subprocess
import sys
from toolchain import ZIRAN, ZIRAN_ROOT

ROOT=Path(__file__).resolve().parents[1]
env=dict(os.environ)
for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','GDK_DISPLAY'):
    env.pop(key,None)
env['YUE_DESKTOP_RECOVERY']='0'
short_temp=ROOT/'build/scratch/t'
short_temp.mkdir(parents=True,exist_ok=True)
env['TMPDIR']=str(short_temp)
env.setdefault('EM_CACHE',str(ROOT/'build/emscripten-cache'))
projects=[]
for backend in ('canvas',):
    project=ROOT/'build/scratch/rotated-text'/backend
    (project/'src').mkdir(parents=True,exist_ok=True)
    (project/'assets').mkdir(exist_ok=True)
    (project/'src/app.zi').write_bytes((ROOT/'tests/fixtures/rotated_text.zi').read_bytes())
    (project/'ziran.toml').write_text(
        '[package]\nname="text_rotation"\nentry="src/app.zi"\nmodule_roots=["src"]\nbridge_modules=["app"]\n'
        '[toolchain]\ngit="https://github.com/ziranlang/ziran.git"\nref="master"\n'
        '[dependencies.kryon]\ngit="https://github.com/kryonlabs/kryon.git"\nref="master"\n'
        '[tool.kryon.profiles.web]\nbackend="'+backend+'"\nassets="assets"\n')
    (project/'ziran.local.toml').write_text('[overrides]\nkryon="'+str(ROOT)+'"\nziran="'+str(ZIRAN_ROOT)+'"\n')
    def run(args):
        subprocess.run(list(map(str,args)),cwd=project,env=env,check=True,timeout=600)
    run([ZIRAN,'lock'])
    run([ZIRAN,'tool','kryon','build','--profile','web'])
    projects.append((backend,project))
if '--build-only' not in sys.argv:
    for backend,project in projects:
        run([sys.executable,ROOT/'tests/rotated_text_browser.py',
             (project/'build/text_rotation-web.html').as_uri(),project/'browser-profile',backend])

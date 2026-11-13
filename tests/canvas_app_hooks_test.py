"""Optional host dispatch in native C, saved IR, and portable ZX."""
import os
from pathlib import Path
import subprocess
from toolchain import ROOT, ZIRAN

work = ROOT / 'work/canvas-app-hooks-test'
subprocess.run(['git', 'check-ignore', '-q', str(work)], cwd=ROOT, check=True)
work.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ZIRAN_JOBS='1')
def run(args):
    subprocess.run(list(map(str, args)), cwd=ROOT, env=env, check=True)
paths = ['--project', '--offline', '--root', ROOT]
source = ROOT / 'tests/canvas_app_hooks_test.zi'
run([ZIRAN, 'ir', *paths, '-o', work / 'ir', source])
for form in ('source', 'saved'):
    selected = source if form == 'source' else work / 'ir/tests/canvas_app_hooks_test.zir'
    # Saved IR keeps the source module's relative path.
    if form == 'saved' and not selected.exists():
        selected = next((work / 'ir').rglob('canvas_app_hooks_test.zir'))
    modules = paths if form == 'source' else ['--root', work / 'ir']
    run([ZIRAN, 'build', '--target=c', '--exe', '--entry', 'canvas_app_hooks_test:main',
         *modules, '-o', work / 'native', selected])
    run([work / 'native/canvas_app_hooks_test'])
run([ZIRAN, 'bundle', *paths, '--entry', 'canvas_app_hooks_test:main', '-o', work / 'test.zx', source])
result = subprocess.check_output([str(ZIRAN), 'run', str(work / 'test.zx')], cwd=ROOT, env=env, text=True)
assert result.strip() == '0', result
print('Canvas optional hooks: source, saved IR and portable local tests passed')

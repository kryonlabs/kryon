#!/usr/bin/env python3
"""Run the real Canvas/DOM image boundary in private headless Firefox."""
import os
import subprocess
from browser_behavior import ROOT, environment, EMCC, BIN, ZIRAN

work = ROOT / 'build/scratch/avif-browser'
work.mkdir(parents=True, exist_ok=True)
generated = work / 'generated'
command = [BIN/'zi2c', '--no-main','--define','PLATFORM_WEB','--root',ROOT/'tests',
           '--module-path',ROOT/'src/backend','--module-path',ZIRAN/'std',
           '-o',generated,ROOT/'tests/avif_browser_behavior.zi']
if not os.environ.get('AVIF_BROWSER_PREBUILT'):
    subprocess.run(list(map(str,command)),env=environment,check=True)
shell=work/'shell.html'
shell.write_text('<!doctype html><html><head><meta charset="utf-8"></head><body><pre id="result">pending</pre>{{{ SCRIPT }}}</body></html>')
command=[EMCC,'-O1','-I'+str(ZIRAN/'include'),'-iquote',generated,*sorted(generated.glob('*.c')),
         '--js-library',ZIRAN/'web/ziran_web.js','-sASYNCIFY','-sSINGLE_FILE=1','-sENVIRONMENT=web',
         '--preload-file',str(ROOT/'tests/fixtures/avif')+'@/avif','--shell-file',shell,'-o',work/'test.html']
if not os.environ.get('AVIF_BROWSER_PREBUILT'):
    subprocess.run(list(map(str,command)),env=environment,check=True)
from avif_firefox_test import main
main()

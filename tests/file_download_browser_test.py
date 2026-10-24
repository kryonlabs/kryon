#!/usr/bin/env python3
"""Run file_download's streaming fetch in private headless Chromium against a local server."""
from pathlib import Path
import subprocess
import tempfile

from browser_behavior import ROOT, build_behavior, run

with tempfile.TemporaryDirectory(prefix='file-download-browser-', dir=ROOT / 'build') as path:
    work = Path(path)
    build_behavior(work, 'file_download')
    server = subprocess.Popen(['python3', ROOT / 'tests/http_transfer_server.py'], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, text=True)
    try:
        port = server.stdout.readline().strip()
        run(['node', ROOT / 'tests/canvas_backend_browser.mjs',
             (work / 'test.html').as_uri() + '?port=' + port, work / 'profile'], timeout=45)
    finally:
        server.kill()
        server.wait()
print('File download in the browser: complete file, missing, size limit and release passed')

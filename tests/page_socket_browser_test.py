#!/usr/bin/env python3
"""Run page_socket's WebSocket in private headless Chromium against a local server."""
from pathlib import Path
import subprocess
import tempfile

from browser_behavior import ROOT, build_behavior, run

with tempfile.TemporaryDirectory(prefix='page-socket-browser-', dir=ROOT / 'build') as path:
    work = Path(path)
    build_behavior(work, 'page_socket')
    server = subprocess.Popen(['python3', ROOT / 'tests/page_socket_server.py'], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, text=True)
    try:
        port = server.stdout.readline().strip()
        run(['node', ROOT / 'tests/canvas_backend_browser.mjs',
             (work / 'test.html').as_uri() + '?port=' + port, work / 'profile'], timeout=45)
    finally:
        server.kill()
        server.wait()
print('Page socket: messages, subprotocols, oversized and binary messages, close and refusals passed')

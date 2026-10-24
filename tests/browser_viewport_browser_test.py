#!/usr/bin/env python3
"""Measure browser_viewport in private headless Chromium."""
from pathlib import Path
import tempfile

from browser_behavior import ROOT, build_behavior, run

with tempfile.TemporaryDirectory(prefix='browser-viewport-browser-', dir=ROOT / 'build') as path:
    work = Path(path)
    build_behavior(work, 'browser_viewport')
    run(['node', ROOT / 'tests/canvas_backend_browser.mjs', (work / 'test.html').as_uri(), work / 'profile'],
        timeout=45)
print('Browser viewport: visual viewport, canvas frame, canvas parent and canvas measured')

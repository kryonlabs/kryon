#!/usr/bin/env python3
"""Compile file_picker for the browser and answer its chooser in private headless Chromium."""
from pathlib import Path
import tempfile

from browser_behavior import ROOT, build_behavior, run

with tempfile.TemporaryDirectory(prefix='file-picker-browser-', dir=ROOT / 'build') as path:
    work = Path(path)
    build_behavior(work, 'file_picker')
    run(['node', ROOT / 'tests/file_picker_browser.mjs', (work / 'test.html').as_uri(), work / 'profile'],
        timeout=45)
print('File picker in the browser: chosen file copied, cancel, save path and cleanup passed')

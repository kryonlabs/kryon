#!/usr/bin/env python3
"""Compile file_export for the browser and check its downloads in private headless Chromium."""

from toolchain import JS_RUNTIME
from pathlib import Path
import tempfile

from browser_behavior import ROOT, build_behavior, run

with tempfile.TemporaryDirectory(prefix='file-export-browser-', dir=ROOT / 'build') as path:
    work = Path(path)
    build_behavior(work, 'file_export')
    (work / 'downloads').mkdir()
    run([JS_RUNTIME, ROOT / 'tests/file_export_browser.mjs', (work / 'test.html').as_uri(), work / 'profile', work / 'downloads'],
        timeout=45)
print('File export in the browser: bytes and files downloaded with their names, refusals and cleanup passed')

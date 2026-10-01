"""Resolve the same configured Ziran package as the Makefile and shell tests."""

import os
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]
source = os.environ.get("ZIRAN_ROOT")
if not source:
    source = subprocess.check_output(
        [os.environ.get("ZIRAN_BIN", "ziran"), "pkg", "path", "ziran"],
        cwd=ROOT, text=True,
    ).strip()
ZIRAN_ROOT = Path(source).resolve()
BIN = Path(os.environ.get("ZIRAN_BUILD_DIR", ZIRAN_ROOT / "build")) / "bin"
ZIRAN = Path(os.environ.get("ZIRAN_BIN", BIN / "ziran"))

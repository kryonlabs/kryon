"""Resolve the same configured Ziran package as the Makefile and shell tests."""

import os
from pathlib import Path
import subprocess
import tempfile


JS_RUNTIME = os.environ.get("JS_RUNTIME", "node")

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


def chromium_temp_directory():
    """A temporary directory short enough for Chromium's Unix socket paths,
    which must stay under 108 bytes whatever TMPDIR the caller has."""
    return tempfile.TemporaryDirectory(prefix="kr-", dir="/var/tmp" if os.path.isdir("/var/tmp") else "/tmp")

"""Resolve the same configured Ziran package as the Makefile and shell tests."""

import os
import json
from pathlib import Path
import subprocess
import tempfile


JS_RUNTIME = os.environ.get("JS_RUNTIME", "node")

ROOT = Path(__file__).resolve().parents[1]
UI_MODULE_ARGS = [arg for directory in sorted((ROOT / "src/ui").rglob("*"))
                  if directory.is_dir() for arg in ("--module-path", directory)]
UI_MODULE_ARGS[:0] = ["--module-path", ROOT / "src/ui"]
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


def kryon_test_project(directory):
    """Resolve an ordinary consumer fixture against the canonical local packages."""
    directory = Path(directory)
    (directory / "src").mkdir(parents=True, exist_ok=True)
    (directory / "ziran.toml").write_text("""[package]
name = "kryon_test_fixture"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
""")
    raylib = subprocess.check_output(
        [str(ZIRAN), "pkg", "path", "raylib", "--offline"], cwd=ROOT, text=True,
    ).strip()
    overrides = (("kryon", ROOT), ("ziran", ZIRAN_ROOT), ("raylib", Path(raylib)))
    (directory / "ziran.local.toml").write_text("[overrides]\n" + "".join(
        name + " = " + json.dumps(str(path)) + "\n" for name, path in overrides))
    subprocess.run([str(ZIRAN), "lock", "--offline"], cwd=directory, check=True)
    packages = json.loads((directory / "ziran.lock").read_text())["packages"]
    return next(package["id"] for package in packages if package["name"] == "kryon")

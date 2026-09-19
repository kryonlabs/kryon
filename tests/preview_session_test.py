#!/usr/bin/env python3
"""Exercise real dynamic-library replacement without a display or app checkout."""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
CC = shlex.split(os.environ.get("CC", "cc"))
HOST = r'''
#include "app_host.h"
static int value = VERSION;
static int count(void *context) { (void)context; return value; }
static void draw(void *context, Rectangle bounds) {
    (void)context; (void)bounds; value += 10;
}
static AppHost host = {.screen_count = count, .draw = draw};
AppHost *CreateAppHost(int version, const char *project) {
    (void)project;
#ifdef REJECT
    return 0;
#endif
    if(version != APP_HOST_ABI_VERSION) return 0;
#ifdef NO_DRAW
    host.draw = 0;
#endif
    return &host;
}
#ifndef NO_DESTROY
void DestroyAppHost(AppHost *value) { (void)value; }
#endif
'''
BUILD = r'''
import pathlib
import shutil
import sys
import time
import json

assert sys.argv[1] == "-C" and sys.argv[3] == "kryon-host", sys.argv
assert sys.argv[4] == "KRYON_DIR=directory with 'quotes' and $dollars", sys.argv
project = pathlib.Path(sys.argv[2])
mode = (project / "mode").read_text()
print("building " + str(project), flush=True)
if mode == "build-failure":
    print("sample.kry:4:2: invalid widget property", file=sys.stderr)
    print(json.dumps(dict(severity="error", code="check.widget", message="invalid widget property",
                          path="sample.kry", line=4, column=2, end_line=4, end_column=8)), file=sys.stderr)
    sys.exit(1)
if mode == "slow-good-2":
    time.sleep(0.25)
    mode = "good-2"
destination = project / "build/kryon/app_host.so"
destination.parent.mkdir(parents=True, exist_ok=True)
if mode == "invalid-library":
    destination.write_text("not a shared library")
else:
    shutil.copyfile(project / (mode + ".so"), destination)
'''


def run(*args, **kwargs):
    return subprocess.run(*args, check=True, **kwargs)


with tempfile.TemporaryDirectory(prefix="kryon-preview-test.") as temporary:
    work = Path(temporary)
    project = work / "project with 'quotes' and $dollars"
    project.mkdir()
    source = work / "host.c"
    source.write_text(HOST)
    for name, definitions in {
        "good-1": ["-DVERSION=1"],
        "good-2": ["-DVERSION=2"],
        "missing-symbol": ["-DVERSION=3", "-DNO_DESTROY"],
        "rejected-abi": ["-DVERSION=4", "-DREJECT"],
        "missing-draw": ["-DVERSION=5", "-DNO_DRAW"],
    }.items():
        run(CC + ["-shared", "-fPIC", "-I", str(ROOT / "include"), *definitions,
                  str(source), "-o", str(project / (name + ".so"))])
    builder = work / "make fixture"
    builder.write_text(f"#!{sys.executable}\n" + BUILD)
    builder.chmod(0o755)
    binary = work / "preview-session-test"
    run(CC + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "include"),
              "-I", str(ROOT / "cmd/kryon-preview"),
              str(ROOT / "tests/preview_session_test.c"),
              str(ROOT / "cmd/kryon-preview/session.c"),
              str(ROOT / "cmd/kryon-preview/watch.c"),
              str(ROOT / "src/kry_std/kry_dylib.c"), str(ROOT / "src/kry_std/kry_json.c"),
              "-ldl", "-o", str(binary)])
    environment = dict(os.environ, MAKE=str(builder), TMPDIR=str(work),
                       KRYON_DIR="directory with 'quotes' and $dollars")
    result = subprocess.run([str(binary), str(project)], env=environment,
                            capture_output=True, text=True, timeout=30)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(result.returncode)
    assert "sample.kry:4:2: invalid widget property" in result.stderr
    assert not list(work.glob("kryon-preview.*")), list(work.iterdir())
    print(result.stdout.strip())
    launcher = work / "kryon"
    launcher.write_bytes((ROOT / "scripts/kryon.sh").read_bytes())
    preview = work / "kryon-preview"
    preview.write_text(f"#!{sys.executable}\nimport sys\nprint(repr(sys.argv[1:]))\n")
    preview.chmod(0o755)
    result = run(["sh", str(launcher), "--project", str(project), "preview",
                  "--source", "source with spaces.kry"], capture_output=True, text=True)
    assert result.stdout.strip() == repr(["watch", "--project", str(project),
                                         "--source", "source with spaces.kry"])
    print("preview launcher: argument forwarding without a build lock passed")

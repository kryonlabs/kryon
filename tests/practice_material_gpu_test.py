#!/usr/bin/env python3
"""Integrated skin/watercolor CPU-GLES test. Invoke through build-queue."""
import os
import subprocess
from toolchain import ROOT, ZIRAN

OUT = ROOT / "build/scratch/practice-material-gpu"
SOURCE = ROOT / "tests/practice_material_gpu_test.zi"
ENTRY = "practice_material_gpu_test:main"


def private_env():
    env = {key: value for key, value in os.environ.items() if key not in (
        "DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
        "SESSION_MANAGER", "DESKTOP_SESSION")}
    env.update(HARMONY_DESKTOP_RECOVERY="0", YUE_DESKTOP_RECOVERY="0",
               EGL_PLATFORM="surfaceless")
    return env


def build(saved):
    ir = OUT / "ir"
    for old in ir.glob("*.zir"):
        old.unlink()
    subprocess.run([str(ZIRAN), "ir", "--project", "--root", str(SOURCE.parent),
                    "--entry", ENTRY, "-o", str(ir), str(SOURCE)], cwd=ROOT, check=True)
    if saved:
        initial, ir = ir, OUT / "saved-ir"
        for old in ir.glob("*.zir"):
            old.unlink()
        subprocess.run([str(ZIRAN), "ir", "--root", str(initial), "--module-path", str(initial),
                        "--entry", ENTRY, "-o", str(ir), str(initial / (SOURCE.stem + ".zir"))],
                       cwd=ROOT, check=True)
    native = OUT / ("saved" if saved else "source")
    subprocess.run([str(ZIRAN.with_name("zi2c")), "--entry", ENTRY, "--exe", "--root", str(ir),
                    "-o", str(native), *map(str, sorted(ir.glob("*.zir")))], cwd=ROOT,
                   env=os.environ | {"LDLIBS": "-lEGL -lGLESv2 -lGLX -lX11 -lm"}, check=True)
    return native / SOURCE.stem


for saved in (False, True):
    subprocess.run([str(build(saved))], cwd=ROOT, env=private_env(), check=True)
print("Source and saved-IR skin/watercolor CPU-GLES integration local tests passed")

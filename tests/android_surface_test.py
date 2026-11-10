#!/usr/bin/env python3
"""Verify Android viewport policy and native surface updates without a display."""
import os
from pathlib import Path
import shutil
import subprocess

from toolchain import BIN, ZIRAN_ROOT as ZIRAN

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/android-surface-check"
GEN = WORK / "native"
shutil.rmtree(GEN, ignore_errors=True)
GEN.mkdir(parents=True, exist_ok=True)
environment = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER"):
    environment.pop(name, None)
environment["YUE_DESKTOP_RECOVERY"] = "0"

def run(arguments, **options):
    return subprocess.run(list(map(str, arguments)), env=environment, check=True, **options)

run([BIN / "ziran", "build", "--project", "--module-path", ROOT / "tests", "--target=c", "--no-main",
     "--define", "ANDROID_BUILD", "--entry", "android_surface_behavior:main",
     "--root", ROOT / "tests", "-o", GEN, ROOT / "tests/android_surface_behavior.zi"])
files = sorted(GEN.rglob("*.c"))
includes = ["-I" + str(ZIRAN / "include"), "-I" + str(GEN)]
includes += ["-I" + str(path) for path in sorted({p.parent for p in GEN.rglob("*.h")})]
run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
     "-Wno-unused-function", "-pthread", "-ffunction-sections", "-fdata-sections",
     "-Wl,--gc-sections", *includes, *files, "-lm", "-o", WORK / "native-test"])
run([WORK / "native-test"])

run([BIN / "ziran", "bundle", "--project", "--module-path", ROOT / "tests", "--root", ROOT / "tests", "--entry", "android_viewport_policy:PolicyCheck",
     "-o", WORK / "policy.zx", ROOT / "tests/android_viewport_policy.zi"])
result = run([BIN / "ziran", "run", WORK / "policy.zx"], capture_output=True, text=True)
assert result.stdout.strip() == "42", result.stdout

# Validate generated current native-surface code on every supported Android ABI.
# The old private raylib CORE.Window prefix is no longer part of this API.
ndk_base = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Android/Sdk"))) / "ndk"
ndks = sorted(ndk_base.glob("*/toolchains/llvm/prebuilt/linux-x86_64"))
if ndks:
    clang = ndks[-1] / "bin/clang"
    for target in ("aarch64-linux-android24", "armv7a-linux-androideabi24", "i686-linux-android24", "x86_64-linux-android24"):
        for file in files:
            run([clang, "--target=" + target, "-std=c11", "-fsyntax-only", *includes, file])
        print(target + " current native surface generation passed", flush=True)
else:
    print("Android NDK unavailable; cross ABI checks skipped")
print("Android viewport policy, native surface rotation/transform/pixels, and available four-ABI checks passed")

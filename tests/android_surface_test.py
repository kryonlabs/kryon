#!/usr/bin/env python3
"""Verify Android viewport policy and native surface updates without a display."""
import os
from pathlib import Path
import re
import subprocess

from toolchain import BIN, ZIRAN_ROOT as ZIRAN

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/android-surface-check"
GEN = WORK / "native"
GEN.mkdir(parents=True, exist_ok=True)
environment = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY"):
    environment.pop(name, None)
RAYLIB_SOURCE = Path(os.environ.get(
    "RAYLIB_SOURCE",
    subprocess.run(
        [BIN / "ziran", "pkg", "path", "raylib"],
        env=environment, text=True, capture_output=True, check=True,
    ).stdout.strip(),
))

def run(arguments, **options):
    return subprocess.run(list(map(str, arguments)), env=environment, check=True, **options)

run([BIN / "zi2c", "--no-main", "--define", "ANDROID_BUILD",
     "--root", ROOT, "--module-path", ZIRAN / "std", "-o", GEN,
     ROOT / "tests/android_surface_behavior.zi", ROOT / "tests/android_viewport_policy.zi",
     ROOT / "src/backend/android_surface.zi"])
run([BIN / "zi2c", "--no-main", "--root", ZIRAN / "std", "-o", GEN,
     ZIRAN / "std/c_string.zi"])
files = sorted(GEN.rglob("*.c"))
includes = ["-I" + str(ZIRAN / "include"), "-I" + str(GEN)]
includes += ["-I" + str(path) for path in sorted({p.parent for p in GEN.rglob("*.h")})]
run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
     "-Wno-unused-function", "-pthread", *includes, *files, "-o", WORK / "native-test"])
run([WORK / "native-test"])

cpp = WORK / "cpp"
run([BIN / "zi2cpp", "--entry", "android_viewport_policy:main", "--root", ROOT / "tests",
     "--module-path", ROOT / "src/backend", "--module-path", ZIRAN / "std",
     "-o", cpp, ROOT / "tests/android_viewport_policy.zi"])
run([os.environ.get("CXX", "c++"), "-std=c++17", "-I" + str(ZIRAN / "include"),
     "-I" + str(cpp), *sorted(cpp.glob("*.cpp")), "-o", WORK / "cpp-test"])
run([WORK / "cpp-test"])
run([BIN / "ziran", "bundle", "--root", ROOT / "tests", "--module-path", ROOT / "src/backend",
     "--module-path", ZIRAN / "std", "--entry", "android_viewport_policy:PolicyCheck",
     "-o", WORK / "policy.zib", ROOT / "tests/android_viewport_policy.zi"])
result = run([BIN / "ziran", "run", WORK / "policy.zib"], capture_output=True, text=True)
assert result.stdout.strip() == "42", result.stdout

# Compare the accessed raylib prefix to the actual package definition on all
# four Android ABIs. The test declaration below is extracted from raylib itself.
source = (RAYLIB_SOURCE / "src/rcore.c").read_text()
window = source.split("typedef struct CoreData {", 1)[1].split("} Window;", 1)[0] + "} Window;"
point = re.search(r"typedef struct \{ int x; int y; \} Point;", source).group(0)
size = re.search(r"typedef struct \{ unsigned int width; unsigned int height; \} Size;", source).group(0)
fields = {
    "title": "title", "flags": "flags", "ready": "ready", "should_close": "shouldClose",
    "resized_last_frame": "resizedLastFrame", "event_waiting": "eventWaiting", "using_fbo": "usingFbo",
    "display": "display", "screen": "screen", "position": "position", "previous_screen": "previousScreen",
    "previous_position": "previousPosition", "render": "render", "render_offset": "renderOffset",
    "current_fbo": "currentFbo", "screen_min": "screenMin", "screen_max": "screenMax",
    "screen_scale": "screenScale",
}
assertions = ['#include <stddef.h>', '#include "raylib.h"', '#include "android_surface.h"', point, size,
              "typedef struct {" + window + "} NativeRaylibCore;"]
for field, native in fields.items():
    assertions.append(f'_Static_assert(offsetof(RaylibWindowState, {field}) == offsetof(NativeRaylibCore, Window.{native}), "{field}");')
pruned = WORK / "pruned"
run([BIN / "zi2c", "--define", "ANDROID_BUILD", "--entry", "android_surface_layout:main",
     "--root", ROOT / "tests", "--module-path", ZIRAN / "std", "-o", pruned,
     ROOT / "tests/android_surface_layout.zi"])
ndk_base = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Android/Sdk"))) / "ndk"
ndks = sorted(ndk_base.glob("*/toolchains/llvm/prebuilt/linux-x86_64"))
if ndks:
    clang = ndks[-1] / "bin/clang"
    for target in ("aarch64-linux-android24", "armv7a-linux-androideabi24", "i686-linux-android24", "x86_64-linux-android24"):
        command = [clang, "--target=" + target, "-std=c11", "-fsyntax-only", *includes,
                   "-I" + str(RAYLIB_SOURCE / "src")]
        run([*command, "-x", "c", "-"], input="\n".join(assertions), text=True)
        run([clang, "--target=" + target, "-std=c11", "-fsyntax-only",
             "-I" + str(ZIRAN / "include"), "-I" + str(pruned),
             "-I" + str(RAYLIB_SOURCE / "src"), "-x", "c", "-"],
            input="\n".join(assertions), text=True)
        for file in files:
            run([*command, file])
else:
    print("Android NDK unavailable; cross ABI checks skipped")
print("Android viewport policy, surface behavior, and available NDK ABI checks passed")

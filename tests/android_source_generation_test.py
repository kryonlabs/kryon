#!/usr/bin/env python3
"""Generate real Android host graphs and consume their C with the NDK."""
import os
from pathlib import Path
import subprocess
import shutil
import sys

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, kryon_test_project

WORK = ROOT / "build/scratch/android-source-generation"
ENV = dict(os.environ, YUE_DESKTOP_RECOVERY="0")
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY",
             "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER", "DESKTOP_SESSION",
             "XDG_SESSION_ID", "XDG_SESSION_TYPE"):
    ENV.pop(name, None)

def run(command, cwd=ROOT):
    subprocess.run(list(map(str, command)), cwd=cwd, env=ENV, check=True)

def main():
    sdk = Path(os.environ.get("ANDROID_HOME", Path.home() / "Android/Sdk"))
    ndks = sorted((sdk / "ndk").glob("*/toolchains/llvm/prebuilt/linux-x86_64"))
    if not ndks:
        raise RuntimeError("Android source generation tests require the NDK")
    clang = ndks[-1] / "bin/clang"
    for kind in ("native", "custom", "zx"):
        project = WORK / kind
        identity = kryon_test_project(project)
        source = project / "src"
        entry = source / ("selected.zi" if kind == "native" else "main.zi")
        (source / "android_glue.zi").write_text(
            "#program_export\nGenerationGlue :: () -> s32 { return 0 }\n")
        custom = []
        if kind == "native":
            entry.write_text('\n'.join([
                '#import "kryon/session"', '#import "kryon/geometry"',
                '#program_export',
                'Frame :: (session: Session, viewport: Rectangle) -> s32 {',
                '    unused session; unused viewport; return 0', '}\n']))
        if kind == "custom":
            host = source / "custom_host.zi"
            host.write_text('#program_export\nCustomEntry :: () -> s32 { return 77 }\n')
            (source / "main.zi").write_text(
                '#import "custom_host"\n#program_export\nmain :: () -> s32 { return CustomEntry() }\n')
            custom = ["--host", host]
        else:
            (source / "main.zi").write_text(
                '#import "kryon/android_run"\n#program_export\n'
                'main :: (argc: s32, argv: **u8) -> s32 {\n'
                '    unused argc; unused argv; return AndroidRunMain()\n}\n')
        ir, generated = project / "build/ir", project / "build/c"
        manifest = project / "build/sources.cmake"
        run([sys.executable, ROOT / "scripts/generate_android_sources.py",
             "--ziran", ZIRAN, "--zi2c", ZIRAN_ROOT / "build/bin/zi2c",
             "--root", project, "--kryon", ROOT, "--entry", entry,
             "--package-id", identity, "--ir", ir, "--output", generated,
             "--manifest", manifest, "--define", "ANDROID_BUILD",
             "--define", "__aarch64__",
             "--codegen", "zx" if kind == "zx" else "c99", *custom])
        adapter = ir / (identity + "_frame_entry.zir")
        if adapter.is_file() != (kind == "native"):
            raise AssertionError(kind + " has the wrong native Frame adapter graph")
        if kind == "native" and not (ir / "selected.zir").is_file():
            raise AssertionError("selected native Frame provider was omitted")
        outputs = sorted(generated.rglob("*.c"))
        cmake = manifest.read_text()
        if any(str(path) not in cmake for path in outputs):
            raise AssertionError("Android CMake manifest omitted generated C")
        includes = {generated, ZIRAN_ROOT / "include",
                    *(path.parent for path in generated.rglob("*.h"))}
        objects = project / "build/objects"
        shutil.rmtree(objects, ignore_errors=True)
        objects.mkdir(parents=True)
        run([clang, "--target=aarch64-linux-android24", "-std=c11",
             "-c", "-fPIC", "-Werror=implicit-function-declaration",
             *["-I" + str(path) for path in sorted(includes)], *outputs], cwd=objects)
        if len(list(objects.glob("*.o"))) != len(outputs):
            raise AssertionError("NDK did not compile every generated C output")
        if kind == "custom":
            run([clang, "--target=aarch64-linux-android24",
                 *sorted(objects.glob("*.o")), "-o", project / "build/custom-host"])
        print(kind + " Android graph generated and all C compiled by NDK", flush=True)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generate real Android host graphs, compile their C with the NDK, and link
the JNI loader entry through the canonical Android export map."""
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

# Defined by generated C although no Zi code calls them: Android's loader
# calls JNI_OnLoad, and kryon_android_app's --wrap=fopen/fopen64 link the rest.
LOADER_ROOTS = {"JNI_OnLoad", "__wrap_fopen", "__wrap_fopen64"}

def link_contract(kind, project, manifest, ndk):
    """Link the graph as an app's libmain.so through kryon_android_app and
    kryon_android_host, whose version script names JNI_OnLoad exactly and
    rejects an undefined name with --no-undefined-version."""
    ndk_root = ndk.parents[3]
    # A ZX player also links the app's codecs and the ZX runtime, which this
    # test does not build; the map still requires every name it lists defined.
    external = "-Wl,--unresolved-symbols=ignore-all" if kind == "zx" else ""
    (project / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.22)
project(GenerationLink C)
set(ZI2C_BIN "{ZIRAN_ROOT}/build/bin/zi2c")
include("{ROOT}/cmake/KryonAndroid.cmake")
include("{manifest}")
add_library(main SHARED ${{APP_ZIRAN_GENERATED_SOURCES}})
target_include_directories(main PRIVATE ${{APP_ZIRAN_INCLUDE_DIRS}} "{ZIRAN_ROOT}/include")
target_link_options(main PRIVATE -Wl,--no-undefined {external})
kryon_android_app(main)
kryon_android_host(main ${{APP_ZIRAN_GENERATED_SOURCES}})
''')
    build = project / "build/link"
    shutil.rmtree(build, ignore_errors=True)
    run(["cmake", "-S", project, "-B", build, "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={ndk_root}/build/cmake/android.toolchain.cmake",
         "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-24"])
    run(["cmake", "--build", build, "--parallel", "2"])
    symbols = subprocess.run(
        [ndk / "bin/llvm-nm", "-D", "--defined-only", build / "libmain.so"],
        env=ENV, check=True, capture_output=True, text=True).stdout
    exported = {line.split()[-1] for line in symbols.splitlines() if line.split()}
    # Android resolves these two by name; native_app_glue calls android_main
    # inside the library, where -fvisibility=hidden leaves it.
    required = {"JNI_OnLoad", "ANativeActivity_onCreate"}
    if not required <= exported:
        raise AssertionError(kind + " libmain.so does not export " + ", ".join(sorted(required - exported)))
    for private in ("GenerationGluePrivate", "KryonAndroidRoots"):
        if private in exported:
            raise AssertionError(kind + " libmain.so leaked " + private)
    print(kind + " libmain.so links and exports JNI_OnLoad through the canonical map", flush=True)

def main():
    sdk = Path(os.environ.get("ANDROID_HOME", Path.home() / "Android/Sdk"))
    ndks = sorted((sdk / "ndk").glob("*/toolchains/llvm/prebuilt/linux-x86_64"))
    if not ndks:
        raise RuntimeError("Android source generation tests require the NDK")
    ndk = ndks[-1]
    clang = ndk / "bin/clang"
    for kind in ("native", "custom", "zx"):
        project = WORK / kind
        identity = kryon_test_project(project)
        source = project / "src"
        entry = source / ("selected.zi" if kind == "native" else "main.zi")
        # Like the scaffold's glue, nothing in the graph calls this entry:
        # only Android's loader does, so generation must keep it explicitly.
        (source / "android_glue.zi").write_text('\n'.join([
            '#import "kryon/android_device"', '#import "kryon/android_jni"', '#program_export',
            'JNI_OnLoad :: (vm: *JniVm, reserved: *void) -> s32 {',
            '    unused reserved',
            '    return AndroidJniOnLoad(vm, "org/kryonlabs/generation/Activity")',
            '}', '#program_export', 'GenerationGluePrivate :: () -> s32 { return 0 }\n']))
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
                '#import "custom_host"\n#program_export\n'
                'main :: (argc: s32, argv: **u8) -> s32 {\n'
                '    unused argc; unused argv; return CustomEntry()\n}\n')
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
        if not (ir / "android_glue.zir").is_file():
            raise AssertionError(kind + " pruned the uncalled android_glue module")
        roots = list(ir.rglob("kryon_android_roots.zir"))
        if len(roots) != 1:
            raise AssertionError(kind + " IR was not selected through the generated root")
        outputs = sorted(generated.rglob("*.c"))
        if not any(path.name == "android_glue.c" for path in outputs):
            raise AssertionError(kind + " emitted no android_glue.c for JNI_OnLoad")
        # The root only selects the graph: startup runs the application's own
        # main, with no generated code between the host and it.
        if any("KryonAndroidRoots" in path.read_text() for path in generated.rglob("*.[ch]")):
            raise AssertionError(kind + " native output contains the generated root")
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
        defined = subprocess.run(
            [ndk / "bin/llvm-nm", "--defined-only", "--extern-only", *sorted(objects.glob("*.o"))],
            env=ENV, check=True, capture_output=True, text=True).stdout.split()
        missing = LOADER_ROOTS - set(defined)
        if missing:
            raise AssertionError(kind + " generated C lacks " + ", ".join(sorted(missing)))
        if kind == "custom":
            # The retained glue and fopen wrappers link as kryon_android_app links them.
            run([clang, "--target=aarch64-linux-android24",
                 "-Wl,--wrap=fopen", "-Wl,--wrap=fopen64",
                 *sorted(objects.glob("*.o")), "-landroid", "-llog",
                 "-o", project / "build/custom-host"])
        print(kind + " Android graph generated and all C compiled by NDK", flush=True)
        if kind != "custom":
            link_contract(kind, project, manifest, ndk)

if __name__ == "__main__":
    main()

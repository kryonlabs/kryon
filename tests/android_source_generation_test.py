#!/usr/bin/env python3
"""Generate Android host graphs and verify their NDK-linked JNI exports."""
import argparse
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tomllib

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, kryon_test_project

WORK = ROOT / "build/scratch/android-source-generation"
ENV = dict(os.environ, YUE_DESKTOP_RECOVERY="0", HARMONY_DESKTOP_RECOVERY="0")
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "GDK_DISPLAY",
             "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER", "DESKTOP_SESSION",
             "XDG_SESSION_ID", "XDG_SESSION_TYPE"):
    ENV.pop(name, None)

def run(command, cwd=ROOT):
    subprocess.run(list(map(str, command)), cwd=cwd, env=ENV, check=True)

# Defined by generated C although no Zi code calls them: Android's loader
# calls JNI_OnLoad, and kryon_android_app's --wrap=fopen/fopen64 link the rest.
LOADER_ROOTS = {"JNI_OnLoad", "__wrap_fopen", "__wrap_fopen64"}
RECEIVER_EXPORTS = {
    "Java_org_kryonlabs_notifications_NotificationReceiver_receive",
    "Java_org_kryonlabs_notifications_NotificationReceiver_requestPermission",
}
PICTURE_MODES = {"pictures", "avif", "webp"}
GLES_MODES = {"native-gles", "runtime-gles", "runtime-gles-armv7"}
KINDS = ("native", "native-gles", "pictures", "avif", "webp", "custom", "runtime", "runtime-gles", "runtime-gles-armv7", "zx")

def link_contract(kind, project, manifest, ndk, abi):
    """Link the graph as an app's libmain.so through kryon_android_app and
    kryon_android_host, whose version script names JNI_OnLoad exactly and
    rejects an undefined name with --no-undefined-version."""
    ndk_root = ndk.parents[3]
    # The ZX runtime is tested separately. The export contract is still checked.
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
app_ziran_link_native(main)
target_link_libraries(main PRIVATE z)
''')
    build = project / "build/link"
    shutil.rmtree(build, ignore_errors=True)
    run(["cmake", "-S", project, "-B", build, "-G", "Ninja",
         f"-DCMAKE_TOOLCHAIN_FILE={ndk_root}/build/cmake/android.toolchain.cmake",
         f"-DANDROID_ABI={abi}", "-DANDROID_PLATFORM=android-24"])
    run(["cmake", "--build", build, "--parallel", "2"])
    targets = (build / "build.ninja").read_text()
    for target, expected in (("libavif.a", kind == "avif"),
                             ("libziran_webp_android.a", kind == "webp"), ("libaom.a", False)):
        if (target in targets) != expected:
            raise AssertionError(f"{kind} has the wrong native targets for {target}")
    if kind in ("native", "runtime") or kind in GLES_MODES:
        dynamic = subprocess.check_output([ndk / "bin/llvm-readelf", "-d", build / "libmain.so"],
                                          env=ENV, text=True)
        for library in ("libEGL.so", "libGLESv3.so"):
            if (library in dynamic) != (kind in GLES_MODES):
                raise AssertionError(f"{kind} has the wrong dynamic dependency on {library}")
        if kind == "native" and "libz.so" in dynamic:
            raise AssertionError("plain Android Frame host loads libz.so")
    symbols = subprocess.run(
        [ndk / "bin/llvm-nm", "-D", "--defined-only", build / "libmain.so"],
        env=ENV, check=True, capture_output=True, text=True).stdout
    exported = {line.split()[-1] for line in symbols.splitlines() if line.split()}
    # Android resolves these by name; native_app_glue calls android_main
    # directly inside the library.
    required = {"JNI_OnLoad", "ANativeActivity_onCreate"} | RECEIVER_EXPORTS
    if not required <= exported:
        raise AssertionError(kind + " libmain.so does not export " + ", ".join(sorted(required - exported)))
    for private in ("GenerationGluePrivate", "KryonAndroidRoots"):
        if private in exported:
            raise AssertionError(kind + " libmain.so leaked " + private)
    print(kind + " libmain.so exports Android loader and notification JNI entries", flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=KINDS, action="append")
    args = parser.parse_args()
    sdk = Path(os.environ.get("ANDROID_HOME", Path.home() / "Android/Sdk"))
    ndks = sorted((sdk / "ndk").glob("*/toolchains/llvm/prebuilt/linux-x86_64"))
    if not ndks:
        raise RuntimeError("Android source generation tests require the NDK")
    ndk = ndks[-1]
    clang = ndk / "bin/clang"
    local = ROOT / "ziran.local.toml"
    overrides = []
    if local.is_file():
        with local.open("rb") as source:
            paths = tomllib.load(source).get("overrides", {})
        overrides = [(name, (ROOT / paths[name]).resolve()) for name in ("avif", "webp") if name in paths]
    for kind in args.kind or KINDS:
        armv7 = kind == "runtime-gles-armv7"
        abi = "armeabi-v7a" if armv7 else "arm64-v8a"
        target = "armv7a-linux-androideabi24" if armv7 else "aarch64-linux-android24"
        cpu_define = "__arm__" if armv7 else "__aarch64__"
        project = WORK / kind
        # Each case owns its generated application. Recreate authored inputs
        # so an earlier run cannot leave a second module with another suffix.
        shutil.rmtree(project / "src", ignore_errors=True)
        kryon_test_project(project, overrides)
        native_frame = kind in ("native", "native-gles") or kind in PICTURE_MODES
        source = project / "src"
        entry = source / ("selected.tsx" if kind == "native" else "selected.zi" if native_frame else "main.zi")
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
            (source / "thread_probe.zi").write_text(
                '#import "std/thread_linux"\nTouch :: () { unused ThreadFinished(null) }\n')
            entry.write_text('\n'.join([
                'import * as App from "kryon/app";',
                'import * as Logic from "thread_probe";',
                'export function Frame(frame: App.Frame): void { Logic.Touch(); }',
                'export function main(): s32 { return App.Run({}, Frame); }\n']))
        elif kind == "native-gles":
            entry.write_text('\n'.join([
                'App :: #import "kryon/app";',
                '#import "kryon/geometry"',
                '#import "kryon/android_egl"',
                'Frame :: (frame: App.Frame) {',
                '    unused frame',
                '    EnableAndroidGlesComposition()',
                '}',
                'main :: () -> s32 { return App.Run(App.Window.{}, Frame) }\n']))
        elif kind in PICTURE_MODES:
            codec = f'#import "kryon/android_{kind}"' if kind != "pictures" else ""
            entry.write_text('\n'.join([
                'App :: #import "kryon/app";',
                '#import "kryon/session"', '#import "kryon/geometry"',
                'Image :: #import "kryon/image";', codec,
                'Frame :: (frame: App.Frame) {',
                '    session := Session.{.slot = frame.slot, .epoch = frame.epoch}',
                '    Image.Draw(session, Image.Props.{ .key = 1, .bounds = frame.viewport, .asset_path = "image.png" })',
                '}',
                'main :: () -> s32 { return App.Run(App.Window.{}, Frame) }\n']))
        if kind in ("runtime", "runtime-gles", "runtime-gles-armv7"):
            enable_gles = kind in GLES_MODES
            (source / "main.zi").write_text(
                '#import "kryon/android_runtime"\n'
                + ('#import "kryon/android_egl"\n' if enable_gles else '')
                + '#program_export\n'
                'main :: (argc: s32, argv: **u8) -> s32 {\n'
                '    unused argc; unused argv\n'
                + ('    EnableAndroidGlesComposition()\n' if enable_gles else '')
                + '    while PollAndroidHost(16) {\n'
                '        unused AndroidHostViewport(); BeginAndroidFrame()\n'
                '        if !EndAndroidFrame() { return 1 }\n'
                '    }\n    return 0\n}\n')
            custom = ["--host", ROOT / "src/backend/android_runtime.zi"]
        elif kind == "custom":
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
             "--ir", ir, "--output", generated,
             "--manifest", manifest, "--define", "ANDROID_BUILD",
             "--define", cpu_define,
             "--codegen", "zx" if kind == "zx" else "c99", *custom])
        host_module = ir / "kryon__android_run.zir"
        if host_module.is_file() != (kind != "custom" and not kind.startswith("runtime")):
            raise AssertionError(kind + " has the wrong Android program host graph")
        if native_frame and not (ir / "selected.zir").is_file():
            raise AssertionError("selected native Frame provider was omitted")
        if kind == "native":
            thread_headers = [path for path in generated.rglob("*.h")
                              if path.name == "std_thread_linux.h"]
            if len(thread_headers) != 1:
                raise AssertionError("native Android graph omitted std/thread_linux")
            thread_header = thread_headers[0].read_text()
            if "pthread_gettid_np" not in thread_header or "pthread_tryjoin_np" in thread_header:
                raise AssertionError("native Android graph retained the host-Linux thread branch")
        if not (ir / "android_glue.zir").is_file():
            raise AssertionError(kind + " pruned the uncalled android_glue module")
        if list(ir.glob("kryon_android_*.zir")):
            raise AssertionError(kind + " IR contains a generated program adapter or selector")
        outputs = sorted(generated.rglob("*.c"))
        if not any(path.name == "android_glue.c" for path in outputs):
            raise AssertionError(kind + " emitted no android_glue.c for JNI_OnLoad")
        if any("GenerationGluePrivate" in path.read_text() for path in generated.rglob("*.[ch]")):
            raise AssertionError(kind + " native output retains an unused glue function")
        cmake = manifest.read_text()
        if kind == "native" and (str(entry) not in cmake or str(source / "thread_probe.zi") not in cmake):
            raise AssertionError("mixed TSX/Ziran sources are missing from Android configure inputs")
        if native_frame:
            if bool(list(ir.glob("*_pixmap_assets_android.zir"))) != (kind in PICTURE_MODES):
                raise AssertionError(f"{kind} has the wrong picture-provider graph")
            if ("ZIRAN_SOURCE_libavif" in cmake) != (kind == "avif"):
                raise AssertionError(f"{kind} has the wrong AVIF dependency graph")
            if ("ZIRAN_SOURCE_libwebp" in cmake) != (kind == "webp") or "ZIRAN_SOURCE_libaom" in cmake:
                raise AssertionError(f"{kind} retained an unused codec dependency")
        if any(str(path) not in cmake for path in outputs):
            raise AssertionError("Android CMake manifest omitted generated C")
        includes = {generated, ZIRAN_ROOT / "include",
                    *(path.parent for path in generated.rglob("*.h"))}
        objects = project / "build/objects"
        shutil.rmtree(objects, ignore_errors=True)
        objects.mkdir(parents=True)
        run([clang, f"--target={target}", "-std=c11",
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
            link_contract(kind, project, manifest, ndk, abi)

if __name__ == "__main__":
    main()

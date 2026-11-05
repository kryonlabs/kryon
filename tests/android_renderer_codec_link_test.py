#!/usr/bin/env python3
"""Verify that Android renderer interfaces carry their codec dependency."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT


PREBUILT_APP_SOURCE = '''#include "android_native_app_glue.h"

extern int KryonRendererProbe(void);

int KryonAppProbe(void) { return KryonRendererProbe(); }

void android_main(struct android_app *application)
{
    (void)application;
    (void)KryonAppProbe();
}
'''

SOURCE_APP_SOURCE = '''#include "android_native_app_glue.h"

extern int WebPGetDecoderVersion(void);

__attribute__((used, visibility("default")))
int KryonRendererProbe(void) { return WebPGetDecoderVersion(); }

int main(void) { return KryonRendererProbe(); }
'''

PREBUILT_PROJECT = '''cmake_minimum_required(VERSION 3.22)
project(KryonPrebuiltRendererLink C)
include("${KRYON_DIR}/cmake/KryonAndroid.cmake")
add_library(renderer_archive STATIC renderer.c)
add_library(codec_archive STATIC codec.c)
add_library(raylib INTERFACE)
target_link_libraries(raylib INTERFACE
    "$<TARGET_FILE:renderer_archive>" "$<TARGET_FILE:codec_archive>")
add_library(app SHARED app.c)
target_link_options(app PRIVATE -Wl,--no-undefined)
add_dependencies(app renderer_archive codec_archive)
kryon_android_app(app)
'''

SOURCE_PROJECT = '''cmake_minimum_required(VERSION 3.22)
project(KryonSourceRendererLink C)
include("${KRYON_DIR}/cmake/KryonAndroid.cmake")
kryon_android_raylib("${RAYLIB_DIR}" "${WEBP_DIR}" "${AVIF_DIR}" "${AOM_DIR}")
add_library(app SHARED app.c)
target_link_options(app PRIVATE -Wl,--no-undefined)
kryon_android_app(app)
'''


def run(command, extra_environment=None):
    environment = {key: value for key, value in os.environ.items()
                   if key not in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                                  "DBUS_SESSION_BUS_ADDRESS")}
    environment.update(extra_environment or {})
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=environment,
                            text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout[-8000:])
    return result.stdout


def package(name):
    return Path(run([ZIRAN, "pkg", "path", name]).strip())


def locate_ndk(selected):
    if selected:
        return selected.resolve()
    candidates = [os.environ.get("ANDROID_NDK_HOME"),
                  os.environ.get("ANDROID_NDK_ROOT"),
                  str(Path.home() / "Android/Sdk/ndk/28.2.13676358")]
    for candidate in candidates:
        if candidate and (Path(candidate) / "build/cmake/android.toolchain.cmake").is_file():
            return Path(candidate).resolve()
    installed = Path.home() / "Android/Sdk/ndk"
    if installed.is_dir():
        found = sorted(path for path in installed.iterdir()
                       if (path / "build/cmake/android.toolchain.cmake").is_file())
        if found:
            return found[-1].resolve()
    raise SystemExit("No Android NDK found; pass --ndk PATH")


def build(project, output, ndk, zi2c, raylib, webp, abi="armeabi-v7a"):
    run(["cmake", "-S", project, "-B", output,
         f"-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake",
         f"-DANDROID_ABI={abi}", "-DANDROID_PLATFORM=android-21",
         "-DCMAKE_BUILD_TYPE=Release", f"-DKRYON_DIR={ROOT}",
         f"-DAVIF_DIR={package('libavif')}", f"-DAOM_DIR={package('libaom')}", f"-DRAYLIB_DIR={raylib}/src", f"-DWEBP_DIR={webp}",
         f"-DZI2C_BIN={zi2c}"])
    run(["cmake", "--build", output, "--target", "app", "--parallel", "2"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", type=Path)
    args = parser.parse_args()
    ndk = locate_ndk(args.ndk)
    tools = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    assert (tools / "llvm-nm").is_file(), "Use a Linux Android NDK"
    zi2c = ZIRAN_ROOT / "build/bin/zi2c"
    assert zi2c.is_file(), f"Missing Ziran compiler at {zi2c}"
    raylib = package("raylib")
    webp = package("libwebp")
    scratch = ROOT / "build/scratch/android-renderer-codec-link"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        work = Path(temporary)
        for name, project, renderer in (
            ("prebuilt", PREBUILT_PROJECT, False),
            ("source", SOURCE_PROJECT, True),
        ):
            fixture = work / name
            fixture.mkdir()
            (fixture / "app.c").write_text(
                SOURCE_APP_SOURCE if renderer else PREBUILT_APP_SOURCE)
            (fixture / "CMakeLists.txt").write_text(project)
            if not renderer:
                (fixture / "renderer.c").write_text(
                    "extern int KryonCodecProbe(void);\n"
                    "int KryonRendererProbe(void) { return 40 + KryonCodecProbe(); }\n")
                (fixture / "codec.c").write_text(
                    "int KryonCodecProbe(void) { return 2; }\n")
            output = fixture / "build"
            build(fixture, output, ndk, zi2c, raylib, webp)
            link = (output / "CMakeFiles/app.dir/link.txt").read_text()
            assert "-lkryonwebp" not in link, link
            library = output / "libapp.so"
            undefined = run([tools / "llvm-nm", "--undefined-only", "-D", library])
            assert "WebPGetDecoderVersion" not in undefined, undefined
            if renderer:
                assert "libkryonwebp.a" in link, link
            else:
                assert "librenderer_archive.a" in link, link
                assert "libcodec_archive.a" in link, link
                defined = run([tools / "llvm-nm", "--defined-only", "-D", library])
                assert "KryonRendererProbe" in defined, defined
                assert "KryonCodecProbe" in defined, defined
            print(f"{name}: renderer codec dependency linked transitively")
    print("Android renderer codec link passed")


if __name__ == "__main__":
    main()

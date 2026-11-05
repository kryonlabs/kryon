#!/usr/bin/env python3
"""Link Kryon's WebP decoder into Android libraries for every ABI.

The fixture builds `kryonwebp` from the locked libwebp source through
cmake/KryonAndroid.cmake, as application scaffolds do, and links Ziran code
generated from src/backend/webp_image.zi into a shared library with
--no-undefined. x86_64 uses the one-argument kryon_android_raylib form that
older scaffolds call, with KRYON_ANDROID_WEBP_DIR from `kryon build`.
Run with --ndk PATH; no display or device is used.
"""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
from image_packages import image_module_args

ABIS = ("arm64-v8a", "armeabi-v7a", "x86", "x86_64")
PROBE = '''#import "webp_image"
#import "std/byte_text_linux"

#program_export
KryonWebPProbe :: (data: *u8, count: s32) -> s32 {
    width: s32 = 0
    height: s32 = 0
    pixels := WebPImageDecode(TextFromBytes(BytesFromPointer(data, cast(u64)count)), *width, *height)
    if pixels == null { return 0 }
    return width
}
'''
CMAKE = '''cmake_minimum_required(VERSION 3.22)
project(KryonWebPAndroid C)
include("${KRYON_DIR}/cmake/KryonAndroid.cmake")
if(ONE_ARGUMENT)
    kryon_android_raylib("${RAYLIB_DIR}")
else()
    kryon_android_raylib("${RAYLIB_DIR}" "${WEBP_DIR}" "${AVIF_DIR}" "${AOM_DIR}")
endif()
file(GLOB probe_sources "${PROBE_DIR}/*.c")
add_library(probe SHARED ${probe_sources})
target_include_directories(probe PRIVATE "${ZIRAN_INCLUDE}" "${PROBE_DIR}")
target_link_options(probe PRIVATE -Wl,--no-undefined -Wl,--gc-sections)
target_link_libraries(probe PRIVATE kryonwebp m)
'''


def run(command, extra=None):
    environment = {key: value for key, value in os.environ.items()
                   if key not in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS")}
    environment.update(extra or {})
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=environment, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise SystemExit(result.stdout[-6000:])
    return result.stdout


def package(name):
    return Path(run([ZIRAN, "pkg", "path", name]).strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", required=True, type=Path)
    args = parser.parse_args()
    ndk = args.ndk.resolve()
    nm = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm"
    assert nm.is_file(), "Use a Linux Android NDK"
    raylib, webp = package("raylib") / "src", package("libwebp")
    scratch = ROOT / "build/scratch/android-webp-link"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        work = Path(temporary)
        source = work / "probe.zi"
        source.write_text(PROBE)
        generated = work / "generated"
        run([ZIRAN, "build", *image_module_args(), "--target=c", "--no-main", "--define", "ANDROID_BUILD",
             "--root", work, "--module-path", ROOT / "src/backend",
             "--module-path", ZIRAN_ROOT / "std", "-o", generated, source])
        project = work / "project"
        project.mkdir()
        (project / "CMakeLists.txt").write_text(CMAKE)
        for abi in ABIS:
            output = work / abi
            one = abi == "x86_64"
            run(["cmake", "-S", project, "-B", output,
                 f"-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake",
                 f"-DANDROID_ABI={abi}", "-DANDROID_PLATFORM=android-21",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DKRYON_DIR={ROOT}",
                 f"-DAVIF_DIR={package('libavif')}", f"-DAOM_DIR={package('libaom')}", f"-DRAYLIB_DIR={raylib}", *([] if one else [f"-DWEBP_DIR={webp}"]),
                 f"-DONE_ARGUMENT={'ON' if one else 'OFF'}", f"-DPROBE_DIR={generated}",
                 f"-DZIRAN_INCLUDE={ZIRAN_ROOT / 'include'}"],
                {"KRYON_ANDROID_WEBP_DIR": str(webp), "KRYON_ANDROID_AVIF_DIR": str(package("libavif")), "KRYON_ANDROID_AOM_DIR": str(package("libaom"))} if one else None)
            run(["cmake", "--build", output, "--target", "probe", "--parallel", "2"])
            symbols = run([nm, "--defined-only", "-D", output / "libprobe.so"])
            assert "KryonWebPProbe" in symbols, abi
            undefined = run([nm, "--undefined-only", "-D", output / "libprobe.so"])
            assert "WebP" not in undefined, f"{abi}: {undefined}"
            print(f"{abi}: kryonwebp built and webp_image linked with --no-undefined")
    print("Android WebP link passed for every ABI")


if __name__ == "__main__":
    main()

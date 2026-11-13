#!/usr/bin/env python3
"""Configure the Android `kryon:renderer-raylib` artifact project for arm64.

scripts/renderer_artifact.py builds this project for apps that link the
raylib renderer on Android. It must define its own raylib build while
cmake/KryonAndroid.cmake, the native host's helpers, stays renderer-free.
"""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import ROOT, ZIRAN


def run(command):
    environment = {key: value for key, value in os.environ.items()
                   if key not in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                                  "DBUS_SESSION_BUS_ADDRESS")}
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
    for candidate in (os.environ.get("ANDROID_NDK_HOME"),
                      os.environ.get("ANDROID_NDK_ROOT"),
                      str(Path.home() / "Android/Sdk/ndk/28.2.13676358")):
        if candidate and (Path(candidate) / "build/cmake/android.toolchain.cmake").is_file():
            return Path(candidate).resolve()
    raise SystemExit("No Android NDK found; pass --ndk PATH")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", type=Path)
    args = parser.parse_args()
    ndk = locate_ndk(args.ndk)
    nm = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm"
    assert nm.is_file(), "Use a Linux Android NDK"
    host = (ROOT / "cmake/KryonAndroid.cmake").read_text()
    assert "raylib" not in host, "KryonAndroid.cmake must stay renderer-free"
    scratch = ROOT / "build/scratch/android-renderer-artifact-cmake"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        output = Path(temporary) / "arm64"
        # The same definitions scripts/renderer_artifact.py passes.
        configured = run([
            "cmake", "-S", ROOT / "cmake/renderer-artifact", "-B", output,
            "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_SHARED_LIBS=OFF",
            f"-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake",
            "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-21",
            f"-DRAYLIB_DIR={package('raylib')}/src",
            f"-DWEBP_DIR={package('libwebp')}",
            f"-DKRYON_WEBP_PACKAGE_DIR={package('webp')}",
            f"-DAVIF_DIR={package('libavif')}", f"-DAOM_DIR={package('libaom')}",
            f"-DKRYON_AVIF_PACKAGE_DIR={package('avif')}",
            f"-DDAV1D_DIR={package('dav1d')}",
            f"-DKRYON_ANDROID_AUDIO_DIR={package('audio')}",
            f"-DKRYON_ANDROID_OPUS_DIR={package('libopus')}",
            f"-DKRYON_ANDROID_SAMPLERATE_DIR={package('libsamplerate')}"])
        assert "Unknown CMake command" not in configured, configured
        targets = run(["cmake", "--build", output, "--target", "help"])
        for target in ("raylib", "kryonwebp", "avif_static", "opus"):
            assert f"... {target}" in targets, (target, targets)
        print("arm64-v8a: renderer artifact project configured")
        run(["cmake", "--build", output, "--target", "raylib", "--parallel", "2"])
        symbols = run([nm, "--defined-only", next(output.rglob("libraylib.a"))])
        # src/backend/android_fopen.zi defines the app's __wrap_fopen.
        assert "KryonRaylibFopen" in symbols, symbols[-2000:]
        assert " __wrap_fopen" not in symbols, "raylib would collide with Kryon's __wrap_fopen"
        print("arm64-v8a: raylib built without a conflicting __wrap_fopen")
    print("Android renderer artifact CMake passed")


if __name__ == "__main__":
    main()

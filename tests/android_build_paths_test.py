"""Check native build IDs and crash symbols across different Android paths.

Run with --ndk PATH. The disposable CMake fixtures are not source checkouts;
they exercise app, generated, dependency, and NDK sources without a display.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
ABIS = ("arm64-v8a", "armeabi-v7a", "x86", "x86_64")


def run(command, directory):
    environment = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                 "DBUS_SESSION_BUS_ADDRESS"):
        environment.pop(name, None)
    result = subprocess.run(command, cwd=directory, env=environment,
                            text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return result.stdout


def fixture(root, ndk):
    source = root / "project"
    source.mkdir(parents=True)
    (source / "dependency").mkdir()
    (source / "app.c").write_text(
        '#include "android_native_app_glue.h"\n'
        'extern const char *DependencyFile(void);\n'
        'const char *AppFile(void) { return __FILE__; }\n'
        'const char *DependencyPath(void) { return DependencyFile(); }\n'
        'void android_main(struct android_app *app) { (void)app; }\n'
    )
    (source / "dependency/dependency.c").write_text(
        'const char *DependencyFile(void) { return __FILE__; }\n'
    )
    (source / "dependency/CMakeLists.txt").write_text(
        'add_library(dependency STATIC dependency.c)\n'
        'target_compile_options(dependency PRIVATE -Oz -g1)\n'
    )
    (source / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.22)\n'
        'project(AndroidBuildPaths C)\n'
        f'include("{ROOT}/cmake/KryonAndroid.cmake")\n'
        'if(TEST_NORMALIZE)\n'
        '    kryon_android_reproducible_paths("${CMAKE_SOURCE_DIR}")\n'
        'endif()\n'
        'add_subdirectory(dependency)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/generated.c"\n'
        '    "const char *GeneratedFile(void) { return __FILE__; }\\n")\n'
        'add_library(probe SHARED app.c "${CMAKE_BINARY_DIR}/generated.c"\n'
        '    "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}/android_native_app_glue.c")\n'
        'target_include_directories(probe PRIVATE\n'
        '    "${KRYON_ANDROID_NATIVE_APP_GLUE_DIR}")\n'
        'target_compile_options(probe PRIVATE -Oz -g1)\n'
        'target_link_libraries(probe PRIVATE dependency android log)\n'
    )
    ndk_link = root / "sdk/ndk"
    ndk_link.parent.mkdir()
    ndk_link.symlink_to(ndk, target_is_directory=True)
    return source, ndk_link


def build(source, ndk, output, abi, normalize):
    run(["cmake", "-S", str(source), "-B", str(output),
         f"-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake",
         f"-DANDROID_ABI={abi}", "-DANDROID_PLATFORM=android-21",
         "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         f"-DTEST_NORMALIZE={'ON' if normalize else 'OFF'}"], source)
    run(["cmake", "--build", str(output), "--parallel", "2"], source)
    return output / "libprobe.so"


def build_id(library, tools):
    notes = run([str(tools / "llvm-readelf"), "--notes", str(library)], ROOT)
    match = re.search(r"Build ID: ([0-9a-f]+)", notes)
    assert match, "The native library must retain its build ID"
    return match.group(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", required=True, type=Path)
    args = parser.parse_args()
    ndk = args.ndk.resolve()
    tools = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"
    assert (tools / "llvm-readelf").is_file(), "Use a Linux Android NDK"
    scratch = ROOT / "build/scratch/android-build-paths"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=scratch) as temporary:
        directory = Path(temporary)
        left, left_ndk = fixture(directory / "runner workspace", ndk)
        right, right_ndk = fixture(directory / "vagrant/build", ndk)
        # A negative control proves this fixture catches the original failure.
        control_left = build(left, left_ndk, left / "control", "x86_64", False)
        control_right = build(right, right_ndk, right / "other-control",
                              "x86_64", False)
        assert build_id(control_left, tools) != build_id(control_right, tools)
        for abi in ABIS:
            first = build(left, left_ndk, left / "output" / abi, abi, True)
            second = build(right, right_ndk, right / "different/output" / abi,
                           abi, True)
            assert first.read_bytes() == second.read_bytes(), (
                f"{abi}: unstripped libraries differ across build paths"
            )
            assert build_id(first, tools) == build_id(second, tools)
            debug = run([str(tools / "llvm-dwarfdump"), "--debug-info",
                         "--debug-line", str(first)], ROOT)
            # dwarfdump prints the input filename before the actual DWARF.
            debug = "\n".join(debug.splitlines()[1:])
            assert "DW_AT_comp_dir" in debug and "debug_line[" in debug
            assert str(directory) not in debug and str(ndk) not in debug
            assert "./build/android" in debug and "./ndk" in debug
            for library in (first, second):
                stripped = library.with_name("libprobe-stripped.so")
                run([str(tools / "llvm-strip"), "--strip-unneeded",
                     "-o", str(stripped), str(library)], ROOT)
                assert build_id(stripped, tools) == build_id(library, tools)
            assert first.with_name("libprobe-stripped.so").read_bytes() == (
                second.with_name("libprobe-stripped.so").read_bytes()
            )
            print(f"{abi}: native libraries, build IDs, and crash symbols match")
    print("Android build-path reproducibility passed")


if __name__ == "__main__":
    main()

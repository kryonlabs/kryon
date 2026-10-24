#!/usr/bin/env python3
"""Compare real GLES texture/frame hashes in an offscreen Android process.

Builds a native test executable, not an APK. Running it never launches,
installs, stops or captures an application. Its two phases use the same
renderer with serial composition and the shared frame pipeline.
"""
import argparse
import binascii
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/scratch/android-pipeline/pixels"
LOG = ROOT / "backup/android-pipeline"


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial")
    parser.add_argument("--host", action="store_true", help="Use an isolated Linux software EGL pbuffer")
    parser.add_argument("--package", default="xyz.waozi.inbe.verification")
    parser.add_argument("--raylib", type=Path, required=True,
                        help="Android raylib static archive built with custom frame control")
    parser.add_argument("--runtime", type=Path,
                        help="Android Ziran runtime static archive for the same ABI")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    if not args.host and (not args.serial or not args.runtime):
        parser.error("Android runs need --serial and --runtime")
    if not re.fullmatch(r"[A-Za-z0-9_.]+", args.package) or (args.serial and ":" in args.serial):
        parser.error("use a debuggable package and a USB development device")
    WORK.mkdir(parents=True, exist_ok=True)
    LOG.mkdir(parents=True, exist_ok=True)
    ziran = Path(run([os.environ.get("ZIRAN_BIN", "ziran"), "pkg", "path", "ziran"], cwd=ROOT).strip())
    sdk = Path(os.environ.get("ANDROID_HOME", str(Path.home() / "Android/Sdk")))
    toolchain = sdk / "ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64"
    compiler = os.environ.get("CC", "cc") if args.host else toolchain / "bin/armv7a-linux-androideabi24-clang"
    # Existing artifacts are immutable inputs. No active app output is replaced.
    raylib = args.raylib.resolve(strict=True)
    runtime = (args.runtime or ziran / "build/libziran.a").resolve(strict=True)
    source = ROOT / "tests/android_frame_pixels_test.zi"
    generated = WORK / "c"
    if generated.exists():
        shutil.rmtree(generated)
    defines = ["KRYON_FRAME_THREADS", "PLATFORM_ANDROID", "__x86_64__", "KRYON_TEST_LINUX"] if args.host else ["ANDROID_BUILD", "__arm__"]
    command = [ziran / "build/bin/ziran", "build", "--target=c", "--no-main",
               *[part for define in defines for part in ["--define", define]], "--root", ROOT / "tests",
               "--module-path", ROOT / "src/backend", "--module-path", ROOT / "src/ui",
               "-o", generated, source]
    output = run(command, cwd=ROOT)
    includes = sorted({generated, *(p.parent for p in generated.rglob("*.h"))})
    wraps = ["fopen", "BeginDrawing", "EndDrawing", "ClearBackground", "BeginMode2D",
             "EndMode2D", "BeginScissorMode", "EndScissorMode", "SetTargetFPS", "GetFrameTime",
             "CloseWindow", "eglGetCurrentContext", "PollInputEvents", "GetWindowHandle",
             "LoadImageFromMemory", "LoadTextureFromImage", "UnloadTexture", "LoadFontFromMemory", "UnloadFont", "DrawTexturePro", "MeasureText", "LoadFileData"]
    if args.host:
        wraps.remove("fopen")
        wraps.append("SDL_GL_GetCurrentContext")

    libraries = (run(["pkg-config", "--libs", "sdl2", "libdrm", "gbm", "egl", "glesv2"]).split()
                 + ["-ldl", "-lpthread", "-lm"]) if args.host else [
                     "-landroid", "-llog", "-lEGL", "-lGLESv2", "-lOpenSLES", "-latomic", "-ldl", "-lm"]
    executable = WORK / ("host-frame-test" if args.host else "frame-test")
    command = [compiler, "-std=c11", "-O2", "-ffunction-sections", "-fdata-sections",
               "-I" + str(ziran / "include"), *["-iquote" + str(p) for p in includes],
               *sorted(generated.rglob("*.c")), raylib, runtime,
               "-Wl,--gc-sections", "-Wl,--export-dynamic-symbol=CORE",
               *["-Wl,--wrap=" + name for name in wraps],
               *libraries, "-o", executable]
    output += run(command, cwd=ROOT)
    (LOG / "pixels-build.log").write_text(output)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data))

    # Asymmetric RGBA bytes exercise colour, alpha, orientation and scaling.
    raw = b"".join(b"\0" + bytes(v for x in range(8) for v in
                                  (x * 29, y * 31, (x * 17 + y * 23) % 256, 100 + (x + y) * 10))
                   for y in range(8))
    fixture = WORK / "fixture.png"
    fixture.write_bytes(b"\x89PNG\r\n\x1a\n" +
                        chunk(b"IHDR", struct.pack(">IIBBBBB", 8, 8, 8, 6, 0, 0, 0)) +
                        chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
    font = ROOT / "assets/fonts/LiberationSans-Regular.ttf"
    receipt = {"time": datetime.now(timezone.utc).isoformat(),
               "originating_chat": "-1000000002", "serial": args.serial,
               "package": args.package, "mode": "private EGL pbuffer; hashes only",
               "inputs": {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in [executable, fixture, font, raylib, runtime]}}
    if args.build_only:
        print("Built offscreen GLES hash harness; no device access")
        return
    if args.host:
        env = {key: value for key, value in os.environ.items() if key not in {
            "DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "GDK_DISPLAY"}}
        env.update(EGL_PLATFORM="surfaceless", LIBGL_ALWAYS_SOFTWARE="1", YUE_DESKTOP_RECOVERY="0")
        result = subprocess.run([executable], cwd=ROOT, env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        (LOG / "pixels-host.log").write_text(result.stdout)
        receipt.update(exit_code=result.returncode, mode="private surfaceless software EGL pbuffer")
        (LOG / "pixels-host-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        for line in result.stdout.splitlines():
            if any(key in line for key in ["frame mismatch", "hash result", "Fatal", "error", "Error"]):
                print(line)
        raise SystemExit(result.returncode)
    adb = ["adb", "-s", args.serial]
    activities = run(adb + ["shell", "dumpsys", "activity", "activities"])
    receipt["foreground"] = [line.strip() for line in activities.splitlines()
                             if "mResumedActivity:" in line]
    print("\n".join(receipt["foreground"]), flush=True)
    if not any(args.package + "/" in line for line in receipt["foreground"]):
        raise SystemExit("The test package must be foreground before a device run")
    remote = "/data/local/tmp/kryon-frame-test"
    run(adb + ["shell", "mkdir", "-p", remote])
    for path, name in [(executable, "frame-test"), (fixture, "fixture.png"), (font, "font.ttf")]:
        run(adb + ["push", path, remote + "/" + name])
    command = ("mkdir -p files/kryon-frame-test && cp " + remote +
               "/frame-test files/kryon-frame-test/frame-test && chmod 700 files/kryon-frame-test/frame-test")
    run(adb + ["shell", "run-as", args.package, "sh", "-c", shlex.quote(command)])
    result = subprocess.run(adb + ["shell", "run-as", args.package,
                                  "./files/kryon-frame-test/frame-test"],
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
    (LOG / "pixels-device.log").write_text(result.stdout)
    receipt["exit_code"] = result.returncode
    (LOG / "pixels-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    for line in result.stdout.splitlines():
        if any(key in line for key in ["frame mismatch", "hash result", "Fatal", "error", "Error"]):
            print(line)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        LOG.mkdir(parents=True, exist_ok=True)
        output = error.stdout or ""
        (LOG / "pixels-command-failure.log").write_text(output)
        print(output[-6000:])
        raise SystemExit(error.returncode)

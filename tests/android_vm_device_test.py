"""Run Ziran Android services inside Android's real VM without a window.

Each suite is a Ziran library with one JNI entry, linked for ARMv7 and
AArch64 from source and saved IR. With --serial, a small Java host loads the
ARMv7 libraries with app_process on the USB Moto.
"""
import argparse
import os
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

FONT_HOST = '''public final class FontHost {
    private static native int run();
    public static void main(String[] args) {
        System.load(args[0]);
        int result = run();
        System.out.println("Android device test result=" + result);
        if (result != 0) System.exit(result);
    }
}
'''
PICTURE_HOST = '''public final class PictureHost {
    private native int run(android.content.res.AssetManager assets, int width, int height, int checksum);
    public static void main(String[] args) {
        System.load(args[0]);
        System.out.println("PictureHost pid=" + android.os.Process.myPid());
        int result = new PictureHost().run(android.content.res.Resources.getSystem().getAssets(),
            Integer.parseInt(args[1]), Integer.parseInt(args[2]), (int) Long.parseLong(args[3]));
        System.out.println("Android device test result=" + result);
        if (result != 0) System.exit(result);
    }
}
'''
SUITES = {
    "typefaces": dict(host="FontHost", java=FONT_HOST, codecs=False,
        done="USB Moto: source/saved real Paint/Canvas, bundled bytes/file, system fallback, "
             "UTF-16, metrics, masks and cleanup passed without a window"),
    "pictures": dict(host="PictureHost", java=PICTURE_HOST, codecs=True,
        done="USB Moto: source/saved real AssetManager PNG asset decoded on first use, "
             "AVIF and WebP textures match RGBA goldens, missing bridge reported once, "
             "missing asset and cleanup passed without a window"),
}


def prepare_pictures(run, adb, out, remote):
    """Push the texture fixtures; return the framework asset's size and ARGB
    checksum, decoded independently."""
    from PIL import Image
    for name in ("opaque.avif", "opaque.rgba", "transform31.avif", "transform31.rgba", "jungle-title.avif"):
        run([*adb, "push", ROOT / "tests/fixtures/avif" / name, remote + "/" + name], "device-" + name)
    webp = ROOT / "tests/fixtures/webp/lossless.webp"
    golden = out / "lossless.rgba"
    with Image.open(webp) as image:
        golden.write_bytes(image.convert("RGBA").tobytes())
    for path in (webp, golden):
        run([*adb, "push", path, remote + "/" + path.name], "device-" + path.name)
    jpeg = out / "plain.jpg"
    Image.new("RGB", (2, 1), (200, 40, 10)).save(jpeg, quality=90)
    run([*adb, "push", jpeg, remote + "/plain.jpg"], "device-jpeg")
    apk = out / "framework-res.apk"
    run([*adb, "pull", "/system/framework/framework-res.apk", apk], "device-framework")
    with zipfile.ZipFile(apk) as archive, archive.open("assets/images/android-logo-mask.png") as stream:
        image = Image.open(stream).convert("RGBA")
        checksum = 0
        pixels = image.tobytes()
        for at in range(0, len(pixels), 4):
            red, green, blue, alpha = pixels[at:at + 4]
            checksum = (checksum * 31 + (alpha << 24 | red << 16 | green << 8 | blue)) & 0xffffffff
        return [str(image.width), str(image.height), str(checksum)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("suite", choices=sorted(SUITES))
    parser.add_argument("--serial", choices=["ZE2223BQZT"])
    args = parser.parse_args()
    suite = SUITES[args.suite]
    out = ROOT / "build/scratch" / ("android-" + args.suite)
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER"):
        env.pop(key, None)
    env.update(HARMONY_DESKTOP_RECOVERY="0", YUE_DESKTOP_RECOVERY="0")
    cache = Path(env["ZIRAN_ROOT"])
    compiler = Path(env.get("ZIRAN_BIN", cache / "build/bin/ziran"))
    sdk = Path(env.get("ANDROID_HOME", Path.home() / "Android/Sdk"))
    ndk = sdk / "ndk/28.2.13676358"
    tools = ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin"

    def run(command, name, capture=False, cwd=ROOT):
        print(name, flush=True)
        log = out / (name + ".log")
        with log.open("w") as stream:
            result = subprocess.run(list(map(str, command)), cwd=cwd, env=env, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=600)
        if result.returncode:
            with log.open("rb") as stream:
                stream.seek(max(0, log.stat().st_size - 4000))
                print(stream.read(4000).decode(errors="replace"), flush=True)
            raise SystemExit(f"{name} failed ({result.returncode}); {log}")
        if capture:
            assert log.stat().st_size < 65536
            return log.read_text().strip()

    source = ROOT / ("tests/android_" + args.suite + "_device_test.zi")
    entry_name = f"{source.stem}:Java_{suite['host']}_run"
    project = ["--project", "--module-path", str(ROOT / "tests")]
    libraries = []
    shared = []
    if suite["codecs"]:
        # Picture decoding links the same codec artifact as an application.
        import android_codec_artifact_test as codecs
        codec_env = codecs.codec_environment()
        consumer = codecs.codec_consumer(codec_env)
    for abi, prefix, cpu, define in (("armv7", "armv7a-linux-androideabi", "none", "__arm__"),
                                      ("aarch64", "aarch64-linux-android", "arm64", "__aarch64__")):
        definitions = ["--target-os", "android", "--target-cpu", cpu, "--define", "ANDROID_BUILD", "--define", define]
        ir = out / abi / "ir"
        flags = []
        if suite["codecs"]:
            _, meta, flags = codecs.codec_artifact(consumer, abi, codec_env)
            if abi == "armv7":
                shared = list(map(Path, meta["shared_libraries"]))
        run([compiler, "ir", *project, "--root", source.parent, *definitions,
             "--entry", entry_name, "-o", ir, source], abi + "-generate-ir")
        for form in ("source", "saved"):
            entry = source if form == "source" else ir / source.with_suffix(".zir").name
            generated = out / abi / form
            name = abi + "-" + form
            run([compiler, "build", *(project if form == "source" else []), "--target=c", "--root", entry.parent,
                 *definitions, "--entry", entry_name, "--prune-stale", "-o", generated, entry], name + "-generate")
            sources = sorted(generated.rglob("*.c"))
            library = generated / "libdevicetest.so"
            run([tools / (prefix + "21-clang"), "-std=c11", "-O1", "-fPIC", "-shared",
                 "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-Wl,--no-undefined",
                 "-I" + str(cache / "include"), *["-iquote" + str(p) for p in sorted({s.parent for s in sources})],
                 *sources, *flags, "-landroid", "-llog", "-ldl", "-lm", "-lz", "-o", library], name + "-link")
            run([tools / "llvm-nm", "-D", "--defined-only", library], name + "-symbols")
            with (out / (name + "-symbols.log")).open() as symbols:
                assert any(line.rstrip().endswith(f" Java_{suite['host']}_run") for line in symbols), library
            if abi == "armv7":
                libraries.append((form, library))
    if not args.serial:
        print(f"Android {args.suite} source/saved NDK link passed on ARMv7 and AArch64; device execution not requested")
        return
    java = out / (suite["host"] + ".java")
    java.write_text(suite["java"])
    classes = out / "classes"
    classes.mkdir(exist_ok=True)
    android_jar = sorted((sdk / "platforms").glob("android-*/android.jar"))[-1]
    run(["javac", "--release", "8", "-classpath", android_jar, "-d", classes, java], "java-host")
    dex = out / "dex"
    dex.mkdir(exist_ok=True)
    d8 = sorted((sdk / "build-tools").glob("*/d8"))[-1]
    run([d8, "--min-api", "21", "--lib", android_jar, "--output", dex,
         classes / (suite["host"] + ".class")], "dex-host")
    adb = ["adb", "-s", args.serial]
    remote = f"/data/local/tmp/kryon-android-{args.suite}-test"
    run([*adb, "shell", "mkdir", "-p", remote], "device-directory")
    try:
        arguments = []
        if args.suite == "typefaces":
            run([*adb, "push", ROOT / "assets/fonts/LiberationSans-Regular.ttf",
                 remote + "/LiberationSans-Regular.ttf"], "device-font")
        else:
            arguments = prepare_pictures(run, adb, out, remote)
        for library in shared:
            run([*adb, "push", library, remote + "/" + library.name], "device-" + library.stem)
        run([*adb, "push", dex / "classes.dex", remote + "/classes.dex"], "device-dex")
        for form, library in libraries:
            run([*adb, "push", library, remote + "/libdevicetest.so"], form + "-device-library")
            result = run([*adb, "shell", f"CLASSPATH={remote}/classes.dex", f"LD_LIBRARY_PATH={remote}",
                          "/system/bin/app_process", "/system/bin",
                          suite["host"], remote + "/libdevicetest.so", *arguments], form + "-device-run", capture=True)
            assert "Android device test result=0" in result, result[-3000:]
            if args.suite == "pictures":
                pid = next(line.removeprefix("PictureHost pid=") for line in result.splitlines()
                           if line.startswith("PictureHost pid="))
                assert pid.isdecimal(), result
                log = run([*adb, "logcat", "-d", f"--pid={pid}", "-s", "Kryon"], form + "-logcat", capture=True)
                assert log.count("has no decodeBundleImage") == 1, log[-2000:]
            if args.suite == "typefaces":
                leftovers = run([*adb, "shell", "find", remote, "-name", "kryon-font-*"], form + "-temporary-fonts", capture=True)
                assert not leftovers, leftovers
    finally:
        run([*adb, "shell", "rm", "-rf", remote], "device-cleanup")
    print(suite["done"])


if __name__ == "__main__":
    main()

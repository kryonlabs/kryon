"""Run Ziran Pixmap font services inside Android's real VM without a window."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/scratch/android-typefaces"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", choices=["ZE2223BQZT"])
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
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
        log = OUT / (name + ".log")
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

    source = ROOT / "tests/android_typefaces_device_test.zi"
    project = ["--project", "--module-path", str(ROOT / "tests")]
    libraries = []
    for abi, prefix, cpu, define in (("armv7", "armv7a-linux-androideabi", "none", "__arm__"),
                                      ("aarch64", "aarch64-linux-android", "arm64", "__aarch64__")):
        definitions = ["--target-os", "android", "--target-cpu", cpu, "--define", "ANDROID_BUILD", "--define", define]
        ir = OUT / abi / "ir"
        run([compiler, "ir", *project, "--root", source.parent, *definitions,
             "--entry", "android_typefaces_device_test:Java_FontHost_run", "-o", ir, source], abi + "-generate-ir")
        for form in ("source", "saved"):
            entry = source if form == "source" else ir / source.with_suffix(".zir").name
            generated = OUT / abi / form
            name = abi + "-" + form
            run([compiler, "build", *(project if form == "source" else []), "--target=c", "--root", entry.parent,
                 *definitions, "--entry", "android_typefaces_device_test:Java_FontHost_run", "--prune-stale", "-o", generated, entry], name + "-generate")
            sources = sorted(generated.rglob("*.c"))
            library = generated / "libfonttest.so"
            run([tools / (prefix + "21-clang"), "-std=c11", "-O1", "-fPIC", "-shared",
                 "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-Wl,--no-undefined",
                 "-I" + str(cache / "include"), *["-iquote" + str(p) for p in sorted({s.parent for s in sources})],
                 *sources, "-landroid", "-ldl", "-lm", "-o", library], name + "-link")
            symbols = run([tools / "llvm-nm", "-D", "--defined-only", library], name + "-symbols", capture=True)
            assert " Java_FontHost_run" in symbols, symbols[-2000:]
            if abi == "armv7":
                libraries.append((form, library))
    if not args.serial:
        print("Android font source/saved NDK link passed on ARMv7 and AArch64; device execution not requested")
        return
    java = OUT / "FontHost.java"
    java.write_text('''public final class FontHost {
    private static native int run();
    public static void main(String[] args) {
        System.load(args[0]);
        int result = run();
        System.out.println("Android font services result=" + result);
        if (result != 0) System.exit(result);
    }
}
''')
    classes = OUT / "classes"
    classes.mkdir(exist_ok=True)
    run(["javac", "--release", "8", "-d", classes, java], "java-host")
    dex = OUT / "dex"
    dex.mkdir(exist_ok=True)
    d8 = sorted((sdk / "build-tools").glob("*/d8"))[-1]
    run([d8, "--min-api", "21", "--output", dex, classes / "FontHost.class"], "dex-host")
    adb = ["adb", "-s", args.serial]
    remote = "/data/local/tmp/kryon-android-typefaces-test"
    run([*adb, "shell", "mkdir", "-p", remote], "device-directory")
    try:
        run([*adb, "push", dex / "classes.dex", remote + "/classes.dex"], "device-dex")
        run([*adb, "push", ROOT / "assets/fonts/LiberationSans-Regular.ttf", remote + "/LiberationSans-Regular.ttf"], "device-font")
        for form, library in libraries:
            run([*adb, "push", library, remote + "/libfonttest.so"], form + "-device-library")
            result = run([*adb, "shell", f"CLASSPATH={remote}/classes.dex", "/system/bin/app_process", "/system/bin", "FontHost", remote + "/libfonttest.so"], form + "-device-run", capture=True)
            assert "Android font services result=0" in result, result[-3000:]
            leftovers = run([*adb, "shell", "find", remote, "-name", "kryon-font-*"], form + "-temporary-fonts", capture=True)
            assert not leftovers, leftovers
    finally:
        run([*adb, "shell", "rm", "-rf", remote], "device-cleanup")
    print("USB Moto: source/saved real Paint/Canvas, bundled bytes/file, system fallback, UTF-16, metrics, masks and cleanup passed without a window")


if __name__ == "__main__":
    main()

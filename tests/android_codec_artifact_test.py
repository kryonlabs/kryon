"""Consume the renderer-desktop codec artifact for both Android ABIs."""
import json
import os
from pathlib import Path
import subprocess
import sys

from toolchain import ROOT, ZIRAN, ZIRAN_ROOT
sys.path.insert(0, str(ZIRAN_ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "scripts"))
from ziran_artifacts import Artifact
from renderer_artifact import renderer_metadata, renderer_link


OUT = ROOT / "build/scratch/android-codec-artifact"
NDK = Path(os.environ.get("ANDROID_NDK_HOME", Path.home() / "Android/Sdk/ndk/28.2.13676358"))
TOOLS = NDK / "toolchains/llvm/prebuilt/linux-x86_64/bin"
ABIS = {"aarch64": ("aarch64-linux-android", "arm64", "__aarch64__", "AArch64"),
        "armv7": ("armv7a-linux-androideabi", "none", "__arm__", "ARM")}


def codec_environment():
    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER"):
        env.pop(key, None)
    env.update(HARMONY_DESKTOP_RECOVERY="0", YUE_DESKTOP_RECOVERY="0", CMAKE_BUILD_PARALLEL_LEVEL="1")
    return env


def codec_consumer(env):
    """A project that consumes this checkout's renderer-desktop artifact."""
    assert (NDK / "build/cmake/android.toolchain.cmake").is_file(), "Android NDK required"
    project = OUT / "consumer"
    project.mkdir(parents=True, exist_ok=True)
    pin = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ZIRAN_ROOT, text=True).strip()
    (project / "ziran.toml").write_text(f'''[package]
name = "android_codec_consumer"
module_roots = ["."]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "{pin}"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon.renderer]
android_ndk = {json.dumps(str(NDK))}
android_api = "21"
''')
    overrides = {"kryon": str(ROOT), "ziran": str(ZIRAN_ROOT)}
    for package in ("audio", "avif", "webp", "image_codec"):
        overrides[package] = subprocess.check_output(
            [str(ZIRAN), "pkg", "path", package, "--offline"], cwd=ROOT, env=env, text=True).strip()
    (project / "ziran.local.toml").write_text("[overrides]\n" + "".join(
        f"{name} = {json.dumps(path)}\n" for name, path in overrides.items()))
    subprocess.run([str(ZIRAN), "lock", "--offline"], cwd=project, env=env, check=True)
    return project


def codec_artifact(project, arch, env):
    """The artifact, its metadata and link flags for one Android ABI."""
    compiler = TOOLS / (ABIS[arch][0] + "21-clang")
    descriptor = Path(subprocess.check_output([str(ZIRAN), "pkg", "artifact",
        "kryon:renderer-desktop", "--platform", f"android-{arch}-bionic", "--offline"],
        cwd=project, env=dict(env, CC=str(compiler)), text=True).strip())
    artifact = Artifact(descriptor)
    meta = renderer_metadata(artifact)
    return artifact, meta, renderer_link(meta)


def main():
    env = codec_environment()
    project = codec_consumer(env)
    source = project / "main.zi"
    source.write_text((ROOT / "tests/android_codec_artifact_test.zi").read_text())
    for arch in ("aarch64", "armv7"):
        prefix, cpu, define, machine = ABIS[arch]
        compiler = TOOLS / (prefix + "21-clang")
        selected = dict(env, CC=str(compiler))
        target = ["--target-os", "android", "--target-cpu", cpu, "--define", "ANDROID_BUILD", "--define", define]
        ir = OUT / arch / "ir"
        subprocess.run([str(ZIRAN), "ir", "--project", "--root", str(project), *target,
                        "--entry", "main:main", "-o", str(ir), str(source)], cwd=project, env=selected, check=True)
        artifact, meta, flags = codec_artifact(project, arch, env)
        assert [Path(path).name for path in meta["archives"]] == [
            "libkryonwebp.a", "libavif.a", "libaom.a", "libopus.a"]
        assert meta["compile_flags"] == [] and meta["include_directories"] == []
        assert meta["defines"] == ["ANDROID_BUILD"]
        forbidden = ("raylib", "sdl", "cairo", "opengl", "gles", "-legl", "opensles", "mp3", "vorbis", "samplerate")
        assert not any(word in str(flags).lower() for word in forbidden), flags
        for name in ("LICENSE-libwebp", "PATENTS-libwebp", "LICENSE-libavif", "LICENSE-libaom",
                     "PATENTS-libaom", "LICENSE-dav1d", "LICENSE-opus"):
            assert artifact.output("resources/" + name).stat().st_size > 0
        for form in ("source", "saved"):
            generated = OUT / arch / form
            entry = source if form == "source" else ir / "main.zir"
            subprocess.run([str(ZIRAN), "build", "--project", "--target=c", "--root", str(entry.parent), *target,
                "--entry", "main:main", "-o", str(generated), str(entry)], cwd=project, env=selected, check=True)
            sources = sorted(generated.rglob("*.c"))
            binary = generated / "consumer.so"
            link = [str(compiler), "-O1", "-fPIC", "-shared", "-Wl,--no-undefined",
                "-I" + str(ZIRAN_ROOT / "include"),
                *["-iquote" + str(path) for path in sorted({p.parent for p in sources})],
                *map(str, sources), *flags, "-o", str(binary)]
            (OUT / f"{arch}-{form}-link.json").write_text(json.dumps(link, indent=2) + "\n")
            subprocess.run(link, cwd=ROOT, env=env, check=True)
            for library in (binary, *map(Path, meta["shared_libraries"])):
                header = subprocess.check_output([str(TOOLS / "llvm-readelf"), "-h", str(library)], text=True)
                assert machine in header, header
                dynamic = subprocess.check_output([str(TOOLS / "llvm-readelf"), "-d", str(library)], text=True)
                assert not any(word in dynamic.lower() for word in forbidden), dynamic
            print(f"{arch} {form}: verified codec descriptor, all four APIs link, no renderer dependencies", flush=True)
    print("Android codec artifact source/saved NDK link passed for ARMv7 and AArch64; no device execution")


if __name__ == "__main__":
    main()

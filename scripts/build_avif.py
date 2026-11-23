"""Build shared AVIF libraries without exposing in-progress CMake output."""
import argparse
import fcntl
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from codec_configure import configure


def publish(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    staged = tempfile.NamedTemporaryFile(dir=target.parent, prefix=f".{target.name}.", delete=False)
    replacement = Path(staged.name)
    try:
        with staged, source.open("rb") as archive:
            shutil.copyfileobj(archive, staged)
        os.chmod(replacement, source.stat().st_mode & 0o777)
        replacement.replace(target)
    finally:
        replacement.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--jobs", required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--aom-archive", type=Path)
    parser.add_argument("--decode-only", action="store_true",
                        help="dav1d decode without libaom; --aom-archive is not accepted")
    parser.add_argument("--shared-library", type=Path)
    parser.add_argument("--input", type=Path, action="append", default=[])
    parser.add_argument("config", nargs="+")
    args = parser.parse_intermixed_args()
    if args.decode_only == (args.aom_archive is not None):
        parser.error("pass either --decode-only or --aom-archive")
    inputs = dict(argument.split("=", 1) for argument in args.config)
    cmake_build = args.build / "cmake-build"
    outputs = {"avif/libavif.a": args.archive}
    if not args.decode_only:
        outputs["aom/libaom.a"] = args.aom_archive
    targets = ["avif_static"]
    if args.shared_library:
        outputs["libkryonavif.so"] = args.shared_library
        targets.append("kryonavif")

    args.build.mkdir(parents=True, exist_ok=True)
    with (args.build / "kryon-build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        config = configure(cmake_build, inputs)
        done = args.build / "kryon-built"
        changed = max(path.stat().st_mtime_ns for path in [config, *args.input])
        if (done.is_file() and done.stat().st_mtime_ns >= changed and
                all(path.is_file() and path.stat().st_size > 0 for path in outputs.values())):
            return
        done.unlink(missing_ok=True)
        mode = ["-DDECODE_ONLY=ON"] if args.decode_only else [f"-DAOM_DIR={inputs['aom']}"]
        subprocess.run([
            "cmake", "-S", inputs["source"], "-B", str(cmake_build),
            f"-DAVIF_DIR={inputs['avif']}", *mode,
            f"-DDAV1D_DIR={inputs['dav1d']}", f"-DKRYON_AVIF_PACKAGE_DIR={inputs['package']}",
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5", "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_C_COMPILER={inputs['compiler']}",
        ], check=True)
        subprocess.run(["cmake", "--build", str(cmake_build), "--target", *targets,
                        "--parallel", args.jobs], check=True)
        for name, target in outputs.items():
            publish(cmake_build / name, target)
        done.touch()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generate the complete checked Zi graph and its CMake source manifest.

Resolution follows the project, not flat module paths: `ziran ir --project`
namespaces every package's modules, so an application module may share a
name with a Kryon module. Run through scripts/android-build.sh for
`kryon build --profile android`; concurrent ABIs serialize through one lock.
"""
import argparse
import fcntl
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--ziran", type=Path, required=True)
parser.add_argument("--zi2c", type=Path, required=True)
parser.add_argument("--root", type=Path, required=True)
parser.add_argument("--kryon", type=Path, required=True)
parser.add_argument("--ir", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--manifest", type=Path, required=True)
parser.add_argument("--define", action="append", default=[])
args = parser.parse_args()
root = args.root.resolve()
kryon = args.kryon.resolve()
sources = sorted((root / "src").rglob("*.zi"))
if list((root / "src").rglob("*.c")) or list((root / "src").rglob("*.kry")):
    raise SystemExit("Android application implementation must be current Zi")
if not sources:
    raise SystemExit("No Zi sources under src/")
host_roots = [
    kryon / "src/backend/android_run.zi",
    root / "src/main.zi",
    root / "src/android_glue.zi",
]
for path in host_roots:
    if not path.is_file():
        raise SystemExit(f"Missing host source {path}; run scripts/android_scaffold.py")

defines = []
for name in args.define:
    defines.extend(["--define", name])

if args.ir.exists():
    shutil.rmtree(args.ir)
if args.output.exists():
    shutil.rmtree(args.output)
args.ir.mkdir(parents=True)
args.output.mkdir(parents=True)

lock_path = root / "build" / "kryon-android-compiler.lock"
lock_path.parent.mkdir(parents=True, exist_ok=True)
with lock_path.open("w") as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    subprocess.run(
        [str(args.ziran), "ir", "--project", *defines, "-o", str(args.ir),
         *map(str, host_roots)],
        cwd=root, check=True,
    )
    subprocess.run(
        [str(args.zi2c), "--no-main", *defines, "--root", str(args.ir),
         "-o", str(args.output), *map(str, args.ir.glob("*.zir"))],
        cwd=root, check=True,
    )

outputs = sorted(args.output.rglob("*.c"))
if not outputs:
    raise SystemExit("Zi compiler emitted no Android native sources")
if not any(path.name == "main.c" for path in outputs):
    raise SystemExit("The Android host needs the scaffold's src/main.zi entry")
headers = sorted(args.output.rglob("*.h"))
include_dirs = sorted({args.output, *(p.parent for p in headers)})
lines = ["# Generated from the complete Zi import graph.",
         "set(APP_ZIRAN_GENERATED_SOURCES"]
lines.extend(f'    "{path}"' for path in outputs)
lines.extend([")", "set(APP_ZIRAN_GENERATED_HEADERS"])
lines.extend(f'    "{path}"' for path in headers)
lines.extend([")", "set(APP_ZIRAN_INCLUDE_DIRS"])
lines.extend(f'    "{path}"' for path in include_dirs)
lines.append(")\n")
content = "\n".join(lines)
args.manifest.parent.mkdir(parents=True, exist_ok=True)
if not args.manifest.exists() or args.manifest.read_text() != content:
    args.manifest.write_text(content)
print(f"Generated {len(outputs)} Android C outputs from maintained Zi sources")

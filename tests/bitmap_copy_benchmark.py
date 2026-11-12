#!/usr/bin/env python3
"""Shared BitmapCopyPixels timing; run through build-queue."""
import json
import os
import shlex
from pathlib import Path
import subprocess
import numpy as np
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

OUT = ROOT / "build/scratch/bitmap-store/copy"

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    ir, generated = OUT / "ir", OUT / "c"
    for directory, suffix in ((ir, "*.zir"), (generated, "*.c"), (generated, "*.h")):
        for old in directory.glob(suffix):
            old.unlink()
    entry = "bitmap_copy_benchmark:main"
    subprocess.run([str(ZIRAN), "ir", "--project", "--root", str(ROOT / "tests"),
                    "--entry", entry, "-o", str(ir),
                    str(ROOT / "tests/bitmap_copy_benchmark.zi")], cwd=ROOT, check=True)
    subprocess.run([str(ZIRAN.with_name("zi2c")), "--entry", entry, "--root", str(ir),
                    "-o", str(generated), *map(str, sorted(ir.glob("*.zir")))],
                   cwd=ROOT, check=True)
    executable = OUT / "bitmap_copy_benchmark"
    compiler=shlex.split(os.environ.get("KRYON_TEST_CC",os.environ.get("CC","cc")))
    subprocess.run([*compiler,
                    "-std=c11", "-O2", "-ffunction-sections", "-fdata-sections",
                    "-I" + str(ZIRAN_ROOT / "include"), "-iquote", str(generated),
                    *map(str, sorted(generated.glob("*.c"))),
                    "-Wl,--gc-sections", "-lm", "-o", str(executable)], cwd=ROOT, check=True)
    env = dict(os.environ)
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                 "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER", "DESKTOP_SESSION"):
        env.pop(name, None)
    env["HARMONY_DESKTOP_RECOVERY"] = "0"
    process = subprocess.Popen([str(executable)], cwd=ROOT, env=env,
                               stdout=subprocess.PIPE, text=True)
    rows = {}
    with (OUT / "copy.log").open("w") as log:
        for line in process.stdout:
            log.write(line)
            if line.startswith("copy frame "):
                row = line.split()
                rows.setdefault((int(row[2]), int(row[3]), int(row[4])), []).append(
                    [float(row[6]), float(row[7])])
    if process.wait():
        raise RuntimeError("Bitmap copy equivalence/retention benchmark failed")
    result = {}
    for (width, height, mixed), samples in rows.items():
        assert len(samples) >= 120
        result[f"{width}x{height}-{'mixed' if mixed else 'opaque'}"] = {
            name: dict(zip(("p50", "p95", "p99"),
                           np.percentile(np.array(samples)[:, column], (50, 95, 99)).tolist()))
            for column, name in enumerate(("reference_ms", "actual_ms"))}
    (OUT / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()

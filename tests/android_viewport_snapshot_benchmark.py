#!/usr/bin/env python3
"""Run with python3 tests/android_viewport_snapshot_benchmark.py on local Linux.

Builds the shared Android snapshot path and a headless pixel and fade oracle.
Writes timings, raw pairs and build identities to build/scratch/android-viewport-snapshot/result.json.
"""
from datetime import datetime, timezone
import csv
import fcntl
import hashlib
import json
import math
import os
import platform
import shlex
import statistics
import subprocess

from toolchain import BIN, ROOT, ZIRAN, ZIRAN_ROOT


FIXTURE = "android_viewport_snapshot_benchmark"
WORK = ROOT / "build/scratch/android-viewport-snapshot"
SOURCES = (
    f"tests/{FIXTURE}.zi", f"tests/{FIXTURE}.py", "tests/toolchain.py", "ziran.toml",
    "src/backend/android_native.zi", "src/backend/frame_texture_android.zi",
    "src/ui/bitmap_store.zi", "src/ui/frame_transition.zi",
)
FLAGS = ["-std=c11", "-O2", "-ffunction-sections", "-fdata-sections"]
FIELDS = ["pair", "full_capture_ns", "viewport_capture_ns"]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def summarize(output):
    oracle, surface, viewport, warmup, header, *data = list(csv.reader(output.splitlines()))
    if oracle != ["oracle", "passed"] or header != FIELDS:
        raise ValueError("snapshot oracle or timing output is missing")
    if surface[0] != "surface" or viewport[0] != "viewport" or warmup[0] != "warmup_pairs":
        raise ValueError("snapshot fixture metadata is missing")
    pairs = [list(map(int, row)) for row in data]
    if len(pairs) != 40 or any(index != pair or min(full, cropped) <= 0
                              for index, (pair, full, cropped) in enumerate(pairs)):
        raise ValueError("expected 40 ordered pairs of positive capture durations")

    report = {"pixel_oracle": "passed", "surface_pixels": list(map(int, surface[1:])),
              "viewport_xywh_pixels": list(map(int, viewport[1:])),
              "warmup_pairs": int(warmup[1]), "sample_pairs": len(pairs),
              "pair_order": "alternate full and viewport starting order across all pairs, including warmups"}
    for column, name in enumerate(("full_capture_ms", "viewport_capture_ms"), start=1):
        values = sorted(row[column] / 1e6 for row in pairs)
        report[name] = {"median": statistics.median(values), "p95": values[math.ceil(len(values) * .95) - 1]}
    full = report["full_capture_ms"]["median"]
    cropped = report["viewport_capture_ms"]["median"]
    report["median_capture_reduction_percent"] = (full - cropped) / full * 100
    return report, [dict(zip(FIELDS, row)) for row in pairs]


def main():
    if platform.system() != "Linux":
        raise RuntimeError("this headless benchmark requires a Linux host")
    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                "SESSION_MANAGER", "DESKTOP_SESSION"):
        env.pop(key, None)
    env.update(HARMONY_DESKTOP_RECOVERY="0", YUE_DESKTOP_RECOVERY="0")
    compiler = shlex.split(env.get("KRYON_TEST_CC", env.get("CC", "cc")))
    archive = BIN.parent / "libziran.a"
    inputs = {name: ROOT / name for name in SOURCES}
    inputs.update({"ziran": ZIRAN, "zi2zir": ZIRAN.with_name("zi2zir"),
                   "zi2c": ZIRAN.with_name("zi2c"), "libziran.a": archive})
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        hashes = {name: digest(path) for name, path in inputs.items()}
        source_revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        generated = WORK / "c"
        binary = WORK / FIXTURE
        subprocess.run([ZIRAN, "build", "--project", "--module-path", ROOT / "tests", "--target=c",
                        "--root", ROOT / "tests", "--define", "ANDROID_BUILD", "--prune-stale",
                        "-o", generated, ROOT / f"tests/{FIXTURE}.zi"],
                       cwd=ROOT, env=env, check=True, timeout=180)
        modules = sorted(generated.rglob("*.c"))
        includes = [arg for directory in sorted({p.parent for p in generated.rglob("*.h")} | {generated})
                    for arg in ("-iquote", directory)]
        subprocess.run([*compiler, *FLAGS, "-I", ZIRAN_ROOT / "include", *includes, *modules, archive,
                        "-Wl,--gc-sections", "-ldl", "-lm", "-lpthread", "-o", binary],
                       cwd=ROOT, env=env, check=True, timeout=180)
        if hashes != {name: digest(path) for name, path in inputs.items()}:
            raise RuntimeError("snapshot source or toolchain files changed during the build")

        run = subprocess.run([binary], cwd=WORK, env=env, text=True, stdout=subprocess.PIPE, timeout=30)
        if run.returncode:
            print(run.stdout, end="", flush=True)
            run.check_returncode()
        report, samples = summarize(run.stdout)
        report["measurement"] = "Local Linux monotonic wall time. Fade replay and disposal occur outside the capture timer. All raw pairs retained."
        with open("/proc/cpuinfo") as cpuinfo:
            cpu = next((line.partition(":")[2].strip() for line in cpuinfo if line.startswith("model name")),
                       platform.processor())
        report["provenance"] = {
            "recorded_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "host": platform.node(), "system": platform.system(), "kernel": platform.release(),
            "machine": platform.machine(), "cpu": cpu, "kryon_head": source_revision,
            "ziran": str(ZIRAN),
            "ziran_version": subprocess.check_output([ZIRAN, "--version"], env=env, text=True, timeout=10).strip(),
            "compiler": compiler,
            "compiler_version": subprocess.check_output([*compiler, "--version"], env=env, text=True, timeout=10).splitlines()[0],
            "compiler_flags": FLAGS, "define": "ANDROID_BUILD", "input_sha256": hashes,
            "binary_sha256": digest(binary),
        }
        (WORK / "result.json").write_text(json.dumps({**report, "pairs": samples}, indent=2) + "\n")
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

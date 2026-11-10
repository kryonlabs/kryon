#!/usr/bin/env python3
"""Build and run a reproducible ComponentHost baseline without display access."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/scratch/component-host-benchmark"
DEFAULT_OUTPUT = ROOT / "backup/component-host-benchmark/results.json"
DEFAULT_LOG = ROOT / "backup/component-host-benchmark/run.log"
DEFAULT_PROFILE = ROOT / "backup/component-host-benchmark/perf.txt"
COMMANDS: list[str] = []
BINDS = [
    ("--bind", "font_metrics:MeasureGlyphWidth=component_raster_host:ComponentMeasureGlyphWidth"),
    ("--bind", "font_metrics:MeasureGlyphLineHeight=component_raster_host:ComponentMeasureGlyphLineHeight"),
    ("--bind", "image_raster:ImageWidth=component_raster_host:ComponentImageWidth"),
    ("--bind", "image_raster:ImageHeight=component_raster_host:ComponentImageHeight"),
    ("--bind", "raster_text:RasterText=component_raster_host:ComponentRasterText"),
    ("--bind", "raster_text:RasterTextClipped=component_raster_host:ComponentRasterTextClipped"),
    ("--bind", "raster_text:RasterTextRotated=component_raster_host:ComponentRasterTextRotated"),
    ("--bind", "raster_shape:RasterRoundedRectangle=component_raster_host:ComponentRasterRoundedRectangle"),
    ("--bind", "raster_shape:RasterRoundedRectangleOutline=component_raster_host:ComponentRasterRoundedRectangleOutline"),
    ("--bind", "raster:RasterLine=component_raster_host:ComponentRasterLine"),
    ("--bind", "raster_triangles:RasterTriangles=component_raster_host:ComponentRasterTriangles"),
    ("--bind", "system_clipboard:SystemClipboardOpenHost=component_raster_host:ComponentSystemClipboardOpenHost"),
    ("--bind", "system_clipboard:SystemClipboardByteHost=component_raster_host:ComponentSystemClipboardByteHost"),
    ("--bind", "system_clipboard:SystemClipboardWriteHost=component_raster_host:ComponentSystemClipboardWriteHost"),
    ("--bind", "paint_queue:RasterImage=component_raster_host:ComponentRasterImage"),
    ("--bind", "paint_queue:RasterImageMesh=component_raster_host:ComponentRasterImageMesh"),
]


def run(command: list[str], *, cwd: Path = ROOT, env: dict[str, str]) -> None:
    COMMANDS.append(shlex.join(command))
    subprocess.run(command, cwd=cwd, env=env, check=True)


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def percentile(values: list[int], fraction: float) -> int:
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    weight = position - lower
    return round(ordered[lower] * (1.0 - weight) + ordered[upper] * weight)


def module_paths(ziran_root: Path) -> list[str]:
    paths = [str(ROOT / "src/backend"), str(ROOT / "src/ui")]
    paths.extend(str(path) for path in sorted((ROOT / "src/ui").rglob("*")) if path.is_dir())
    paths.append(str(ziran_root / "std"))
    return paths


def build_guest(env: dict[str, str], ziran: Path, ziran_root: Path, name: str,
                source: Path, define: str | None) -> dict[str, Any]:
    ir = WORK / f"{name}-ir"
    output = WORK / f"{name}.zx"
    source_output = WORK / f"{name}-source.zx"
    ir_command = [str(ziran), "ir", "--root", str(source.parent),
                  "--define", "KRYON_HOSTED_UI", *module_args(ziran_root),
                  "-o", str(ir), str(source)]
    if define:
        ir_command[6:6] = ["--define", define]
    run(ir_command, env=env)
    bundle_command = [str(ziran), "bundle", "--root", str(ir),
                      "--module-path", str(ir), "--define", "KRYON_HOSTED_UI",
                      "--entry", "component:Frame", "-o", str(output),
                      str(ir / "component.zir")]
    run(bundle_command, env=env)
    direct_command = [str(ziran), "bundle", "--root", str(source.parent),
                      "--define", "KRYON_HOSTED_UI", *module_args(ziran_root),
                      "--entry", "component:Frame", "-o", str(source_output), str(source)]
    if define:
        direct_command[6:6] = ["--define", define]
    run(direct_command, env=env)
    if output.read_bytes() != source_output.read_bytes():
        raise RuntimeError(f"{name}: source and saved-IR bundles differ")
    return {"name": name, "path": str(output), "bytes": output.stat().st_size,
            "sha256": digest(output), "saved_ir_reproducible": True}


def module_args(ziran_root: Path) -> list[str]:
    result: list[str] = []
    for path in module_paths(ziran_root):
        result.extend(("--module-path", path))
        if Path(path).is_relative_to(ROOT / "src/ui"):
            result.extend(("--module-path", f"kryon={path}"))
    return result


def build(env: dict[str, str], ziran: Path, ziran_root: Path) -> tuple[dict[str, Any], list[str]]:
    WORK.mkdir(parents=True, exist_ok=True)
    guest = build_guest(env, ziran, ziran_root, "benchmark_v1",
                        ROOT / "tests/fixtures/component_host_benchmark/component.zi", None)
    replacement = build_guest(env, ziran, ziran_root, "benchmark_v2",
                              ROOT / "tests/fixtures/component_host_benchmark/component.zi",
                              "COMPONENT_HOST_BENCHMARK_V2")
    bad_abi = build_guest(env, ziran, ziran_root, "bad_abi",
                          ROOT / "tests/fixtures/component_bad_abi/component.zi", None)
    bad_migrated = build_guest(env, ziran, ziran_root, "bad_migrated",
                               ROOT / "tests/fixtures/component_bad_migrated/component.zi", None)
    truncated = WORK / "truncated.zx"
    truncated.write_bytes((WORK / "benchmark_v1.zx").read_bytes()[:16])

    codecs = WORK / "component_benchmark_codecs.zi"
    run([sys.executable, str(ROOT / "scripts/generate-widget-codecs.py"), "--complete",
         "--uses", str(ROOT / "src/backend/widget_codec.zi"), "--import", '"widget_codec"',
         "--output", str(codecs)], env=env)
    generated = WORK / "c"
    host_build = [str(ziran), "build", "--target=c", "--no-main",
                  "--entry", "component_host_benchmark_host:main",
                  "--root", str(ROOT / "tests"), *module_args(ziran_root),
                  "--module-path", str(WORK),
                  *[argument for bind in BINDS for argument in bind],
                  "-o", str(generated),
                  str(ROOT / "tests/component_host_benchmark_host.zi")]
    run(host_build, env=env)
    objects = sorted(generated.glob("*.c"))
    compiler = os.environ.get("CC", "cc")
    executable = WORK / "run"
    compile_command = [compiler, "-O2", "-std=c11", "-ffunction-sections",
                       "-fdata-sections", f"-I{generated}", f"-I{ziran_root / 'include'}",
                       *[str(path) for path in objects],
                       os.environ.get("ZIRAN_LIB", str(ziran_root / "build/libziran.a")),
                       "-Wl,--gc-sections", "-lm", "-lpthread", "-o", str(executable)]
    run(compile_command, cwd=WORK, env=env)
    identities = {"guest": guest, "replacement": replacement,
                  "bad_abi": bad_abi, "bad_migrated": bad_migrated,
                  "truncated_bytes": 16,
                  "truncated_sha256": digest(truncated),
                  "host_executable_sha256": digest(executable)}
    return identities, COMMANDS


def execute_host(env: dict[str, str], warmup: int, frames: int,
                 profile: bool = False) -> subprocess.CompletedProcess[str]:
    host_env = dict(env)
    host_env.update(BENCH_COMPONENT_WARMUP=str(warmup),
                    BENCH_COMPONENT_FRAMES=str(frames))
    command = [str(WORK / "run")]
    if profile and shutil.which("perf"):
        command = ["perf", "record", "-o", str(WORK / "perf.data"), "-F", "999",
                   "-g", "--", *command]
    COMMANDS.append(shlex.join(command))
    return subprocess.run(command, cwd=WORK, env=host_env, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)


def parse_run(completed: subprocess.CompletedProcess[str]) -> dict[str, Any]:
    if completed.returncode != 0:
        raise RuntimeError(f"host failed ({completed.returncode}):\n{completed.stdout}\n{completed.stderr}")
    lines = completed.stdout.splitlines()
    identity = [line for line in lines if line.startswith("IDENTITY ")]
    code = [int(line.rsplit("=", 1)[1]) for line in lines if line.startswith("CODE_BYTE ")]
    capabilities = [line for line in lines if line.startswith("CAPABILITY ")]
    samples = []
    checks = []
    rejects = []
    replacements = []
    summary = []
    for line in lines:
        fields = {part.split("=", 1)[0]: part.split("=", 1)[1]
                  for part in line.split()[1:] if "=" in part}
        if line.startswith("SAMPLE "):
            fields["frame_ns"] = int(fields["frame_ns"])
            fields["index"] = int(fields["index"])
            fields["tree_count"] = int(fields["tree_count"])
            fields["raster_calls"] = int(fields["raster_calls"])
            fields["raster_hash"] = int(fields["raster_hash"])
            samples.append(fields)
        elif line.startswith("CHECK "):
            checks.append(fields)
        elif line.startswith("REJECT "):
            rejects.append(fields)
        elif line.startswith("REPLACE "):
            fields["index"] = int(fields["index"])
            fields["frame_ns"] = int(fields["frame_ns"])
            replacements.append(fields)
        elif line.startswith("SUMMARY "):
            summary.append(fields)
    warmup = [row for row in samples if row["phase"] == "warmup"]
    measured = [row for row in samples if row["phase"] == "measured"]
    if len(identity) != 1 or len(code) != 32 or not capabilities:
        raise RuntimeError("missing engine/code/capability identity")
    if any(row["tree_count"] != 19 or row["raster_calls"] <= 0 or row["raster_hash"] == 0
           for row in samples):
        raise RuntimeError("invalid retained-tree or raster sample")
    if len(rejects) != 3 or any(row["accepted"] != "0" or row["live_handle"] != "1"
                               for row in rejects):
        raise RuntimeError("replacement rejection rollback failed")
    if len(replacements) != 7 or any(row["accepted"] != "1" for row in replacements):
        raise RuntimeError("replacement samples failed")
    if len(checks) != 3 or len(summary) != 1:
        raise RuntimeError("host assertions incomplete")
    measured_ns = [row["frame_ns"] for row in measured]
    return {
        "identity": identity[0],
        "code_identity_sha256": hashlib.sha256(bytes(code)).hexdigest(),
        "capabilities": capabilities, "warmup_samples": warmup,
        "measured_samples": measured, "checks": checks, "rejects": rejects,
        "replacements": replacements, "summary": summary[0],
        "measured_frame_ns": {
            "min": min(measured_ns), "p50": percentile(measured_ns, 0.50),
            "p90": percentile(measured_ns, 0.90), "p95": percentile(measured_ns, 0.95),
            "p99": percentile(measured_ns, 0.99), "max": max(measured_ns),
        },
    }


def perf_profile(env: dict[str, str], output: Path) -> dict[str, Any]:
    if shutil.which("perf") is None:
        return {"status": "unavailable", "reason": "perf not installed"}
    completed = execute_host(env, 64, 5000, profile=True)
    if completed.returncode != 0:
        return {"status": "unavailable", "reason": completed.stderr.strip()[-2000:]}
    parse_run(completed)
    report = subprocess.run(["perf", "report", "-i", str(WORK / "perf.data"),
                             "--stdio", "--sort", "symbol,dso"],
                            cwd=WORK, env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False)
    if report.returncode != 0:
        return {"status": "unavailable", "reason": report.stderr.strip()[-2000:]}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(report.stdout)
    interesting = [line.strip() for line in report.stdout.splitlines()
                   if any(symbol in line for symbol in (
                       "ComponentHostCall", "WidgetHostCall", "ComponentHostFrame",
                       "BundleComponentRun"))]
    return {"status": "passed", "frames": 5000, "symbols": interesting,
            "interpretation": "observational symbol profile only; not an A/B dispatch result"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--warmup", type=int, default=32)
    parser.add_argument("--frames", type=int, default=200)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--log", type=Path, default=DEFAULT_LOG)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    if args.warmup < 30 or args.frames < 200:
        parser.error("minimum is 30 warmup and 200 measured frames")
    output = args.output.resolve()
    log = args.log.resolve()
    if not str(output).startswith(str(ROOT)) or not str(log).startswith(str(ROOT)):
        parser.error("output paths must remain inside the Kryon repository")
    subprocess.run(["git", "check-ignore", "-q", str(output)], cwd=ROOT, check=True)
    subprocess.run(["git", "check-ignore", "-q", str(log)], cwd=ROOT, check=True)

    ziran_root_text = os.environ.get("ZIRAN_ROOT")
    if ziran_root_text:
        ziran_root = Path(ziran_root_text).resolve()
    else:
        canonical = Path("/mnt/storage/Projects/ziranlang/ziran")
        if not (canonical / "build/bin/ziran").is_file():
            raise RuntimeError("ZIRAN_ROOT is required when the canonical sibling toolchain is absent")
        ziran_root = canonical.resolve()
    ziran = Path(os.environ.get("ZIRAN_BIN", ziran_root / "build/bin/ziran")).resolve()
    env = dict(os.environ)
    for key in list(env):
        if key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                   "SESSION_MANAGER") or key.startswith(("SDL_", "VK_", "MESA_", "BENCH_COMPONENT_")):
            env.pop(key)
    env.update(ZIRAN_PAR_THREADS="1", MAKEFLAGS="-j1", TMPDIR=str(WORK))
    WORK.mkdir(parents=True, exist_ok=True)
    if WORK.exists():
        for child in WORK.iterdir():
            if child.is_dir():
                shutil.rmtree(child)
            else:
                child.unlink()

    started = time.time()
    load_average_start = os.getloadavg()
    source_files = [
        ROOT / "tests/component_host_benchmark.py",
        ROOT / "tests/component_host_benchmark_host.zi",
        ROOT / "tests/fixtures/component_host_benchmark/component.zi",
        ROOT / "src/backend/component_host.zi",
    ]
    source_before = {str(path.relative_to(ROOT)): digest(path) for path in source_files}
    identities, commands = build(env, ziran, ziran_root)
    completed = execute_host(env, args.warmup, args.frames)
    parsed = parse_run(completed)
    if len(parsed["warmup_samples"]) != args.warmup or len(parsed["measured_samples"]) != args.frames:
        raise RuntimeError("sample count does not match requested boundaries")
    output.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(completed.stdout)
    if completed.stderr:
        log.with_suffix(".stderr").write_text(completed.stderr)
    profile = perf_profile(env, DEFAULT_PROFILE) if args.profile else {
        "status": "not-requested",
        "reason": "pass --profile for observational perf symbols; no parity or A/B claim",
    }
    source_after = {str(path.relative_to(ROOT)): digest(path) for path in source_files}
    source_changed_during_run = source_before != source_after
    report = {
        "status": "passed" if not source_changed_during_run else "failed",
        "comparison_claim": "none; WAMR/kernel parity cannot be inferred from this workload",
        "started_unix_seconds": started, "completed_unix_seconds": time.time(),
        "environment": {"system": platform.platform(), "machine": platform.machine(),
                        "cpu_count": os.cpu_count(), "load_average_start": load_average_start,
                        "load_average_end": os.getloadavg()},
        "kryon": {"path": str(ROOT), "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(), "dirty": True},
        "ziran": {"root": str(ziran_root), "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ziran_root, text=True).strip(),
            "binary_sha256": digest(ziran),
            "library_sha256": digest(Path(os.environ.get(
                "ZIRAN_LIB", ziran_root / "build/libziran.a")))},
        "sources": source_before, "sources_after": source_after,
        "source_changed_during_run": source_changed_during_run,
        "driver_command": shlex.join([sys.executable, __file__, *sys.argv[1:]]),
        "commands": commands, "boundaries": {
            "timed_frame": "ComponentRasterBegin + ComponentHostFrame through EndFrame",
            "untimed": "stdout formatting and Python JSON serialization",
            "replacement": "ComponentHostReplace including candidate open, migration, private validation and commit",
        }, "identities": identities, "measurements": parsed, "profile": profile,
        "raw_log": str(log),
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    if source_changed_during_run:
        print(f"failed: source changed during run: {output}")
        return 2
    measured = parsed["measured_frame_ns"]
    print(f"passed: {output}")
    print(f"portable-zx measured p50={measured['p50'] / 1000:.1f}us "
          f"p95={measured['p95'] / 1000:.1f}us replacements={len(parsed['replacements'])}")
    print(profile["status"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

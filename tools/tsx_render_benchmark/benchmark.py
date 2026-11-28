#!/usr/bin/env python3
"""Matched persistent TSX and internal Ziran composition on physical Alpha."""

import argparse
import csv
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import select
import shutil
import signal
import statistics
import subprocess
import tempfile
import threading
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ZIRAN = ROOT.parents[1] / "ziranlang/ziran"
WORK = ROOT / "build/scratch/tsx-render-benchmark"
STARTER = ROOT / "templates/web/src"
KINDS = ("direct", "tsx")
SCENARIOS = ("toolbar", "changing_counter", "keyed_list_128", "event_counter",
             "controlled_text", "independent_96", "keyed_churn_96", "full_list_1024",
             "virtual_list_10000", "migrated_web_starter")
CASES = [(scenario, mode) for scenario in range(len(SCENARIOS))
         for mode in (("retained", "forced") if scenario < 3 else ("retained",))]
PHASES = ("input_ns", "compose_ns", "commit_ns", "raster_ns", "present_ns", "total_ns", "cpu_ns")
ORACLE = ("frame", "nodes", "paint", "emitted", "pixel_hash", "tree_hash", "behavior")
WARMUP, FRAMES, SEED = 60, 300, 19514
ALLOC = Path("/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so")
ENV = os.environ.copy()
for var in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS", "LD_PRELOAD",
            "MALLOC_TRACE", "ZIRAN_PROFILE", "LDLIBS", "CFLAGS", "CC", "LDFLAGS"):
    ENV.pop(var, None)
ENV.update(HARMONY_DESKTOP_RECOVERY="0", SDL_VIDEODRIVER="x11", SDL_AUDIODRIVER="dummy",
           ZIRAN_LEARN="off", ZIRAN_PAR_THREADS="1", ZIRAN_JOBS="1", ZIRAN_ROOT=str(ZIRAN), ZIRAN_STD=str(ZIRAN / "std"),
           CFLAGS="-O2 -DNDEBUG", CC="cc", MAKEFLAGS="-j1", LC_ALL="C")


def run(command, **kwargs):
    kwargs.setdefault("env", ENV)
    return subprocess.run(list(map(str, command)), cwd=ROOT, check=True, text=True, **kwargs)


def read_command(*command):
    return run(command, capture_output=True).stdout.strip()


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_json(path, value):
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def quantile(values, probability):
    values = sorted(values)
    position = (len(values) - 1) * probability
    lower = int(position)
    upper = min(lower + 1, len(values) - 1)
    return values[lower] + (values[upper] - values[lower]) * (position - lower)


def filter_iqr(values):
    first, third = (quantile(values, fraction) for fraction in (.25, .75))
    radius = 1.5 * (third - first)
    excluded = [index for index, value in enumerate(values) if not first - radius <= value <= third + radius]
    return [value for index, value in enumerate(values) if index not in excluded], excluded


def stats(values):
    kept, excluded = filter_iqr(values)
    return {"p50": statistics.median(kept), "p95": quantile(kept, .95),
            "raw_p50": statistics.median(values), "raw_p95": quantile(values, .95),
            "excluded_indices": excluded}


def paired(direct, tsx):
    deltas = [(b - a) / a * 100 for a, b in zip(direct, tsx, strict=True)]
    kept, excluded = filter_iqr(deltas)
    rng = random.Random(72491)
    boot = [statistics.median(rng.choices(kept, k=len(kept))) for _ in range(10000)]
    return {"percent": statistics.median(kept), "95_ci_percent": [quantile(boot, .025), quantile(boot, .975)],
            "raw_percent": deltas, "excluded_pair_indices": excluded}


def source_manifest():
    kryon_paths = [ROOT / "ziran.toml", ROOT / "ziran.lock", *HERE.glob("*.zi"),
                   *HERE.glob("*.tsx"), Path(__file__), *STARTER.rglob("*.zi"), *STARTER.rglob("*.tsx")]
    if (ROOT / "ziran.local.toml").is_file():
        kryon_paths.append(ROOT / "ziran.local.toml")
    for folder in (ROOT / "src", ROOT / "assets/fonts", ROOT / "examples/shared/screens"):
        kryon_paths.extend(path for path in folder.rglob("*") if path.is_file())
    tool_paths = [path for path in (ZIRAN / "std").rglob("*") if path.is_file()]
    tool_paths.extend(path for path in (ZIRAN / "include").rglob("*") if path.is_file())
    revisions = {project: read_command("git", "-C", checkout, "rev-parse", "HEAD")
                 for project, checkout in (("kryon", ROOT), ("ziran", ZIRAN))}
    tools = [ZIRAN / "build/bin" / name for name in ("ziran", "zi2c", "zi2zir")]
    tools.extend(Path(shutil.which(name)).resolve() for name in ("cc", "as", "ld", "Xvfb", "taskset"))
    tools.append(Path(read_command("cc", "-print-prog-name=cc1")).resolve())
    return {"revisions": revisions,
            "inputs_sha256": {str(path.relative_to(ROOT.parents[1])): sha(path) for path in sorted(kryon_paths + tool_paths)},
            "executables_sha256": {str(path): sha(path) for path in (*tools, ALLOC)}}


def cpu_stats():
    stats = {}
    for line in Path("/proc/stat").read_text().splitlines():
        cpu, _, values = line.partition(" ")
        if not cpu.startswith("cpu") or not cpu[3:].isdigit():
            continue
        ticks = list(map(int, values.split()[:8]))
        stats[int(cpu[3:])] = (sum(ticks), ticks[3] + ticks[4])
    return stats


def systext(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None


def siblings(cpu):
    return systext(f"/sys/devices/system/cpu/cpu{cpu}/topology/thread_siblings_list")


def fresh_dir(path):
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)


def executable(kind):
    return WORK / "persistent" / kind / "bench_runner"


def compile_lane(kind, cpu, fresh, log):
    destination = executable(kind).parent
    frontend_cache = WORK / "persistent" / f"frontend-{kind}"
    if fresh:
        fresh_dir(destination)
        fresh_dir(frontend_cache)
    command = ["taskset", "-c", str(cpu), ZIRAN / "build/bin/ziran", "build", "--project", "--offline",
               "--root", HERE, "--module-path", HERE, "--module-path", STARTER / "page",
               "--target=c", "--exe", "--entry", "bench_runner:main", "--define", "KRYON_NO_DESKTOP_HOST",
               "-o", destination]
    if kind == "tsx":
        command += ["--define", "TSX_BENCH"]
    command += [HERE / "bench_runner.zi"]
    build_env = dict(ENV, ZIRAN_FRONTEND_CACHE_DIR=str(frontend_cache), ZIRAN_FRONTEND_CACHE="1")
    try:
        with log.open("w") as stream:
            start = time.perf_counter_ns()
            run(command, env=build_env, stdout=stream, stderr=subprocess.STDOUT)
            elapsed = time.perf_counter_ns() - start
    except subprocess.CalledProcessError as error:
        raise RuntimeError(f"{kind} build failed. See {log}") from error
    binary = executable(kind)
    return {"argv": list(map(str, command)), "frontend_cache": str(frontend_cache), "wall_ns": elapsed,
            "bytes": binary.stat().st_size, "sha256": sha(binary), "log": log.name}


def parse_output(path, phase):
    with path.open() as source:
        evidence = {}
        def data():
            for line in source:
                if line.startswith("# "):
                    name, value = line[2:].strip().split(",")
                    evidence[name] = int(value)
                else:
                    yield line
        rows = [{key: int(value) for key, value in row.items()} for row in csv.DictReader(data())]
    count = WARMUP + FRAMES if phase == "parity" else FRAMES
    if len(rows) != count or any(evidence.get(name, 0) <= 0 for name in ("peak_rss_kib", "steady_rss_kib", "startup_ns")):
        raise RuntimeError(f"Missing samples or process metrics in {path}")
    return rows, evidence


def allocation_counts(path):
    calls = requested = 0
    with path.open() as trace:
        if next(trace, None) != "= Start\n":
            raise RuntimeError(f"Missing glibc mtrace start in {path}")
        complete = False
        for line in trace:
            if line == "= End\n":
                complete = True
                break
            match = re.search(r" (?:\+|>) 0x[0-9a-f]+ (0x[0-9a-f]+)\s*$", line)
            if match:
                calls += 1
                requested += int(match.group(1), 16)
        if not complete:
            raise RuntimeError(f"Missing glibc mtrace end in {path}")
    return {"calls": calls, "requested_bytes": requested, "calls_per_frame": calls / FRAMES,
            "requested_bytes_per_frame": requested / FRAMES, "trace": path.name}


def one_run(kind, scenario, mode, cpu, phase, raw, name):
    path = raw / f"{name}.csv"
    trace = raw / f"{name}.mtrace"
    env = ENV.copy()
    if phase == "alloc":
        env.update(LD_PRELOAD=str(ALLOC), MALLOC_TRACE=str(trace))
    frequencies = []
    stop = threading.Event()
    def observe():
        while not stop.is_set():
            sample = systext(f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_cur_freq")
            if sample is not None:
                frequencies.append(int(sample))
            stop.wait(.025)
    watcher = threading.Thread(target=observe)
    before = cpu_stats()
    load_before = os.getloadavg()
    if phase == "timing":
        watcher.start()
    with path.open("w") as stream, (raw / f"{name}.stderr").open("w") as stderr:
        try:
            launched_at = time.monotonic_ns()
            run(["taskset", "-c", cpu, executable(kind), scenario, mode, phase],
                env=env, stdout=stream, stderr=stderr)
        finally:
            stop.set()
            if phase == "timing":
                watcher.join()
    rows, evidence = parse_output(path, phase)
    evidence["launch_to_present_ns"] = evidence["presented_at_ns"] - launched_at
    if evidence["launch_to_present_ns"] < evidence["startup_ns"]:
        raise RuntimeError(f"Launch and first-frame clocks disagree in {path}")
    if phase == "alloc":
        evidence["allocation"] = allocation_counts(trace)
    if phase == "timing":
        after = cpu_stats()
        evidence["noise"] = {"load_before": load_before, "load_after": os.getloadavg(),
                             "frequency_khz": frequencies,
                             "logical_cpu_busy_fraction": {
                                 str(i): (after[i][0] - total - after[i][1] + idle) / max(1, after[i][0] - total)
                                 for i, (total, idle) in before.items()}}
        evidence["metrics"] = {key: stats([row[key] for row in rows]) for key in PHASES}
    evidence["raw"] = path.name
    return rows, evidence


def case_name(scenario, mode):
    return f"{SCENARIOS[scenario]}_{mode}"


def comparable(rows, fields=ORACLE):
    return [tuple(row[key] for key in fields) for row in rows]


def require_same(a, b, label):
    for left, right in zip(a, b, strict=True):
        if left != right:
            raise RuntimeError(f"{label}: mismatch at frame {left[0]}: {left[1:]} vs {right[1:]}")


def verify(output, raw, cpu):
    oracles = {}
    paths = {}
    for scenario, mode in CASES:
        case = case_name(scenario, mode)
        records = {}
        paths[case] = {}
        for kind in KINDS:
            records[kind], evidence = one_run(kind, scenario, mode, cpu, "parity", raw, f"{case}_{kind}_parity")
            paths[case][kind] = evidence["raw"]
        reference = records["direct"]
        require_same(comparable(reference), comparable(records["tsx"]), case)
        if mode == "forced":
            fields = ("frame", "nodes", "paint", "pixel_hash", "tree_hash", "behavior")
            require_same(comparable(oracles[case_name(scenario, "retained")], fields),
                         comparable(reference, fields), f"{case} vs retained")
        if scenario in (0, 2, 7) and mode == "retained" and any(row["emitted"] for row in reference[WARMUP:]):
            raise RuntimeError(f"{case}: unchanged frame repainted")
        if scenario == 1 and len({row["pixel_hash"] for row in reference[WARMUP:]}) != FRAMES:
            raise RuntimeError(f"{case}: frame number did not change the pixels")
        if scenario in (2, 7) and reference[0]["nodes"] != 2 + (128 if scenario == 2 else 1024) * 4:
            raise RuntimeError(f"{case}: not every row was submitted")
        if scenario == 8 and (max(row["nodes"] for row in reference) > 100 or
                              len({row["tree_hash"] for row in reference}) < FRAMES):
            raise RuntimeError(f"{case}: expected a moving window into the list")
        oracles[case] = reference
        print(f"Parity verified for {case}", flush=True)
    write_json(output / "parity.json", {"frames_per_lane": FRAMES + WARMUP, "raw": paths, "fields": ORACLE})
    return oracles


def order_pairs(samples, cases):
    rng = random.Random(SEED)
    first = {case: rng.randrange(2) for case in cases}
    for sample in range(samples):
        order = list(cases)
        rng.shuffle(order)
        for case in order:
            kinds = KINDS if (first[case] + sample) % 2 == 0 else KINDS[::-1]
            yield sample, case, kinds


def summarize(trials):
    comparisons = {}
    for case in sorted({trial["case"] for trial in trials}):
        lanes = {kind: sorted((trial for trial in trials if trial["case"] == case and trial["kind"] == kind),
                              key=lambda trial: trial["sample"]) for kind in KINDS}
        report = {}
        for phase in PHASES:
            # At least one nontrivial phase usually dominates total. Retained/no-input
            # timer measurements can quantize close to zero on fast machines.
            for score in ("p50", "p95", "raw_p95"):
                values = {kind: [row["metrics"][phase][score] for row in lanes[kind]] for kind in KINDS}
                report[f"{phase}_{score}"] = {kind: statistics.median(values[kind]) for kind in KINDS}
                if min(values["direct"]) > 0:
                    report[f"{phase}_{score}"]["paired"] = paired(values["direct"], values["tsx"])
        for metric in ("launch_to_present_ns", "startup_ns", "peak_rss_kib", "steady_rss_kib"):
            values = {kind: [row[metric] for row in lanes[kind]] for kind in KINDS}
            report[metric] = {kind: {"raw": values[kind], **stats(values[kind])} for kind in KINDS}
            report[metric]["paired"] = paired(values["direct"], values["tsx"])
        comparisons[case] = report
    return comparisons


def measure(output, raw, cpu, samples, oracles):
    trials, allocations = [], {}
    non_hash = ("frame", "nodes", "paint", "emitted", "behavior")
    for sample, (scenario, mode), kinds in order_pairs(samples, CASES):
        case = case_name(scenario, mode)
        reference = comparable(oracles[case][WARMUP:], non_hash)
        for kind in kinds:
            records, metrics = one_run(kind, scenario, mode, cpu, "timing", raw, f"{case}_{kind}_{sample:02d}")
            require_same(reference, comparable(records, non_hash), f"{case} timing {kind} #{sample + 1}")
            trials.append({"sample": sample, "case": case, "kind": kind, **metrics})
            write_json(output / "timing-progress.json", trials)
        print(f"Paired timing {sample + 1}/{samples} for {case}", flush=True)
    for scenario, mode in CASES:
        case = case_name(scenario, mode)
        allocations[case] = {}
        for kind in KINDS:
            records, metrics = one_run(kind, scenario, mode, cpu, "alloc", raw, f"{case}_{kind}_alloc")
            require_same(comparable(oracles[case][WARMUP:], non_hash), comparable(records, non_hash),
                         f"{case} allocation profile {kind}")
            allocations[case][kind] = metrics["allocation"]
    return {"raw_trials_in_order": trials, "comparisons": summarize(trials), "allocation_profiles": allocations}


def build_times(output, raw, cpu, samples):
    authored = {"direct": HERE / "bench_direct.zi", "tsx": HERE / "bench_scenes.tsx"}
    results = []
    for sample, _, kinds in order_pairs(samples, ["build"]):
        for kind in kinds:
            full = compile_lane(kind, cpu, True, raw / f"build_{kind}_{sample:02d}_full.txt")
            source = authored[kind]
            baseline = source.read_bytes()
            needle = b'"App icon"'
            if baseline.count(needle) != 1:
                raise RuntimeError(f"Controlled incremental edit is ambiguous in {source}")
            edited = baseline.replace(needle, b'"App menu"')
            try:
                source.write_bytes(edited)
                incremental = compile_lane(kind, cpu, False, raw / f"build_{kind}_{sample:02d}_incremental.txt")
            finally:
                if source.read_bytes() == edited:
                    source.write_bytes(baseline)
            if source.read_bytes() != baseline or full["sha256"] == incremental["sha256"]:
                raise RuntimeError(f"Incremental rebuild did not apply and restore the controlled edit in {source}")
            incremental["edited_source_sha256"] = hashlib.sha256(edited).hexdigest()
            results.append({"sample": sample, "kind": kind, "full": full, "incremental": incremental})
            write_json(output / "build-progress.json", results)
            print(f"Full and one-label rebuild {sample + 1}/{samples} for {kind}", flush=True)
    comparisons = {}
    for mode in ("full", "incremental"):
        values = {kind: [row[mode]["wall_ns"] for row in results if row["kind"] == kind] for kind in KINDS}
        comparisons[mode] = {kind: stats(values[kind]) for kind in KINDS}
        comparisons[mode]["paired"] = paired(values["direct"], values["tsx"])
    return {"samples_in_order": results, "comparisons": comparisons}


def start_xvfb(private, cpu, log):
    authority = Path(private) / "Xauthority"
    # Xauthority FamilyWild entry. Xvfb selects and reports its unused display.
    with open(authority, "wb", opener=lambda name, flags: os.open(name, flags, 0o600)) as stream:
        stream.write(b"\xff\xff")
        for field in (b"", b"", b"MIT-MAGIC-COOKIE-1", os.urandom(16)):
            stream.write(len(field).to_bytes(2, "big") + field)
    read_fd, write_fd = os.pipe()
    with os.fdopen(read_fd, "r") as pipe, log.open("w") as errors:
        try:
            xvfb = subprocess.Popen(["taskset", "-c", str(cpu), "Xvfb", "-displayfd", str(write_fd),
                                     "-auth", str(authority), "-nolisten", "tcp", "-screen", "0", "960x600x24"],
                                    env=ENV, stderr=errors, pass_fds=(write_fd,))
        finally:
            os.close(write_fd)
        try:
            ready, _, _ = select.select([pipe], [], [], 15)
            answer = pipe.readline().strip() if ready else ""
            if not answer.isdecimal():
                raise RuntimeError(f"Private Xvfb did not return a display. See {log}")
        except BaseException:
            xvfb.terminate()
            xvfb.wait(timeout=15)
            raise
    ENV.update(DISPLAY=f":{answer}", XAUTHORITY=str(authority))
    return xvfb


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="Unused directory beneath ~/Documents")
    parser.add_argument("--cpu", type=int, default=0)
    parser.add_argument("--xvfb-cpu", type=int, default=1)
    parser.add_argument("--samples", type=int, default=9)
    parser.add_argument("--verify-only", action="store_true", help="Build fixtures and verify parity without entering collection")
    args = parser.parse_args()
    if platform.node().split(".")[0] != "alpha":
        parser.error("Run only on the physical Alpha host")
    physical = subprocess.run(["systemd-detect-virt"], capture_output=True, text=True)
    if physical.returncode != 1 or physical.stdout.strip() != "none":
        parser.error("Run only on the physical Alpha host")
    if args.samples < 9:
        parser.error("Collect at least nine paired process trials")
    allowed = os.sched_getaffinity(0)
    if not {args.cpu, args.xvfb_cpu} <= allowed or siblings(args.cpu) == siblings(args.xvfb_cpu):
        parser.error("Choose allowed CPU numbers on different physical cores")
    output = args.output.expanduser().resolve()
    if not output.is_relative_to(Path.home() / "Documents") or output == Path.home() / "Documents":
        parser.error("Store measurements under a new directory within ~/Documents")
    WORK.mkdir(parents=True, exist_ok=True)
    work_lock = os.open(WORK, os.O_RDONLY | os.O_DIRECTORY)
    try:
        fcntl.flock(work_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        output.mkdir(parents=True, exist_ok=False)
        raw = output / "raw"
        raw.mkdir()
        run_campaign(args, output, raw)
    finally:
        os.close(work_lock)


def run_campaign(args, output, raw):
    resolved_ziran = read_command(ZIRAN / "build/bin/ziran", "pkg", "path", "ziran", "--offline")
    if Path(resolved_ziran.strip()).resolve() != ZIRAN:
        raise RuntimeError("Kryon must resolve Ziran to the canonical sibling checkout")
    identity = source_manifest()
    binaries = output / "binaries"
    sources = output / "sources"
    for folder in (sources, binaries):
        folder.mkdir()
    for source in (Path(__file__), *HERE.glob("*.zi"), *HERE.glob("*.tsx"), *STARTER.rglob("*.tsx"),
                   *STARTER.rglob("*.zi"), ROOT / "src/backend/pixmap_frame.zi"):
        target = sources / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    builds = {}
    for kind in KINDS:
        builds[kind] = compile_lane(kind, args.cpu, True, raw / f"prepare_{kind}.txt")
        binary = binaries / kind
        shutil.copy2(executable(kind), binary)
        dependencies = read_command("ldd", binary)
        builds[kind]["linked_libraries"] = {str(path): sha(path) for path in
                                             map(Path, re.findall(r"(?:=>\s+|^\s*)(/[^\s]+)", dependencies, flags=re.M))}
    if source_manifest() != identity:
        raise RuntimeError("Sources or tools changed during preparation")
    metadata = {"host": platform.node(), "virtualization": "none", "kernel": platform.release(),
                "cpu_model": next(line.partition(":")[2].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                                  if line.startswith("model name")),
                "date_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "identity": identity,
                "compiler": read_command("cc", "--version").splitlines()[0], "optimization": ENV["CFLAGS"],
                "path": "generic Ziran frontend and checked IR, C exporter and optimized Linux executables",
                "viewport": [960, 600], "raster_threads": 1, "presentation": "SDL2 software on private Xvfb",
                "cpu": args.cpu, "cpu_siblings": siblings(args.cpu), "xvfb_cpu": args.xvfb_cpu,
                "cpu_governor": systext(f"/sys/devices/system/cpu/cpu{args.cpu}/cpufreq/scaling_governor"),
                "warmup_frames": WARMUP, "measured_frames": FRAMES, "paired_trials": args.samples,
                "order_seed": SEED, "builds": builds, "verify_only": args.verify_only,
                "outlier_rule": "symmetric 1.5xIQR, inclusive interpolated quartiles",
                "uncertainty": "percentile bootstrap across filtered process pairs, 10000 draws, seed 72491"}
    write_json(output / "metadata.json", metadata)
    with tempfile.TemporaryDirectory(prefix="kryon-bench-xvfb-") as private:
        xvfb = start_xvfb(private, args.xvfb_cpu, raw / "xvfb.txt")
        try:
            oracles = verify(output, raw, args.cpu)
            if args.verify_only:
                return
            results = measure(output, raw, args.cpu, args.samples, oracles)
        finally:
            ENV.pop("DISPLAY", None)
            ENV.pop("XAUTHORITY", None)
            xvfb.terminate()
            xvfb.wait(timeout=15)
    for kind in KINDS:
        if sha(executable(kind)) != builds[kind]["sha256"]:
            raise RuntimeError(f"{kind} timing binary changed during collection")
    if source_manifest() != identity:
        raise RuntimeError("Sources or tools changed during collection")
    results["metadata_sha256"] = sha(output / "metadata.json")
    results["parity_sha256"] = sha(output / "parity.json")
    write_json(output / "timing-results.json", results)
    with (output / "summary.csv").open("w", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["case", "metric", "direct_ns", "tsx_ns", "paired_percent", "95_ci_low_percent", "95_ci_high_percent"])
        for case, comparison in results["comparisons"].items():
            for phase in PHASES:
                for score in ("p50", "p95", "raw_p95"):
                    row = comparison[f"{phase}_{score}"]
                    pair = row.get("paired", {"percent": "", "95_ci_percent": ["", ""]})
                    writer.writerow([case, f"{phase}_{score}", row["direct"], row["tsx"], pair["percent"], *pair["95_ci_percent"]])
    results["build_timing"] = build_times(output, raw, args.cpu, args.samples)
    if source_manifest() != identity:
        raise RuntimeError("Sources or tools changed during controlled build trials")
    write_json(output / "results.json", results)
    write_report(output, metadata, results)
    print(f"Wrote {output / 'REPORT.txt'} and raw evidence", flush=True)


def write_report(output, metadata, results):
    rows = ["Persistent TSX and internal Ziran on physical Alpha", "",
            f"{metadata['cpu_model']}. Kryon {metadata['identity']['revisions']['kryon']}.",
            f"Ziran {metadata['identity']['revisions']['ziran']}. {metadata['compiler']} {metadata['optimization']}.",
            f"{metadata['paired_trials']} balanced, interleaved process pairs per case, {WARMUP} warmup and {FRAMES} measured frames each.",
            "The parity pass matched pixels, exact geometry and clip bits, tree identities, semantics, paint and behavior.",
            "Both lanes used the same widgets, retained tree, software pixmap and SDL2 software presentation on private Xvfb.",
            "The window phase includes damage upload, SDL copy and present on that server. It measures CPU-side submission.",
            "Startup measures launch through first presentation, and also records time from native runner entry. RSS comes from /proc/self/status.",
            "Per-frame filtering uses symmetric 1.5xIQR with inclusive interpolated quartiles. Raw tails are also retained.",
            "Paired deltas are bootstrapped over processes. Intervals spanning zero are inconclusive.",
            "", "Total frame time, p50 / p95 in microseconds, and paired p50 delta with 95% interval", ""]
    for case, values in results["comparisons"].items():
        p50, p95 = values["total_ns_p50"], values["total_ns_p95"]
        pair = p50["paired"]
        lo, hi = pair["95_ci_percent"]
        rows.append(f"{case:30} {p50['direct']/1000:10.2f}/{p95['direct']/1000:<10.2f} "
                    f"{p50['tsx']/1000:10.2f}/{p95['tsx']/1000:<10.2f} {pair['percent']:+.2f}% [{lo:+.2f}, {hi:+.2f}]")
    trials = results["raw_trials_in_order"]
    frequency = [sample for trial in trials for sample in trial["noise"]["frequency_khz"]]
    sibling_ids = set(metadata["cpu_siblings"].split(",")) - {str(metadata["cpu"])}
    sibling_busy = [trial["noise"]["logical_cpu_busy_fraction"][cpu]
                    for trial in trials for cpu in sibling_ids if cpu in trial["noise"]["logical_cpu_busy_fraction"]]
    if frequency:
        rows.extend(["", f"Sampled CPU frequency p10–p90: {quantile(frequency, .10)/1e6:.3f}–{quantile(frequency, .90)/1e6:.3f} GHz."])
    if sibling_busy:
        rows.append(f"Highest sampled sibling busy fraction across a process run: {max(sibling_busy):.1%}.")
    rows.extend(["", "Native executable bytes"])
    rows.extend(f"{kind:10} {metadata['builds'][kind]['bytes']}" for kind in KINDS)
    rows.extend(["", "Build time, p50 seconds. Full trials start with empty frontend and output caches.",
                 "Each incremental trial changes one authored toolbar label after warming that lane. OS page caches remain warm."])
    for name, values in results["build_timing"]["comparisons"].items():
        pair = values["paired"]
        lo, hi = pair["95_ci_percent"]
        rows.append(f"{name:14} {values['direct']['p50']/1e9:8.3f} {values['tsx']['p50']/1e9:8.3f} "
                    f"{pair['percent']:+.2f}% [{lo:+.2f}, {hi:+.2f}]")
    rows.extend(["", "The allocation profiles use separate glibc mtrace runs with a post-warmup trace window.",
                 "results.json records requested bytes, allocation calls, build trials, per-phase distributions, per-process RSS and noise.",
                 "summary.csv lists per-phase p50, filtered p95 and raw p95 with paired uncertainty.",
                 "metadata.json, parity.json, binaries/, sources/ and raw/ preserve identities, per-frame samples and trace evidence."])
    (output / "REPORT.txt").write_text("\n".join(rows) + "\n")


if __name__ == "__main__":
    def stop_requested(signum, _frame):
        raise SystemExit(128 + signum)
    signal.signal(signal.SIGTERM, stop_requested)
    main()

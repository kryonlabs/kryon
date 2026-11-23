#!/usr/bin/env python3
"""Retained EGL/GLES Scene tests. Invoke with build-queue --."""
import os
import argparse
import json
import struct
import threading
import time
from pathlib import Path
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

OUT = ROOT / "build/scratch/scene-gpu"


def build(source=ROOT / "tests/scene_gpu_test.zi", entry="scene_gpu_test:main", module_paths=(),saved=False,
          extra_libraries=""):
    ir = OUT / "ir"
    generated = OUT / "c"
    OUT.mkdir(parents=True, exist_ok=True)
    # Disposable codegen intermediates share one stable test executable.
    for directory, suffix in ((ir, "*.zir"), (generated, "*.c"), (generated, "*.h")):
        for old in directory.glob(suffix):
            old.unlink()
    # Resolve target guards for native execution before saving IR.
    subprocess.run([str(ZIRAN), "ir", "--target=c", "--project", "--root", str(source.parent),
                    *[arg for path in module_paths for arg in ("--module-path", str(path))],
                    "--entry", entry, "-o", str(ir), str(source)], cwd=ROOT, check=True)
    if saved:
        initial=ir;ir=OUT/"saved-ir"
        for old in ir.glob("*.zir"):
            old.unlink()
        subprocess.run([str(ZIRAN),"ir","--root",str(initial),"--module-path",str(initial),
                        "--entry",entry,"-o",str(ir),str(initial/(source.stem+".zir"))],
                       cwd=ROOT,check=True)
    # zi2c --exe emits the entry's main and links the native fixture.
    compiler=os.environ.get("KRYON_TEST_CC",os.environ.get("CC","cc"))
    subprocess.run([str(ZIRAN.with_name("zi2c")), "--entry", entry, "--exe", "--root", str(ir),
                    "-o", str(generated), *map(str, sorted(ir.glob("*.zir")))],
                   cwd=ROOT, check=True,
                    env=os.environ | {"CC": compiler, "LDLIBS": f"-lEGL -lGLESv2 -lGLX -lX11 -lm {extra_libraries}"})
    executable = generated / source.stem
    return executable


def private_env():
    env = dict(os.environ)
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                 "SESSION_MANAGER", "DESKTOP_SESSION"):
        env.pop(name, None)
    env["HARMONY_DESKTOP_RECOVERY"] = "0"
    env["YUE_DESKTOP_RECOVERY"] = "0"
    env["EGL_PLATFORM"] = "surfaceless"
    return env


def room_resolution(executable, assets, env):
    import numpy as np

    prefix = OUT / "resolution"
    frames = {}
    with subprocess.Popen([str(executable), str(prefix), "resolution"], cwd=assets, env=env,
                          stdout=subprocess.PIPE, text=True) as process:
        for line in process.stdout:
            if line.startswith("resolution ") and len(line.split()) == 11:
                _, width, _, _, *timings = line.split()
                frames.setdefault(int(width), []).append(list(map(float, timings)))
            else:
                print(line, end="", flush=True)
        if process.wait():
            raise RuntimeError(f"Room resolution test failed: {process.returncode}")

    assert set(frames) == {2527, 2401, 2274}, frames.keys()
    for width, rows in frames.items():
        assert len(rows) == 12, (width, len(rows))
        values = np.array(rows[1:10])
        render, resize, legacy = values[:, :3].T
        print(f"{width}px first render={rows[0][0]:.1f} ms resize={rows[0][1]:.1f} ms "
              f"shader={rows[0][3]:.1f} ms upload={rows[0][4]:.1f} ms")
        print(f"{width}px cache-fill p50={np.median(render + resize):.1f} ms "
              f"render={np.median(render):.1f} ms resize={np.median(resize):.1f} ms "
              f"legacy cache-fill={np.median(render + legacy):.1f} ms resize={np.median(legacy):.1f} ms "
              f"GPU completion={np.median(values[:, 5]):.1f} ms transfer={np.median(values[:, 6]):.1f} ms")
    for state in (9, 10, 11):
        native = np.memmap(f"{prefix}-2527-{state}.rgba", dtype=np.uint8, mode="r", shape=(1423, 2527, 4))
        for width in (2401, 2274):
            scaled = np.memmap(f"{prefix}-{width}-{state}.rgba", dtype=np.uint8, mode="r", shape=native.shape)
            errors = []
            for x, y in ((0, 0), (463, 351), (927, 703)):
                crop = np.s_[y:y + 720, x:x + 1600]
                assert np.all(native[crop][..., 3] == 255) and np.all(scaled[crop][..., 3] == 255)
                delta = np.abs(native[crop][..., :3].astype(np.int16) - scaled[crop][..., :3])
                errors.append((delta.mean(), (delta > 16).mean()))
            mae = max(row[0] for row in errors)
            fraction = max(row[1] for row in errors)
            print(f"{width}px viewport state {state}: max RGB MAE {mae:.3f}, "
                  f"max RGB channels beyond 16: {fraction:.3%}")
            if width == 2401:
                assert mae < 4.0 and fraction < 0.04, (state, mae, fraction)


def room(jungle, padding_variant=False, resolution=False):
    import numpy as np
    from PIL import Image
    libraries = subprocess.check_output(["pkg-config", "--libs", "pangocairo", "freetype2", "fontconfig"],
                                        text=True).strip()
    executable = build(ROOT / "tests/scene_gpu_room_test.zi", "scene_gpu_room_test:main",
                       (jungle, jungle / "host"), extra_libraries=libraries)
    env = private_env()
    paths = subprocess.check_output([str(executable)], cwd=jungle, env=env, text=True).splitlines()
    assets = OUT / "room-assets"
    # Stage decoded pixels separately so room profiling excludes image codecs.
    decode_start = time.perf_counter()
    for path in dict.fromkeys(paths):
        destination = assets / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        with Image.open(jungle / path) as image, destination.open("wb") as stream:
            rgba = image.convert("RGBA")
            stream.write(struct.pack("<II", *rgba.size))
            stream.write(rgba.tobytes())
    print(f"room asset decode and fixture staging {(time.perf_counter() - decode_start) * 1000:.1f} ms", flush=True)
    if resolution:
        return room_resolution(executable, assets, env)
    prefix = OUT / "room"
    command = [str(executable), str(prefix)]
    if padding_variant:
        command.append("padding")
    process = subprocess.Popen(command, cwd=assets, env=env, stdout=subprocess.PIPE,
                               text=True)
    stop = threading.Event()
    memory = {"rss_kib": 0, "pss_kib": 0}

    def sample():
        while not stop.wait(0.02):
            try:
                with Path(f"/proc/{process.pid}/smaps_rollup").open() as stream:
                    for line in stream:
                        if line.startswith(("Rss:", "Pss:")):
                            key = line.split(":")[0].lower() + "_kib"
                            memory[key] = max(memory[key], int(line.split()[1]))
            except (FileNotFoundError, ProcessLookupError):
                break

    worker = threading.Thread(target=sample)
    worker.start()
    frames = {}
    stages = {}
    profiles = {}
    counters = {}
    resources = []
    with (OUT / "room.log").open("w") as log:
        for line in process.stdout:
            log.write(line)
            if line.startswith("room frame "):
                row = line.split()
                frames.setdefault((int(row[2]), int(row[3])), []).append(list(map(float, row[5:9])))
                stages.setdefault((int(row[2]), int(row[3])), []).append(list(map(float, row[10:13])))
                counters.setdefault(row[2], []).append(list(map(int, row[13:15])))
            elif line.startswith("room profile warm "):
                row = line.split()
                profiles.setdefault((int(row[3]), int(row[4])), []).append(list(map(float, row[6:11])))
            else:
                print(line, end="", flush=True)
                if line.startswith("room resources "):
                    resources.append(line.strip())
    code = process.wait()
    stop.set()
    worker.join()
    if code:
        raise RuntimeError(f"Room GPU test failed: {code}")
    result = {"memory": memory, "resources": resources, "timings_ms": {}, "quality": {},
              "quality_states": {}, "frame_counters": {}, "total_definition": "outer wall time around SceneGpuRender including context restore and cache sweep"}
    for (width, height), rows in frames.items():
        assert len(rows) >= 120
        timings = np.array(rows)
        timings = np.column_stack([timings, timings[:, :3].sum(axis=1), stages[(width, height)], profiles[(width, height)]])
        result["timings_ms"][str(width)] = {
            name: dict(zip(("p50", "p95", "p99"), np.percentile(timings[:, col], (50, 95, 99)).tolist()))
            for col, name in enumerate(("submit", "readback", "conversion", "total", "components", "prepare", "shadow", "draw",
                                       "shader_setup", "texture_upload", "gpu_completion", "pixel_transfer", "image_copy"))}
        result["frame_counters"][str(width)] = {"draw_calls_min": min(r[0] for r in counters[str(width)]),
            "draw_calls_max": max(r[0] for r in counters[str(width)]),
            "shadow_passes_total": sum(r[1] for r in counters[str(width)])}
        assert result["frame_counters"][str(width)]["shadow_passes_total"] == 0
        cpu = np.fromfile(f"{prefix}-{width}-cpu.rgba", dtype=np.uint8).reshape(height, width, 4)
        gpu = np.fromfile(f"{prefix}-{width}-gpu.rgba", dtype=np.uint8).reshape(height, width, 4)
        delta = np.abs(cpu.astype(np.int16) - gpu.astype(np.int16))
        quality = {"mae": float(delta.mean()), "p95": float(np.percentile(delta, 95)),
                   "p99": float(np.percentile(delta, 99)),
                   "fraction_over_16": float((delta > 16).mean())}
        result["quality"][str(width)] = quality
        Image.fromarray(cpu).save(OUT / f"room-{width}-cpu.png")
        Image.fromarray(gpu).save(OUT / f"room-{width}-gpu.png")
        # Float32 shadows and sample locations may differ; large image changes fail.
        assert quality["mae"] <= 1.0 and quality["fraction_over_16"] <= 0.005, quality
        result["quality_states"][str(width)] = []
        for state in (1, 2, 3):
            cpu_state = np.fromfile(f"{prefix}-{width}-state{state}-cpu.rgba", dtype=np.uint8).reshape(height, width, 4)
            gpu_state = np.fromfile(f"{prefix}-{width}-state{state}-gpu.rgba", dtype=np.uint8).reshape(height, width, 4)
            delta = np.abs(cpu_state.astype(np.int16) - gpu_state.astype(np.int16))
            quality = {"state": state, "mae": float(delta.mean()),
                       "p99": float(np.percentile(delta, 99)),
                       "fraction_over_16": float((delta > 16).mean())}
            assert quality["mae"] <= 1.0 and quality["fraction_over_16"] <= 0.005, quality
            result["quality_states"][str(width)].append(quality)
            Image.fromarray(cpu_state).save(OUT / f"room-{width}-state{state}-cpu.png")
            Image.fromarray(gpu_state).save(OUT / f"room-{width}-state{state}-gpu.png")
    (OUT / "room-results.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--room", type=Path, help="Canonical Jungle checkout for the actual room fixture")
    parser.add_argument("--glx", action="store_true", help="Foreign GLX host restoration under private Xvfb")
    parser.add_argument("--async-test", action="store_true", help="Two-slot async identity, mutation and lifecycle")
    parser.add_argument("--readback-profile", action="store_true", help="2527x1423 opaque resolve vs straight RGBA timing")
    parser.add_argument("--saved",action="store_true",help="Reparse saved IR before async native execution")
    parser.add_argument("--resolution-profile", action="store_true", help="Balcony panorama render scale and viewport quality")
    parser.add_argument("--padding-variant", action="store_true",
                        help="Vary only native SceneMaterial padding in the actual room")
    args = parser.parse_args()
    if args.room:
        room(args.room, args.padding_variant, args.resolution_profile)
    elif args.readback_profile:
        executable = build(ROOT / "tests/scene_gpu_async_test.zi", "scene_gpu_async_test:main")
        subprocess.run([str(executable), "profile"], cwd=ROOT, env=private_env(), check=True)
    elif args.async_test:
        process = subprocess.Popen([str(build(ROOT / "tests/scene_gpu_async_test.zi", "scene_gpu_async_test:main",saved=args.saved))],
                                   cwd=ROOT, env=private_env(), stdout=subprocess.PIPE, text=True)
        samples = []
        for line in process.stdout:
            print(line,end="",flush=True)
            if line.startswith("async release cycle "):
                sample = {}
                with Path(f"/proc/{process.pid}/smaps_rollup").open() as stream:
                    for row in stream:
                        if row.startswith(("Rss:","Pss:")):
                            sample[row.split(":")[0]] = int(row.split()[1])
                samples.append(sample)
        assert process.wait() == 0, "Async Scene test failed"
        assert len(samples)==24
        growth = {key:max(s[key] for s in samples[12:])-min(s[key] for s in samples[12:])
                  for key in ("Rss","Pss")}
        assert max(growth.values()) <= 2048,(samples,growth)
        print("Async repeated release KiB growth",growth)
        (OUT/"async-lifecycle-memory.json").write_text(json.dumps(
            {"samples_kib":samples,"late_growth_kib":growth},indent=2)+"\n")
    elif args.glx:
        executable = build(ROOT / "tests/scene_gpu_glx_test.zi", "scene_gpu_glx_test:main")
        # xvfb-run owns this private display and its cleanup. Inherited desktop
        # display/auth/session variables are removed before it starts.
        subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 640x480x24 -nolisten tcp -extension MIT-SHM",
                        str(executable)], cwd=ROOT, env=private_env(), check=True)
    else:
        process = subprocess.Popen([str(build())], cwd=ROOT, env=private_env(),
                                   stdout=subprocess.PIPE, text=True)
        samples = []
        for line in process.stdout:
            print(line, end="", flush=True)
            if line.startswith("release cycle "):
                sample = {}
                with Path(f"/proc/{process.pid}/smaps_rollup").open() as stream:
                    for row in stream:
                        if row.startswith(("Rss:", "Pss:")):
                            sample[row.split(":")[0]] = int(row.split()[1])
                samples.append(sample)
        assert process.wait() == 0, "GPU lifecycle test failed"
        assert len(samples) == 24
        # Allow bounded Mesa/allocator warm-up, not retained 4MiB snapshots
        # per renderer lifetime. The final 12 cycles should stay flat.
        growth = {key: max(s[key] for s in samples[12:]) - min(s[key] for s in samples[12:])
                  for key in ("Rss", "Pss")}
        print("Repeated release KiB growth", growth)
        assert growth["Rss"] <= 2048 and growth["Pss"] <= 2048, samples
        (OUT / "lifecycle-memory.json").write_text(json.dumps(
            {"samples_kib": samples, "late_growth_kib": growth}, indent=2) + "\n")

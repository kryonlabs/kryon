"""Every Ziran target renders the same Kryon frame, byte for byte.

Each program -- the project templates, created with `kryon new` as a user
would, and the examples in examples/ -- runs on the headless pixmap host,
which rasterizes with integer arithmetic and prints a PPM image. It is
built from one saved IR for C, C++, Go, Rust, Python, and the portable
runner (zib), and every target's image must equal C's. On a mismatch the
image tools in tools/image_tools.zi, built for Python, report where pixels
differ and write a red diff.

The contact sheet build/pixmap-parity/grid.png shows every program once,
labeled with the targets that matched, for a visual check:

    python3 tests/pixmap_parity_test.py [--targets c,go] [--only app,hello]

ZIRAN_BIN selects the ziran command (default: the checkout beside Kryon).
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from templates_test import KRYON, ROOT, ZIRAN_ROOT, environment, local  # noqa: E402

ZIRAN = os.environ.get("ZIRAN_BIN") or str(ZIRAN_ROOT / "build/bin/ziran")
TARGETS = ["c", "cpp", "go", "rust", "py", "zib"]
TEMPLATES = ["app", "tui", "pages", "web"]
EXAMPLES = ["hello", "text_hello", "image_demo"]
OUTPUT = ROOT / "build/pixmap-parity"


def run(command, directory, env, stdout=None):
    result = subprocess.run([str(part) for part in command], cwd=directory, env=env,
                            stdin=subprocess.DEVNULL,
                            stdout=stdout if stdout is not None else subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=1800)
    if result.returncode != 0:
        output = result.stdout.decode(errors="replace") if isinstance(result.stdout, bytes) else ""
        raise RuntimeError(f"{' '.join(map(str, command))} returned {result.returncode}:\n"
                           f"{output[-2000:]}{result.stderr.decode(errors='replace')[-4000:]}")
    return result.stdout


class Program:
    """One saved-IR program: its entry and the modules that host it."""

    def __init__(self, name, ir, entry, hosts):
        self.name = name
        self.ir = ir
        self.entry = entry
        self.hosts = hosts

    @property
    def module(self):
        return self.entry.split(":")[0]

    def source(self):
        return self.ir / f"{self.module}.zir"


def template_program(work, name, env):
    """A template, created with kryon new, and its pixmap host's IR."""
    project = work / f"demo-{name}"
    run([KRYON, "new", project.name, "--template", name], work, env)
    local(project)
    lock = (project / "ziran.lock").read_text()
    identity = next(package["id"] for package in json.loads(lock)["packages"]
                    if package["name"] == "kryon")
    ir = project / "build/parity-ir"
    host = ROOT / "src/backend/pixmap_run.zi"
    # The IR command Kryon's build rules use for a native profile.
    run([ZIRAN, "ir", "--project", "--entry", f"{identity}_pixmap_run:main",
         "-o", ir, host], project, env)
    return Program(name, ir, f"{identity}_pixmap_run:main", [f"{identity}_pixmap"])


def example_program(work, name, env):
    """An example from examples/, whose Frame opens its own session, under a
    runner that paints it with the pixmap host."""
    directory = work / f"example-{name}"
    directory.mkdir()
    (directory / f"{name}_shot.zi").write_text(f'''#import "pixmap"
#import "pointer_input"
#import "tree_input"
#import "{name}"

// Examples read the pointer; this frame has none.
#program_export
PollPointer :: () -> PointerFrame {{
    frame: PointerFrame
    return frame
}}

#program_export
main :: () -> s32 {{
    frame: s32 = 0
    while frame < 2 {{
        PixmapBegin(cast(u32)PixmapBackground)
        unused Frame()
        frame += 1
    }}
    PixmapPresent()
    return 0
}}
''')
    ir = directory / "ir"
    run([ZIRAN, "ir", "--root", directory,
         "--module-path", ROOT / "examples", "--module-path", ROOT / "src/ui",
         "--module-path", ROOT / "src/backend", "--module-path", ZIRAN_ROOT / "std",
         "--entry", f"{name}_shot:main", "-o", ir, directory / f"{name}_shot.zi"],
        directory, env)
    return Program(name, ir, f"{name}_shot:main", ["pixmap", f"{name}_shot"])


def render(program, target, work, env):
    """Builds program for target and returns the image it prints."""
    out = work / f"{program.name}-{target}"
    if out.exists():
        shutil.rmtree(out)
    binds = []
    for host in program.hosts:
        binds += ["--bind-host", host]
    common = ["--root", program.ir, "--entry", program.entry]
    if target == "c":
        run([ZIRAN, "build", "--target=c", "--exe", *common, "-o", out, program.source()],
            work, env)
        image = run([out / program.module], work, env)
    elif target == "cpp":
        run([ZIRAN, "build", "--target=cpp", *common, "-o", out, program.source()], work, env)
        sources = sorted(out.glob("*.cpp"))
        run([os.environ.get("CXX", "c++"), "-std=c++17", "-O1", f"-I{ZIRAN_ROOT / 'include'}",
             f"-I{out}", *sources, "-o", out / "program"], work, env)
        image = run([out / "program"], work, env)
    elif target == "go":
        run([ZIRAN, "build", "--target=go", "--pkg", "main", "--exe", *binds, *common,
             "-o", out, program.source()], work, env)
        run(["go", "build", "-o", "program", "."], out, {**env, "GO111MODULE": "off"})
        image = run([out / "program"], work, env)
    elif target == "rust":
        run([ZIRAN, "build", "--target=rust", "--exe", *binds, *common, "-o", out,
             program.source()], work, env)
        target_dir = work / f"rust-target-{program.name}"
        run(["cargo", "build", "--quiet", "--release", "--manifest-path", out / "Cargo.toml"],
            work, {**env, "CARGO_TARGET_DIR": str(target_dir)})
        image = run([target_dir / "release/ziran_generated"], work, env)
    elif target == "py":
        run([ZIRAN, "build", "--target=py", "--exe", *binds, *common, "-o", out,
             program.source()], work, env)
        image = run(["python3", out], work, env)
    elif target == "zib":
        bundle = work / f"{program.name}.zib"
        run([ZIRAN, "bundle", *binds, *common, "-o", bundle, program.source()], work, env)
        image = run([ZIRAN, "run", bundle], work, env)
        # The runner prints the entry's result after the program's output.
        image = image[:image.rstrip(b"\n").rfind(b"\n") + 1]
    else:
        raise ValueError(target)
    path = OUTPUT / f"{program.name}-{target}.ppm"
    path.write_bytes(image)
    return path


def image_tools(work, env):
    """tools/image_tools.zi built for Python."""
    out = work / "image_tools"
    run([ZIRAN, "build", "--target=py", "--exe", "--root", ROOT / "tools",
         "--module-path", ROOT / "src/backend", "--module-path", ZIRAN_ROOT / "std",
         "--entry", "image_tools:main", "-o", out, ROOT / "tools/image_tools.zi"], work, env)
    return out


def tool_session(tools, commands, env):
    result = subprocess.run(["python3", tools], input="\n".join(commands) + "\n",
                            text=True, capture_output=True, env=env, timeout=3600)
    if result.returncode != 0:
        raise RuntimeError(result.stderr)
    return result.stdout.splitlines()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--targets", default=",".join(TARGETS))
    parser.add_argument("--only", default="")
    arguments = parser.parse_args()
    targets = arguments.targets.split(",")
    selected = set(filter(None, arguments.only.split(",")))
    assert "c" in targets, "C is the reference target"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="kryon-pixmap-parity-") as directory:
        work = Path(directory)
        env = environment(work)
        env["PATH"] = f"{Path(ZIRAN).parent}:{env['PATH']}"
        programs = []
        for name in TEMPLATES:
            if not selected or name in selected:
                programs.append(template_program(work, name, env))
        for name in EXAMPLES:
            if not selected or name in selected:
                programs.append(example_program(work, name, env))
        results = {}
        failures = []
        with ThreadPoolExecutor(max_workers=4) as pool:
            jobs = {(program.name, target): pool.submit(render, program, target, work, env)
                    for program in programs for target in targets}
            for key, job in jobs.items():
                try:
                    results[key] = job.result()
                except RuntimeError as error:
                    failures.append(f"{key[0]} on {key[1]}: build or run failed\n{error}")
        tools = image_tools(work, env)
        cells = []
        compare = []
        for program in programs:
            reference = results.get((program.name, "c"))
            if reference is None:
                continue
            matched = []
            for target in targets:
                path = results.get((program.name, target))
                if path is None:
                    continue
                if path.read_bytes() == reference.read_bytes():
                    matched.append(target)
                else:
                    diff = OUTPUT / f"{program.name}-{target}-diff.ppm"
                    compare.append((program.name, target, f"compare {reference} {path} {diff}"))
            cells.append(f"{program.name}:{'+'.join(matched)}={reference}")
        if compare:
            reports = tool_session(tools, [command for _, _, command in compare], env)
            for (name, target, _), report in zip(compare, reports):
                failures.append(f"{name} on {target} differs from C: {report}")
                cells.append(f"{name}:{target}-diff={OUTPUT / f'{name}-{target}-diff.ppm'}")
        columns = 3
        scale = 2 if len(cells) <= 6 else 3
        grid = OUTPUT / "grid.png"
        print("\n".join(tool_session(tools, [
            f"title Kryon pixmap parity: {', '.join(targets)}",
            f"grid {grid} {columns if len(cells) > 3 else len(cells)} {scale} {' '.join(cells)}"],
            env)))
        for program in programs:
            ok = [target for target in targets
                  if (program.name, target) in results and
                  results[(program.name, target)].read_bytes() ==
                  results[(program.name, "c")].read_bytes()]
            print(f"{program.name}: {' '.join(ok)} identical")
        if failures:
            print("\n\n".join(failures), file=sys.stderr)
            sys.exit(1)
    print(f"Kryon pixmap parity: {len(programs)} programs identical on {', '.join(targets)}")


if __name__ == "__main__":
    main()

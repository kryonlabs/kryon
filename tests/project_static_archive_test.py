"""Link a project-owned static archive without a system-library lookup."""

import os
from pathlib import Path
import pty
import select
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
# The sibling checkout CI uses, else the org-grouped local layout.
ZIRAN_ROOT = Path(os.environ.get("ZIRAN_ROOT") or next(
    (path for path in (ROOT.parent / "ziran", ROOT.parent.parent / "ziranlang/ziran")
     if path.is_dir()), ROOT.parent / "ziran"))
ZIRAN = ZIRAN_ROOT / "build/bin/ziran"


def run(command, directory):
    environment = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
        environment.pop(name, None)
    subprocess.run(command, cwd=directory, env=environment, check=True)


def request_exit(binary, directory, expected):
    master, slave = pty.openpty()
    try:
        process = subprocess.Popen(
            [str(binary)], cwd=directory, stdin=slave, stdout=slave,
            stderr=subprocess.PIPE,
        )
    finally:
        os.close(slave)
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            readable, _, _ = select.select([master], [], [], 0.05)
            if readable:
                os.read(master, 65536)
                os.write(master, b"q")
                break
            if process.poll() is not None:
                break
        process.wait(timeout=10)
        stderr = process.stderr.read().decode(errors="replace")
        assert process.returncode == expected, (process.returncode, stderr)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        process.stderr.close()
        os.close(master)


def expect_failure(command, directory):
    environment = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
        environment.pop(name, None)
    result = subprocess.run(command, cwd=directory, env=environment)
    assert result.returncode != 0


def main():
    with tempfile.TemporaryDirectory(prefix="kryon-static-archive-") as temporary:
        temporary_path = Path(temporary)
        project = temporary_path / "project"
        project.mkdir()
        (project / "src").mkdir()
        (project / "build").mkdir()
        (project / "library.c").write_text(
            "int StaticArchiveValue(void) { return 77; }\n"
        )
        run(["cc", "-c", "library.c", "-o", "build/library.o"], project)
        run(["ar", "rcs", "build/libdemo.a", "build/library.o"], project)
        (project / "ziran.toml").write_text(
            '[package]\nname = "static_archive_probe"\n'
            'entry = "src/app.zi"\nmodule_roots = ["src"]\n'
            'bridge_modules = ["app"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.kryon]\n'
            'git = "https://github.com/kryonlabs/kryon.git"\nref = "master"\n\n'
            '[tool.kryon]\ndefault_profile = "tui"\n\n'
            '[tool.kryon.profiles.tui]\nbackend = "terminal"\n'
            'codegen = "c99"\nstatic_archive = "build/escape.a"\n'
        )
        overrides = {
            "kryon": str(ROOT),
            "ziran": str(ZIRAN_ROOT),
        }
        if os.environ.get("RAYLIB_SOURCE"):
            overrides["raylib"] = os.environ["RAYLIB_SOURCE"]
        (project / "ziran.local.toml").write_text(
            "[overrides]\n"
            + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
        )
        (project / "src/app.zi").write_text(
            'using UI :: #import "Widgets";\n'
            'static_archive :: #system_library "static_archive";\n'
            'StaticArchiveValue :: () -> s32 #foreign static_archive;\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
            '    typed: s32 = TypedCodepointTake(session)\n'
            '    if typed == 113 { return StaticArchiveValue() }\n'
            '    return 0\n}\n'
        )
        outside_archive = temporary_path / "outside.a"
        outside_archive.write_bytes((project / "build/libdemo.a").read_bytes())
        (project / "build/escape.a").symlink_to(outside_archive)
        run([str(ZIRAN), "lock"], project)
        expect_failure(
            [str(ZIRAN), "tool", "kryon", "build", "--profile", "tui"], project
        )
        (project / "ziran.toml").write_text(
            (project / "ziran.toml").read_text().replace(
                "build/escape.a", "build/libdemo.a"
            )
        )
        run([str(ZIRAN), "tool", "kryon", "build", "--profile", "tui"], project)
        request_exit(project / "build/static_archive_probe-tui", project, 77)
    print("Kryon project static archive link passed")


if __name__ == "__main__":
    main()

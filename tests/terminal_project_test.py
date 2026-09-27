"""Exercise interactive UTF-8 input through an imported Kryon terminal host."""

import os
from pathlib import Path
import pty
import select
import subprocess
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
ZIRAN = str(ROOT.parent / "ziran/build/bin/ziran")


def run(command, directory):
    environment = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
        environment.pop(name, None)
    subprocess.run(command, cwd=directory, env=environment, check=True)


def exercise(binary, directory, keys, expected):
    master, slave = pty.openpty()
    try:
        process = subprocess.Popen(
            [str(binary)], cwd=directory, stdin=slave, stdout=slave,
            stderr=subprocess.PIPE,
        )
    finally:
        os.close(slave)
    try:
        # The host enters raw mode and paints its first frame before polling.
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            readable, _, _ = select.select([master], [], [], 0.05)
            if readable:
                output = os.read(master, 65536)
                if b"\x1b[?1049h" in output:
                    break
            if process.poll() is not None:
                raise AssertionError("terminal host exited before receiving input")
        else:
            raise AssertionError("terminal host did not enter interactive mode")
        os.write(master, keys)
        while process.poll() is None and time.monotonic() < deadline:
            readable, _, _ = select.select([master], [], [], 0.05)
            if readable:
                try:
                    os.read(master, 65536)
                except OSError:
                    pass
        assert process.poll() is not None, "terminal host did not process input"
        stderr = process.stderr.read().decode(errors="replace")
        assert process.returncode == expected, (keys, process.returncode, stderr)
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        process.stderr.close()
        os.close(master)


def main():
    with tempfile.TemporaryDirectory(prefix="kryon-terminal-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "ziran.toml").write_text(
            '[package]\nname = "terminal_probe"\nentry = "src/app.zi"\n'
            'module_roots = ["src"]\nbridge_modules = ["app"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.Kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.kryon]\ndefault_profile = "tui"\n\n'
            '[tool.kryon.profiles.tui]\nbackend = "terminal"\n'
        )
        (project / "ziran.local.toml").write_text(
            f'[overrides]\nKryon = "{ROOT}"\n'
            f'ziran = "{ROOT.parent / "ziran"}"\n'
        )
        (project / "src/app.zi").write_text(
            'using UI :: #import "Widgets";\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
            '    typed: s32 = TypedCodepointTake(session)\n'
            '    if typed == 122 { return 42 }\n'
            '    if typed == 233 { return 43 }\n'
            '    if typed == 65 { return 44 }\n'
            '    if typed == 65533 { return 45 }\n'
            '    return 0\n}\n'
        )
        run([ZIRAN, "lock"], project)
        run([ZIRAN, "tool", "Kryon", "build", "--profile", "tui"], project)
        binary = project / "build/terminal_probe-tui"
        exercise(binary, project, b"z", 42)
        exercise(binary, project, "é".encode(), 43)
        exercise(binary, project, b"\x1b[Az", 42)
        exercise(binary, project, b"\xff", 45)
        exercise(binary, project, b"\xc3z", 45)
    print("terminal Ziran project: ASCII, UTF-8, escape, and invalid input passed")


if __name__ == "__main__":
    main()

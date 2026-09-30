"""Exercise interactive UTF-8 input through an imported Kryon terminal host."""

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
ZIRAN = str(ZIRAN_ROOT / "build/bin/ziran")


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
            '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.kryon]\ndefault_profile = "tui"\n\n'
            '[tool.kryon.profiles.tui]\nbackend = "terminal"\n'
        )
        overrides = {
            "kryon": str(ROOT),
            "ziran": str(ZIRAN_ROOT),
        }
        if os.environ.get("RAYLIB_SOURCE"):
            overrides["raylib"] = os.environ["RAYLIB_SOURCE"]
        (project / "ziran.local.toml").write_text(
            "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
        )
        (project / "src/app.zi").write_text(
            'using UI :: #import "Widgets";\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 {\n'
            '    key: s32 = KeyboardTake(session)\n'
            '    control: bool = (KeyboardModifiers(session) & KeyModifierControl) != 0\n'
            '    if key == 263 && control { return 51 }\n'
            '    if key == 263 { return 46 }\n'
            '    if key == 259 { return 47 }\n'
            '    if key == 257 { return 48 }\n'
            '    if key == 256 { return 49 }\n'
            '    if key == 65 && control { return 50 }\n'
            '    if key == 261 { return 52 }\n'
            '    typed: s32 = TypedCodepointTake(session)\n'
            '    if typed == 122 { return 42 }\n'
            '    if typed == 233 { return 43 }\n'
            '    if typed == 65 { return 44 }\n'
            '    if typed == 65533 { return 45 }\n'
            '    return 0\n}\n'
        )
        run([ZIRAN, "lock"], project)
        run([ZIRAN, "tool", "kryon", "build", "--profile", "tui"], project)
        binary = project / "build/terminal_probe-tui"
        exercise(binary, project, b"z", 42)
        exercise(binary, project, "é".encode(), 43)
        exercise(binary, project, b"\x1b[Az", 42)
        exercise(binary, project, b"\xff", 45)
        exercise(binary, project, b"\xc3z", 45)
        # Keys use the codes every Kryon host reports.
        exercise(binary, project, b"\x1b[D", 46)
        exercise(binary, project, b"\x7f", 47)
        exercise(binary, project, b"\r", 48)
        exercise(binary, project, b"\x1b", 49)
        exercise(binary, project, b"\x01", 50)
        exercise(binary, project, b"\x1b[1;5D", 51)
        exercise(binary, project, b"\x1b[3~", 52)
        exercise(binary, project, b"\x1bOD", 46)
    print("terminal Ziran project: ASCII, UTF-8, escape, invalid input, and key codes passed")


if __name__ == "__main__":
    main()

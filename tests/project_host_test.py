"""Build an application's own host and reject hosts outside its project."""

import os
from pathlib import Path
import subprocess
import tempfile

from toolchain import ZIRAN, ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]


def run(arguments, project, success=True):
    environment = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY"):
        environment.pop(name, None)
    result = subprocess.run(arguments, cwd=project, env=environment,
                            capture_output=True, text=True)
    if success:
        assert result.returncode == 0, (result.stdout, result.stderr)
    else:
        assert result.returncode != 0, (result.stdout, result.stderr)
    return result


def main():
    with tempfile.TemporaryDirectory(prefix="kryon-project-host-") as temporary:
        project = Path(temporary) / "project"
        (project / "src").mkdir(parents=True)
        (project / "ziran.toml").write_text(
            '[package]\nname = "host_probe"\ntool = "kryon"\n'
            'entry = "src/app.zi"\nmodule_roots = ["src"]\n'
            'bridge_modules = ["app"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n\n'
            '[tool.kryon]\ndefault_profile = "tui"\n\n'
            '[tool.kryon.profiles.tui]\nbackend = "terminal"\n'
            'host = "src/runner.zi"\n'
        )
        (project / "ziran.local.toml").write_text(
            f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n'
        )
        (project / "src/app.zi").write_text(
            'using UI :: #import "kryon/Widgets";\n'
            '#program_export\n'
            'Frame :: (session: Session, viewport: Rectangle) -> s32 { return 99 }\n'
        )
        (project / "src/runner.zi").write_text(
            '#import "std/byte_text_linux"\n'
            '#program_export\nmain :: (argc: s32, argv: **u8) -> s32 {\n'
            '    if argc != 2 { return 1 }\n'
            '    if TextFromCString(argv[1]) != "--probe" { return 2 }\n'
            '    print("custom host\\n")\n    return 0\n}\n'
        )
        run([str(ZIRAN), "lock"], project)
        run([str(ZIRAN), "build"], project)
        run([str(ZIRAN), "check"], project)
        result = run([str(project / "build/host_probe-tui"), "--probe"], project)
        assert result.stdout == "custom host\n", result.stdout

        outside = Path(temporary) / "outside.zi"
        outside.write_text((project / "src/runner.zi").read_text())
        (project / "src/escape.zi").symlink_to(outside)
        manifest = project / "ziran.toml"
        manifest.write_text(manifest.read_text().replace("src/runner.zi", "src/escape.zi"))
        result = run([str(ZIRAN), "build"], project, success=False)
        assert "host must stay inside the project" in result.stderr, result.stderr
    print("Project-owned host build, execution, and path containment passed")


if __name__ == "__main__":
    main()

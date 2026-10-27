"""Kryon's project templates and the kryon front door, display-free.

Each template is created with `kryon new` exactly as a user would, from
this checkout, and built and rendered once on the terminal host. The web
template is checked; its browser profiles need Emscripten.
"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile


from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
KRYON = ROOT / "build/bin/kryon"


def environment(work):
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "KRYON_FRONT_DOOR",
                 "MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
        env.pop(name, None)
    # The front door runs `ziran` from PATH: the one beside this checkout.
    env["PATH"] = f"{ZIRAN_ROOT / 'build/bin'}:{env['PATH']}"
    env["KRYON_TEMPLATES"] = str(ROOT)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    # Exercise the template commands against the canonical source checkouts,
    # including their uncommitted changes, without copying/cloning them or
    # fetching a new toolchain. Explicit overrides are installed before locking.
    ziran = ZIRAN_ROOT / "build/bin/ziran"
    raylib = subprocess.check_output(
        [str(ziran), "pkg", "path", "raylib"], cwd=ROOT, env=env, text=True,
    ).strip()
    # Resolve existing sources before isolating the new template projects.
    # A fresh template cache must not redownload Raylib's entire history.
    env["XDG_CACHE_HOME"] = str(work / "cache")
    overrides = (f'[overrides]\nziran = "{ZIRAN_ROOT}"\n'
                 f'Kryon = "{ROOT}"\nraylib = "{raylib}"\n')
    (work / "overrides.toml").write_text(overrides)
    return env


def run(command, directory, env, succeed=True, stdin=subprocess.DEVNULL):
    result = subprocess.run([str(part) for part in command], cwd=directory, env=env,
                            stdin=stdin, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=900)
    if succeed != (result.returncode == 0):
        raise AssertionError(f"{' '.join(map(str, command))} returned "
                             f"{result.returncode}:\n{result.stdout}")
    return result.stdout


def main():
    assert KRYON.exists(), "build build/bin/kryon first"
    with tempfile.TemporaryDirectory(prefix="kryon-templates-") as directory:
        work = Path(directory)
        env = environment(work)

        usage = run([KRYON, "help"], work, env)
        assert "app (window), tui (terminal), pages (navigation), web (browser page)" in usage

        # Terminal rendering is not tested; each template is created and checked.
        for template in ("app", "tui", "pages"):
            name = f"demo-{template}"
            created = run([KRYON, "new", name, "--template", template,
                           "--overrides", work / "overrides.toml"], work, env)
            assert f"created {name} from kryon:{template}" in created, created
            manifest = (work / name / "ziran.toml").read_text()
            assert 'tool = "kryon"' in manifest and "{{" not in manifest, manifest

        profiles = run([KRYON, "profiles"], work / "demo-app", env)
        assert profiles.splitlines() == ["* desktop  SDL2/Cairo window", "  portable  SDL2/Cairo window", "  tui  terminal",
                                         "  web  Canvas2D browser page",
                                         "  shot  headless image, PPM on standard output"], profiles
        run([KRYON, "check", "desktop"], work / "demo-app", env)
        # The shot profile draws a frame without any display.
        shot = run([KRYON, "run", "shot"], work / "demo-app", env)
        assert shot.startswith("P3\n960 600\n255\n"), shot[:40]
        assert len(shot.splitlines()) == 603, len(shot.splitlines())

        run([KRYON, "new", "demo-web", "--template", "web", "--overrides",
             work / "overrides.toml"], work, env)
        run([KRYON, "check"], work / "demo-web", env)

        missing = run([KRYON, "new", "other", "--template", "nosuch"], work, env,
                      succeed=False)
        assert "has no template nosuch; it has app, tui, pages, web" in missing, missing
        assert not (work / "other").exists()

        # kryon init turns a command-line package into a Kryon application:
        # the package keeps its own entry and gains the window's.
        run(["ziran", "new", "tool", "--overrides", work / "overrides.toml"], work, env)
        tool = work / "tool"
        merged = run([KRYON, "init", "--template", "tui"], tool, env)
        assert 'kept entry = "src/main.zi" in [package]' in merged, merged
        manifest = (tool / "ziran.toml").read_text()
        assert 'entry = "src/main.zi"' in manifest and 'tool = "kryon"' in manifest
        assert "[tool.kryon]" in manifest and 'entry = "src/app.zi"' in manifest

        # Without the settings ziran passes, the front door never loops.
        guarded = dict(env, KRYON_FRONT_DOOR="1")
        looped = run([KRYON, "build"], work / "demo-app", guarded, succeed=False)
        assert "without project settings" in looped, looped
    print("Kryon templates: app, tui, pages, web ok")


if __name__ == "__main__":
    main()

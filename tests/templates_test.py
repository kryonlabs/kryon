"""Kryon's project templates and the kryon front door, display-free.

Every template is created with `kryon new` from this checkout. The app's
desktop profile is checked and its shot profile renders without a display.
The web template is checked without building its browser profiles.
"""

import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import tomllib


from image_test_tools import png_pixels
from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
KRYON = ROOT / "build/bin/kryon"
# The app starter's decoded pixels at phone size, light and dark. When the
# starter's look changes on purpose, check build/test/templates/*.png and
# put the digests the failure prints here.
STARTER_DIGESTS = {
    "light": "d5e65fc5a5c39e0a09221d04cdfa17f2dead82530dbdba26e5b22bd088278d21",
    "dark": "5c217da2bafc43d647c1b48c3548214257d564ac814681b09183fd1a550862a6",
}


def environment(work):
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "KRYON_FRONT_DOOR",
                 "MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
        env.pop(name, None)
    # The front door runs `ziran` from PATH: the one beside this checkout.
    env["PATH"] = f"{ZIRAN_ROOT / 'build/bin'}:{env['PATH']}"
    env["KRYON_TEMPLATES"] = str(ROOT)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    # The projects and their build outputs are temporary. Reuse the prepared
    # package cache so their first lock can reuse the same dependency sources.
    # Install local checkout overrides before any template is locked.
    overrides = f'[overrides]\nKryon = "{ROOT}"\n'
    (work / "overrides.toml").write_text(overrides)
    return env


def run(command, directory, env, succeed=True, stdin=subprocess.DEVNULL, separate=False):
    # SEPARATE keeps build diagnostics (stderr) out of a program's own output.
    command = [str(part) for part in command]
    try:
        result = subprocess.run(command, cwd=directory, env=env, stdin=stdin,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE if separate else subprocess.STDOUT,
                                text=True, timeout=900)
    except subprocess.TimeoutExpired as error:
        output = (error.stderr or error.stdout or b"").decode(errors="replace")
        raise AssertionError(f"{' '.join(command)} timed out in {directory}:\n"
                             f"{output[-4000:]}") from error
    if succeed != (result.returncode == 0):
        raise AssertionError(f"{' '.join(command)} returned "
                             f"{result.returncode}:\n{result.stdout}{result.stderr or ''}")
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
            # new locks the project, so its first build needs no ziran lock.
            assert (work / name / "ziran.lock").is_file(), name

        run([KRYON, "check", "desktop"], work / "demo-app", env)
        profiles = run([KRYON, "profiles"], work / "demo-app", env)
        assert profiles.splitlines() == ["* desktop  SDL2/Cairo window", "  portable  SDL2/Cairo window",
                                         "  web  Canvas2D browser page",
                                         "  shot  headless image, PPM on standard output"], profiles
        # The shot profile draws a frame without any display.
        shot = run([KRYON, "run", "shot"], work / "demo-app", env, separate=True)
        assert shot.startswith("P3\n960 600\n255\n"), shot[:40]
        assert len(shot.splitlines()) == 603, len(shot.splitlines())

        # The starter's screen, offscreen at phone size in both themes.
        shots = ROOT / "build/test/templates"
        shots.mkdir(parents=True, exist_ok=True)
        for theme, expected in STARTER_DIGESTS.items():
            png = shots / f"starter-{theme}.png"
            run([KRYON, "run", "--png", png, "--width", "390", "--height", "844",
                 "--theme", theme], work / "demo-app", env, separate=True)
            pixels = png_pixels(png)
            assert pixels[:2] == (390, 844), pixels[:2]
            digest = hashlib.sha256(b"".join(bytes(row) for row in pixels[3])).hexdigest()
            assert digest == expected, f"the {theme} starter changed: see {png}; digest {digest}"

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
        # init keeps the package's toolchain, and Kryon's checked packages
        # lock only against the exact one the templates pin.
        pinned = tomllib.loads((ROOT / "templates/tui/ziran.toml").read_text())["toolchain"]["ref"]
        manifest = (tool / "ziran.toml").read_text()
        assert 'ref = "master"' in manifest, manifest
        (tool / "ziran.toml").write_text(manifest.replace('ref = "master"', f'ref = "{pinned}"'))
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

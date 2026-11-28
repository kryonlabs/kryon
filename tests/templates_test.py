"""Kryon's project templates and the kryon front door, display-free.

Every template is created with `kryon new` from this checkout. The app's
desktop profile builds and renders PNGs without a display.
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
    "light": "106b594ffe37f34df3540e51d4c7aeb6a1d9a33e95dd3e1f9ea697d62622a616",
    "dark": "04396a069a774432521b245ea26cd1356660e6f767f6f9a06d3476a99a078534",
}


def environment(work):
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
                 "SESSION_MANAGER", "KRYON_FRONT_DOOR",
                 "MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
        env.pop(name, None)
    # The front door runs `ziran` from PATH: the one beside this checkout.
    env["PATH"] = f"{ZIRAN_ROOT / 'build/bin'}:{env['PATH']}"
    env["KRYON_TEMPLATES"] = str(ROOT)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    env["HARMONY_DESKTOP_RECOVERY"] = "0"
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

        # Create and lock each scaffold; route execution is covered separately.
        for template in ("app", "tui", "pages"):
            name = f"demo-{template}"
            created = run([KRYON, "new", name, "--template", template,
                           "--overrides", work / "overrides.toml"], work, env)
            assert f"created {name} from kryon:{template}" in created, created
            manifest = (work / name / "ziran.toml").read_text()
            assert 'tool = "kryon"' in manifest and "{{" not in manifest, manifest
            # new locks the project, so its first build needs no ziran lock.
            assert (work / name / "ziran.lock").is_file(), name

        profiles = run([KRYON, "profiles"], work / "demo-app", env)
        assert profiles.splitlines() == ["* desktop  SDL2/Cairo window", "  portable  SDL2/Cairo window",
                                         "  web  WebGL2 browser page"], profiles

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

        # kryon upgrade moves an app on an older set back to the Ziran, Kryon
        # and CSS this kryon pins, and locks it afresh.
        manifest = work / "demo-app/ziran.toml"
        pinned = manifest.read_text()
        older = pinned
        for table, ref in (("[toolchain]", "5db3d3c39425d5dfe4bfe4a84e6f6e2e5a810ad0"),
                           ("[dependencies.kryon]", "cdf23986a0434cfb01513ecca4763b0477d01a7e"),
                           ("[dependencies.css]", "e2d73fcdd4cffbd745673cbffd4ded7658591365")):
            current = tomllib.loads(pinned)
            for key in table.strip("[]").split("."):
                current = current[key]
            older = older.replace(f'ref = "{current["ref"]}"', f'ref = "{ref}"', 1)
        assert older != pinned
        manifest.write_text(older)
        run([KRYON, "upgrade"], work / "demo-app", env)
        assert manifest.read_text() == pinned, "kryon upgrade restores the template's pins"
        assert (work / "demo-app/ziran.lock").is_file(), "kryon upgrade locks the app"

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
        # The new package floats on Ziran master; init pins the Ziran that
        # Kryon's checked packages need, so the merged project locks.
        pinned = tomllib.loads((ROOT / "templates/tui/ziran.toml").read_text())["toolchain"]["ref"]
        assert 'ref = "master"' in (tool / "ziran.toml").read_text()
        merged = run([KRYON, "init", "--template", "tui"], tool, env)
        assert f"pinned [toolchain] to Ziran {pinned}" in merged, merged
        assert 'kept entry = "src/main.zi" in [package]' in merged, merged
        assert (tool / "ziran.lock").is_file()
        manifest = (tool / "ziran.toml").read_text()
        assert 'entry = "src/main.zi"' in manifest and 'tool = "kryon"' in manifest
        assert "[tool.kryon]" in manifest and 'entry = "src/app.tsx"' in manifest

        # Without the settings ziran passes, the front door never loops.
        guarded = dict(env, KRYON_FRONT_DOOR="1")
        looped = run([KRYON, "build"], work / "demo-app", guarded, succeed=False)
        assert "without project settings" in looped, looped
    print("Kryon templates: creation/pins, app rendering and web checking passed")


if __name__ == "__main__":
    main()

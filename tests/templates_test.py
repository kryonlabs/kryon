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


ROOT = Path(__file__).resolve().parents[1]
# The sibling checkout CI uses, else the org-grouped local layout.
ZIRAN_ROOT = Path(os.environ.get("ZIRAN_ROOT") or next(
    (path for path in (ROOT.parent / "ziran", ROOT.parent.parent / "ziranlang/ziran")
     if path.is_dir()), ROOT.parent / "ziran"))
KRYON = ROOT / "build/bin/kryon"


def environment(work):
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "KRYON_FRONT_DOOR",
                 "MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
        env.pop(name, None)
    # The front door runs `ziran` from PATH: the one beside this checkout.
    env["PATH"] = f"{ZIRAN_ROOT / 'build/bin'}:{env['PATH']}"
    env["XDG_CACHE_HOME"] = str(work / "cache")
    env["KRYON_TEMPLATES"] = str(ROOT)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    # Templates name the public repositories; serve them from here.
    redirects = [("protocol.file.allow", "always"),
                 (f"url.{ZIRAN_ROOT.as_uri()}.insteadOf", "https://github.com/ziranlang/ziran.git"),
                 (f"url.{ROOT.as_uri()}.insteadOf", "https://github.com/kryonlabs/kryon.git")]
    env["GIT_CONFIG_COUNT"] = str(len(redirects))
    for index, (key, value) in enumerate(redirects):
        env[f"GIT_CONFIG_KEY_{index}"] = key
        env[f"GIT_CONFIG_VALUE_{index}"] = value
    return env


def run(command, directory, env, succeed=True, stdin=subprocess.DEVNULL):
    result = subprocess.run([str(part) for part in command], cwd=directory, env=env,
                            stdin=stdin, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=900)
    if succeed != (result.returncode == 0):
        raise AssertionError(f"{' '.join(map(str, command))} returned "
                             f"{result.returncode}:\n{result.stdout}")
    return result.stdout


def local(project):
    (project / "ziran.local.toml").write_text(
        f'[overrides]\nziran = "{ZIRAN_ROOT}"\nKryon = "{ROOT}"\n')


def screen(text):
    """Terminal output without its escape sequences."""
    out, index = [], 0
    while index < len(text):
        if text[index] == "\x1b":
            index += 1
            if index < len(text) and text[index] == "[":
                index += 1
                while index < len(text) and not ("@" <= text[index] <= "~"):
                    index += 1
            index += 1
            continue
        out.append(text[index])
        index += 1
    return "".join(out)


def main():
    assert KRYON.exists(), "build build/bin/kryon first"
    with tempfile.TemporaryDirectory(prefix="kryon-templates-") as directory:
        work = Path(directory)
        env = environment(work)

        usage = run([KRYON, "help"], work, env)
        assert "app (window), tui (terminal), pages (navigation), web (browser page)" in usage

        rendered = {
            "app": ["Count up to the goal, one step at a time.", "Smaller step",
                    "Reset", "Progress to the goal"],
            "tui": ["1 of 6 done", "[x] Add a task list to the app",
                    "Up/Down move  Space toggles  Ctrl+C quits"],
            "pages": ["Home", "Library", "Settings", "Open the library"],
        }
        for template, expected in rendered.items():
            name = f"demo-{template}"
            created = run([KRYON, "new", name, "--template", template], work, env)
            assert f"created {name} from Kryon:{template}" in created, created
            project = work / name
            manifest = (project / "ziran.toml").read_text()
            assert 'tool = "Kryon"' in manifest and "{{" not in manifest, manifest
            local(project)
            # `kryon run PROFILE` goes through ziran to the locked Kryon; with
            # output redirected the terminal host renders one frame.
            output = screen(run([KRYON, "run", "tui"], project, env))
            for line in [name] + expected:
                assert line in output, (template, line, output)
            # tool = "Kryon" makes plain `ziran run` the same command.
            assert screen(run(["ziran", "run", "tui"], project, env)) == output

        profiles = run([KRYON, "profiles"], work / "demo-app", env)
        assert profiles.splitlines() == ["* desktop  SDL2/Cairo window", "  tui  terminal",
                                         "  web  Canvas2D browser page"], profiles
        run([KRYON, "check", "desktop"], work / "demo-app", env)

        run([KRYON, "new", "demo-web", "--template", "web"], work, env)
        local(work / "demo-web")
        run([KRYON, "check"], work / "demo-web", env)

        missing = run([KRYON, "new", "other", "--template", "nosuch"], work, env,
                      succeed=False)
        assert "has no template nosuch; it has app, tui, pages, web" in missing, missing
        assert not (work / "other").exists()

        # kryon init turns a command-line package into a Kryon application:
        # the package keeps its own entry and gains the window's.
        run(["ziran", "new", "tool"], work, env)
        tool = work / "tool"
        merged = run([KRYON, "init", "--template", "tui"], tool, env)
        assert 'kept entry = "src/main.zi" in [package]' in merged, merged
        manifest = (tool / "ziran.toml").read_text()
        assert 'entry = "src/main.zi"' in manifest and 'tool = "Kryon"' in manifest
        assert "[tool.Kryon]" in manifest and 'entry = "src/app.zi"' in manifest
        local(tool)
        assert "1 of 6 done" in screen(run([KRYON, "run"], tool, env))

        # Without the settings ziran passes, the front door never loops.
        guarded = dict(env, KRYON_FRONT_DOOR="1")
        looped = run([KRYON, "build"], work / "demo-app", guarded, succeed=False)
        assert "without project settings" in looped, looped
    print("Kryon templates: app, tui, pages, web ok")


if __name__ == "__main__":
    main()

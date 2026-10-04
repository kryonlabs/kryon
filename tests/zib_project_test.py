"""Export, download and interact with a real Zib app on a private display."""
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import time

from toolchain import ROOT, ZIRAN_ROOT, ZIRAN
from raylib_project_test import png_pixels, rgb, solid_png

OUTPUT = ROOT / "build/zib-test"


def environment():
    env = os.environ.copy()
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                "DBUS_SESSION_BUS_ADDRESS", "SESSION_MANAGER"):
        env.pop(key, None)
    env["PATH"] = str(ROOT / "build/bin") + os.pathsep + str(ZIRAN_ROOT / "build/bin") + os.pathsep + env["PATH"]
    env["ZIRAN_ROOT"] = str(ZIRAN_ROOT)
    env["XDG_CONFIG_HOME"] = str(OUTPUT / "config")
    env["KRYON_TEMPLATES"] = str(ROOT)
    env["KRYON_FONT_PATH"] = str(ROOT / "assets/fonts/LiberationSans-Regular.ttf")
    return env


def run(args, cwd, env, ok=True):
    result = subprocess.run([str(x) for x in args], cwd=cwd, env=env,
                            capture_output=True, timeout=300)
    if ok:
        assert result.returncode == 0, result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace")
    else:
        assert result.returncode != 0, "unexpected success"
    return result


def input_probe(bundle, mode):
    # This process inherits only the private display created by xvfb-run.
    process = subprocess.Popen([str(ROOT / "build/bin/zib"), bundle],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 40
        window = None
        while time.monotonic() < deadline and process.poll() is None:
            found = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid",
                                    str(process.pid)], capture_output=True, text=True)
            if found.returncode == 0 and found.stdout.strip():
                window = found.stdout.splitlines()[0]
                break
            time.sleep(0.1)
        assert window is not None, "player did not open its window"
        asset_directory = Path(os.readlink(f"/proc/{process.pid}/cwd"))
        assert asset_directory.name.startswith("zib-")
        assert (asset_directory / "assets/red.png").is_file()
        subprocess.run(["xdotool", "windowfocus", "--sync", window], check=True)
        # Two event loops must see press and release separately.
        time.sleep(1)
        subprocess.run(["xdotool", "mousemove", "--window", window, "100", "70",
                        "mousedown", "1"], check=True)
        time.sleep(0.4)
        subprocess.run(["xdotool", "mouseup", "1"], check=True)
        time.sleep(0.4)
        if mode == "unicode":
            # Use a stable keymap on this private server: xdotool's temporary
            # mapping races SDL/XIM when it is restored before event lookup.
            subprocess.run(["xmodmap", "-e", "keycode 200 = eacute"], check=True)
            subprocess.run(["xdotool", "key", "eacute"], check=True)
        else:
            subprocess.run(["xdotool", "key", "a"], check=True)
        try:
            out, err = process.communicate(timeout=40)
        except subprocess.TimeoutExpired:
            process.terminate()
            out, err = process.communicate(timeout=10)
            raise AssertionError(f"{mode} input did not finish: {out!r} {err!r}")
        assert b"zib interaction passed" in out, (out, err)
        assert b"interaction failed" not in out, out
        assert not asset_directory.exists(), "player left extracted resources behind"
    finally:
        if process.poll() is None:
            process.terminate()
            process.communicate(timeout=10)


def main():
    env = environment()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    project = OUTPUT / "project"
    project.mkdir(exist_ok=True)
    (project / "src").mkdir(exist_ok=True)
    shutil.copyfile(ROOT / "tests/zib/app.zi", project / "src/app.zi")
    (project / "ziran.toml").write_text('''[package]
name = "zib_probe"
tool = "kryon"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
[install]
bin = "zib_probe"
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
[tool.kryon]
default_profile = "portable"
[tool.kryon.profiles.portable]
backend = "desktop"
codegen = "zib"
assets = "assets"
[tool.kryon.profiles.desktop]
backend = "desktop"
''')
    (project / "ziran.local.toml").write_text(
        f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    (project / "assets").mkdir(exist_ok=True)
    solid_png(project / "assets/red.png", 32, 32, (230, 30, 40, 255))
    run([ZIRAN, "lock"], project, env)
    run([ZIRAN, "tool", "kryon", "build", "portable"], project, env)
    bundle = project / "build/zib_probe-portable.zib"
    assert bundle.is_file()
    ir = project / "build/generated/portable/ir"
    module = next(ir.glob("*_zib_run.zir")).stem
    saved = OUTPUT / "saved.zib"
    run([ZIRAN, "bundle", "--root", ir, "--entry", module + ":main",
         "--asset-dir", "assets=" + str(project / "assets"),
         "-o", saved, ir / (module + ".zir")], project, env)
    assert saved.read_bytes() == bundle.read_bytes()

    # A download contains no source, lock, project manifest or generated code.
    download = OUTPUT / "download with spaces"
    download.mkdir(exist_ok=True)
    downloaded = download / "app.zib"
    shutil.copyfile(bundle, downloaded)
    for extra in download.iterdir():
        if extra != downloaded and extra.is_file():
            extra.unlink()
    assert list(download.iterdir()) == [downloaded]
    shot = OUTPUT / "portable.png"
    capture_env = {**env, "KRYON_CAPTURE_PATH": str(shot)}
    run(["xvfb-run", "-a", ROOT / "build/bin/kryon", "run", downloaded],
        ROOT, capture_env)
    png = png_pixels(shot)
    assert png[:2] == (960, 600)
    assert rgb(png, 550, 130) == (230, 30, 40), "downloaded asset did not render"
    assert rgb(png, 10, 10) == (248, 250, 252), "background did not render"
    assert any(sum(rgb(png, x, y)) < 400 for y in range(128, 165)
               for x in range(40, 350)), "text did not render"

    run([ZIRAN, "tool", "kryon", "build", "desktop"], project, env)
    native = OUTPUT / "native.png"
    run(["xvfb-run", "-a", project / "build/zib_probe-desktop"], project,
        {**env, "KRYON_CAPTURE_PATH": str(native)})
    assert png_pixels(native) == png, "Zib pixels diverged from native app"
    prefix = OUTPUT / "install with spaces"
    run([ZIRAN, "install", "--prefix", prefix], project, env)
    desktop = prefix / "share/applications/zib_probe.desktop"
    executable = next(line[5:] for line in desktop.read_text().splitlines()
                      if line.startswith("Exec="))
    assert shlex.split(executable) == [str(prefix / "bin/zib_probe")]
    installed = OUTPUT / "installed.png"
    run(["xvfb-run", "-a", *shlex.split(executable)], ROOT,
        {**env, "KRYON_CAPTURE_PATH": str(installed)})
    assert png_pixels(installed) == png, "installed Zib app diverged"
    for mode in ("keyboard", "unicode"):
        run(["xvfb-run", "-a", sys.executable, __file__, "--input", downloaded, mode],
            ROOT, env)

    # Malformed files and unknown capabilities fail before opening a display.
    broken = OUTPUT / "broken.zib"
    broken.write_bytes(downloaded.read_bytes()[:24])
    run([ROOT / "build/bin/zib", broken], ROOT, env, ok=False)
    unknown = OUTPUT / "unknown.zi"
    unknown.write_text('host_api :: #system_library "host_api";\n'
                       'DangerousHost :: () -> s32 #foreign host_api;\n'
                       '#program_export\nmain :: () -> s32 { return DangerousHost() }\n')
    bad_bundle = OUTPUT / "unknown.zib"
    run([ZIRAN, "bundle", "--root", OUTPUT, "--entry", "unknown:main",
         "-o", bad_bundle, unknown], ROOT, env)
    rejected = run([ROOT / "build/bin/zib", bad_bundle], ROOT, env, ok=False)
    assert b"unsupported host capability unknown:DangerousHost" in rejected.stdout

    # Every maintained starter can be exported through its ordinary front door.
    templates = OUTPUT / "templates"
    templates.mkdir(exist_ok=True)
    overrides = OUTPUT / "overrides.toml"
    overrides.write_text(f'[overrides]\nkryon = "{ROOT}"\nziran = "{ZIRAN_ROOT}"\n')
    for template in ("app", "tui", "pages", "web"):
        name = "demo-" + template
        app = templates / name
        if app.exists():
            shutil.rmtree(app)
        run([ROOT / "build/bin/kryon", "new", name, "--template", template,
             "--overrides", overrides], templates, env)
        run([ROOT / "build/bin/kryon", "build", "portable"], app, env)
        portable_shot = OUTPUT / (template + "-portable.png")
        run(["xvfb-run", "-a", ROOT / "build/bin/kryon", "run",
             app / ("build/" + name + "-portable.zib")], ROOT,
            {**env, "KRYON_CAPTURE_PATH": str(portable_shot)})
        run([ROOT / "build/bin/kryon", "build", "desktop"], app, env)
        native_shot = OUTPUT / (template + "-native.png")
        run(["xvfb-run", "-a", app / ("build/" + name + "-desktop")], app,
            {**env, "KRYON_CAPTURE_PATH": str(native_shot)})
        assert png_pixels(portable_shot) == png_pixels(native_shot), template
    print("Zib export, saved IR, downloaded rendering, installation, native pixel parity, "
          "persistent click state, keyboard, Unicode, malformed and unavailable host checks "
          "and all four starter apps passed")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--input":
        input_probe(sys.argv[2], sys.argv[3])
    else:
        main()

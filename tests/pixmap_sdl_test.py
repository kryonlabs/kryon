#!/usr/bin/env python3
"""Native presenter authority/lifecycle tests and real private pixel transport."""
import os
from pathlib import Path
import select
import shlex
import subprocess
import sys
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT

OUT = ROOT / "build/scratch/pixmap-sdl/verify"
REQUESTED_DISPLAY = os.environ.get("KRYON_TEST_DISPLAY", "")
ENV = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
             "SESSION_MANAGER", "DESKTOP_SESSION", "XDG_SESSION_ID", "XDG_SESSION_TYPE",
             "XDG_SESSION_DESKTOP", "XDG_CURRENT_DESKTOP", "XDG_RUNTIME_DIR",
             "SDL_RENDER_DRIVER", "SDL_FRAMEBUFFER_ACCELERATION", "LD_PRELOAD",
             "KRYON_PIXMAP_GPU", "KRYON_TEST_DISPLAY"):
    ENV.pop(name, None)
ENV.update(HARMONY_DESKTOP_RECOVERY="0", YUE_DESKTOP_RECOVERY="0",
           SDL_VIDEODRIVER="x11", LIBGL_ALWAYS_SOFTWARE="1")

def run(args, env=ENV):
    subprocess.run(list(map(str, args)), cwd=ROOT, env=env, check=True)

def build(name, saved=False):
    folder = OUT / name
    initial = folder / "ir"
    initial.mkdir(parents=True, exist_ok=True)
    source = ROOT / "tests" / (name + ".zi")
    if not saved:
        run([ZIRAN, "ir", "--project", "--root", ROOT / "tests", "-o", initial, source])
        ir = initial
    else:
        ir = folder / "saved-ir"
        ir.mkdir(exist_ok=True)
        run([ZIRAN, "ir", "--root", initial, "--module-path", initial,
             "-o", ir, initial / (name + ".zir")])
    generated = folder / "c"
    generated.mkdir(exist_ok=True)
    for suffix in ("*.c", "*.h"):
        for file in generated.glob(suffix):
            file.unlink()
    run([ZIRAN.with_name("zi2c"), "--root", ir, "--entry", name + ":main",
         "-o", generated, *sorted(ir.glob("*.zir"))])
    executable = folder / name
    compiler = shlex.split(os.environ.get("KRYON_TEST_CC", os.environ.get("CC", "cc")))
    run([*compiler, "-std=c11", "-O2", "-I" + str(ZIRAN_ROOT / "include"),
         "-iquote", generated, *sorted(generated.glob("*.c")),
         "-lSDL2", "-lm", "-o", executable])
    return executable

def actual(executable, env):
    for mode in ("default", "empty-driver", "env-software", "env-driver",
                 "hint-driver", "env-surface", "hint-surface", "force-gpu"):
        current = dict(env, PIXMAP_SDL_TEST=mode)
        if mode == "force-gpu":
            current["KRYON_PIXMAP_GPU"] = "1"
        if mode not in ("default", "hint-surface", "hint-driver"):
            current["SDL_RENDER_DRIVER"] = ("opengl" if mode == "env-driver"
                                            else "software" if mode == "env-software" else "")
            current["SDL_FRAMEBUFFER_ACCELERATION"] = "1" if mode == "env-surface" else "0"
        run([executable], current)
    run([executable], dict(ENV, PIXMAP_SDL_TEST="fallback", SDL_VIDEODRIVER="dummy"))

def verify(executable, env):
    actual(executable, env)
    actual(build("pixmap_sdl_test", saved=True), env)
    if "--gpu" in sys.argv:
        geometry = build("pixmap_gpu_test")
        run([geometry], dict(env, KRYON_PIXMAP_GPU="1"))
        for choice in ("", "0"):
            completed = subprocess.run([str(geometry)], cwd=ROOT,
                env=dict(env, KRYON_PIXMAP_GPU=choice), text=True,
                capture_output=True)
            assert completed.returncode != 0 and "GPU recording unavailable" in completed.stdout

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for saved in (False, True):
        run([build("pixmap_presenter_selection_test", saved)])
    executable = build("pixmap_sdl_test")
    if REQUESTED_DISPLAY:
        assert REQUESTED_DISPLAY.startswith(":")
        number = REQUESTED_DISPLAY[1:]
        assert number.isdigit() and int(number) != 0, "KRYON_TEST_DISPLAY must name a private X display"
        verify(executable, dict(ENV, DISPLAY=REQUESTED_DISPLAY))
        print("Presenter source/saved selection, private actual pixels/resize and fallback passed")
        return
    readfd, writefd = os.pipe()
    with (OUT / "xvfb.log").open("w") as log:
        server = subprocess.Popen(["Xvfb", "-displayfd", str(writefd), "-screen", "0",
                                   "1280x800x24", "-nolisten", "tcp", "-ac"],
                                  pass_fds=(writefd,), env=ENV, stdout=log, stderr=log)
        os.close(writefd)
        try:
            assert select.select([readfd], [], [], 30)[0], "private Xvfb startup failed"
            display = os.read(readfd, 32).decode().strip()
            assert display.isdigit() and server.poll() is None
            env = dict(ENV, DISPLAY=":" + display)
            verify(executable, env)
        finally:
            os.close(readfd)
            server.terminate()
            server.wait(timeout=5)
    print("Presenter source/saved selection, private actual pixels/resize and fallback passed")

if __name__ == "__main__":
    main()

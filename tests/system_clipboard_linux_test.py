"""Paste PNG images from the X11 clipboard into a Kryon text field.

xclip owns the clipboard of a private Xvfb display, offering only image/png.
A desktop probe presses Ctrl+V in its focused TextField: the field reports a
paste that found no text, and the probe reads the image with
SystemClipboardImage. A large image arrives in INCR increments. The probe
draws only its own window, and the inherited display is never used.
"""

import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import time
import zlib

from raylib_project_test import png_pixels, run
from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
ZIRAN = str(ZIRAN_ROOT / "build/bin/ziran")

APP = '''Desktop :: #import "kryon/Desktop";
using UI :: #import "kryon/Widgets";
using Clipboard :: #import "kryon/SystemClipboardLinux";
#import "kryon/Frame"
#import "kryon/geometry"
#import "std/byte_text_linux"
#import "std/number_text"

libc :: #system_library "c";
Write :: (descriptor: s32, bytes: *u8, count: u64) -> s64 #foreign libc "write";
GetEnvironment :: (name: *u8) -> *u8 #foreign libc "getenv";
Allocate :: (count: usize, bytes: usize) -> *u8 #foreign libc "calloc";

// Fixed arrays stop at a megabyte; a screenshot needs more.
PNG_CAPACITY :: 8388608;
tiny: [64]u8;

Say :: (descriptor: s32, text: string) {
    line: [256]u8
    length: s32 = 0
    while length < cast(s32)text.count && length < 255 {
        line[length] = text[length]
        length += 1
    }
    line[length] = cast(u8)10
    unused Write(descriptor, *line[0], cast(u64)(length + 1))
}

Mode :: () -> string {
    name: [16]u8 = .{80, 82, 79, 66, 69, 95, 77, 79, 68, 69, 0, 0, 0, 0, 0, 0}
    return TextFromCString(GetEnvironment(*name[0]))
}

// available found size png-bytes, for the cases without a window.
Report :: (result: SystemClipboardImageResult) {
    digits: [32]u8
    line: [128]u8
    length: s32 = 0
    words: [4]s64 = .{0, 0, result.size, result.png.count}
    if result.available { words[0] = 1 }
    if result.found { words[1] = 1 }
    for word: 0..3 {
        text: string = FormatInteger(words[word], digits[:])
        for index: 0..cast(s32)text.count - 1 {
            line[length] = text[index]
            length += 1
        }
        if word < 3 {
            line[length] = cast(u8)32
            length += 1
        }
    }
    Say(1, TextView(line[0:length]))
}

#program_export
main :: () -> s32 {
    mode: string = Mode()
    storage: *u8 = Allocate(cast(usize)PNG_CAPACITY, 1)
    if storage == null { return 1 }
    png: []u8 = BytesFromPointer(storage, cast(u64)PNG_CAPACITY)
    if mode == "read" {
        Report(SystemClipboardImage(png[:]))
        return 0
    }
    if mode == "tiny" {
        Report(SystemClipboardImage(tiny[:]))
        return 0
    }
    if !Desktop.OpenDesktopSized(320, 80) { return 2 }
    session: Session = SessionOpen()
    if !SessionValid(session) { return 3 }
    frame: s32 = 0
    while Desktop.PollDesktop() && frame < 900 {
        KeyboardModifiersSend(session, Desktop.DesktopModifiers())
        key: s32 = Desktop.DesktopPressedKey()
        if key != 0 { unused KeyboardSend(session, key) }
        codepoint: s32 = Desktop.DesktopTypedCodepoint()
        while codepoint != 0 {
            unused TypedCodepointSend(session, codepoint)
            codepoint = Desktop.DesktopTypedCodepoint()
        }
        Desktop.BeginDesktopFrame()
        if BeginFrame(session, cast(u64)1, Desktop.DesktopViewport()) !=
            FrameStatus.FrameOk { return 4 }
        typed: [64]u8
        field: TextFieldProps
        field.key = cast(u64)7
        field.bounds = Rectangle.{10.0, 10.0, 300.0, 32.0}
        field.value = "keep"
        field.anchor = 0
        field.cursor = 4
        field.focused = true
        field.input = TextFieldInputFor(KeyboardTake(session),
            KeyboardModifiers(session), TypedTextTake(session, typed[:]))
        result: TextFieldResult = TextField(session, field)
        if EndFrame(session) != FrameStatus.FrameOk { return 5 }
        if !Desktop.EndDesktopFrame() { return 6 }
        if frame == 0 { Say(2, "ready") }
        // A text paste would replace the selected "keep".
        if result.edit.changed { return 7 }
        if result.paste_without_text {
            image: SystemClipboardImageResult = SystemClipboardImage(png[:])
            if !image.found || image.png.count != image.size { return 8 }
            unused Write(1, storage, cast(u64)image.png.count)
            unused CloseSession(session)
            Desktop.CloseDesktop()
            return 0
        }
        Desktop.PauseDesktop()
        frame += 1
    }
    return 9
}
'''


def png_file(path, width, height, noise):
    """An RGB PNG; noise makes it incompressible, so it needs INCR."""
    rows = bytearray()
    generator = random.Random(width * 7919 + height)
    for y in range(height):
        rows.append(0)
        if noise:
            rows += generator.randbytes(width * 3)
        else:
            rows += b"".join(bytes(((x * 5) & 255, (y * 3) & 255, 160)) for x in range(width))

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    data = (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b""))
    path.write_bytes(data)
    return data


def private_environment():
    env = os.environ.copy()
    for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS"):
        env.pop(name, None)
    return env


# Owns the clipboard like a GTK application that copied an image (offered as
# image/png, re-encoded by GTK) or text; it refuses targets it does not hold.
GTK_OWNER = r"""
import signal
import sys
import gi
gi.require_version("Gdk", "3.0")
gi.require_version("Gtk", "3.0")
from gi.repository import Gdk, GdkPixbuf, GLib, Gtk
clipboard = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD)
if sys.argv[1] == "--text":
    clipboard.set_text(sys.argv[2], -1)
else:
    clipboard.set_image(GdkPixbuf.Pixbuf.new_from_file(sys.argv[1]))
GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, signal.SIGTERM, Gtk.main_quit)
print("owned", flush=True)
Gtk.main()
"""

# Runs inside xvfb-run, on the private display only. xclip serves its bytes
# for every target, so it stands in only where no text is requested first;
# a large image it serves to a text request stalls in INCR.
SCENARIOS = r"""
set -eu
probe=$1
work=$2
owner=
release() {
    if test -n "$owner"; then kill "$owner" 2>/dev/null || true; wait "$owner" 2>/dev/null || true; fi
    owner=
}
xclip_owns() {
    release
    xclip -selection clipboard -display "$DISPLAY" -loops 0 "$@" &
    owner=$!
    sleep 0.3
}
gtk_owns() {
    release
    python3 "$work/gtk_owner.py" "$@" > "$work/owner.log" 2>&1 &
    owner=$!
    tries=0
    until grep -q owned "$work/owner.log"; do
        tries=$((tries + 1)); test "$tries" -lt 200; sleep 0.05
    done
}
trap release EXIT
# Nothing owns the clipboard.
test "$(PROBE_MODE=read "$probe")" = "1 0 0 0"
# Text only: no PNG is offered.
gtk_owns --text "just text"
test "$(PROBE_MODE=read "$probe")" = "1 0 0 0"
# The exact bytes arrive, whole and in INCR increments.
for name in small large; do
    xclip_owns -t image/png -i "$work/$name.png"
    test "$(PROBE_MODE=read "$probe")" = "1 1 $(wc -c < "$work/$name.png") $(wc -c < "$work/$name.png")"
done
# An image larger than the caller's buffer reports its size and no bytes.
xclip_owns -t image/png -i "$work/small.png"
test "$(PROBE_MODE=tiny "$probe")" = "1 1 $(wc -c < "$work/small.png") 0"
# Ctrl+V in the focused field pastes the image a GTK application copied.
for name in small large; do
    gtk_owns "$work/$name.png"
    PROBE_MODE=paste "$probe" > "$work/pasted-$name.png" 2> "$work/probe-$name.log" &
    pid=$!
    window=$(xdotool search --sync --pid "$pid" | head -n 1)
    tries=0
    until grep -q ready "$work/probe-$name.log"; do
        tries=$((tries + 1)); test "$tries" -lt 200; sleep 0.05
    done
    xdotool windowfocus --sync "$window"
    xdotool key --clearmodifiers ctrl+v
    status=0
    wait "$pid" || status=$?
    test "$status" = 0 || { echo "probe exited $status for $name" >&2; cat "$work/probe-$name.log" >&2; exit 1; }
done
"""


def main():
    for tool in ("xvfb-run", "xclip", "xdotool"):
        assert shutil.which(tool), f"Install {tool} to test clipboard images"
    with tempfile.TemporaryDirectory(prefix="kryon-clipboard-image-") as directory:
        project = Path(directory)
        (project / "src").mkdir()
        (project / "ziran.toml").write_text(
            '[package]\nname = "clipboard_probe"\nentry = "src/app.zi"\n'
            'module_roots = ["src"]\n\n'
            '[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\n'
            'ref = "master"\n\n'
            '[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\n'
            'ref = "master"\n'
        )
        overrides = {"kryon": str(ROOT), "ziran": str(ZIRAN_ROOT)}
        if os.environ.get("RAYLIB_SOURCE"):
            overrides["raylib"] = os.environ["RAYLIB_SOURCE"]
        (project / "ziran.local.toml").write_text(
            "[overrides]\n" + "".join(f'{key} = "{value}"\n' for key, value in overrides.items())
        )
        (project / "src/app.zi").write_text(APP)
        env = private_environment()
        run([ZIRAN, "lock"], project, env)
        generated = project / "generated"
        run([ZIRAN, "build", "--project", "--target=c", "--entry", "app:main",
             "-o", str(generated), "src/app.zi"], project, env)
        libraries = subprocess.run(["pkg-config", "--libs", "sdl2", "cairo", "freetype2", "fontconfig"],
                                   check=True, capture_output=True,
                                   text=True).stdout.split()
        run([os.environ.get("CC", "cc"), "-std=c99",
             f"-I{ZIRAN_ROOT / 'include'}", f"-I{generated}",
             *map(str, sorted(generated.glob("*.c"))), *libraries, "-ldl", "-lm",
             "-o", "probe"], project, env)

        small = png_file(project / "small.png", 64, 48, False)
        large = png_file(project / "large.png", 900, 900, True)
        assert len(small) < 65536 and len(large) > 2 * 1024 * 1024, (len(small), len(large))

        # Without a display there is no clipboard to read.
        result = subprocess.run([str(project / "probe")], cwd=project, text=True,
                                capture_output=True, env={**env, "PROBE_MODE": "read"},
                                timeout=30)
        assert result.returncode == 0 and result.stdout.strip() == "0 0 0 0", result

        started = time.monotonic()
        (project / "gtk_owner.py").write_text(GTK_OWNER)
        (project / "scenarios.sh").write_text(SCENARIOS)
        run(["timeout", "--kill-after=5s", "120s", "xvfb-run", "-a",
             "sh", str(project / "scenarios.sh"), str(project / "probe"), str(project)],
            project, env)
        elapsed = time.monotonic() - started
        # GTK re-encodes the image; the pixels must survive the paste.
        for name in ("small", "large"):
            pasted = png_pixels(project / f"pasted-{name}.png")
            assert pasted == png_pixels(project / f"{name}.png"), name
        pasted_size = (project / "pasted-large.png").stat().st_size
    print(f"clipboard images: Ctrl+V in a TextField pasted GTK-copied images "
          f"(64x48, and 900x900 as a {pasted_size}-byte INCR PNG) pixel-exact; "
          f"xclip's {len(small)}- and {len(large)}-byte PNGs read byte-exact; "
          f"empty, text-only, oversize and no-display cases passed on a private "
          f"Xvfb display ({elapsed:.1f}s)")


if __name__ == "__main__":
    main()

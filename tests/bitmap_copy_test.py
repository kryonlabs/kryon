#!/usr/bin/env python3
"""Existing bitmap tests with source/saved native and portable execution.

Run through build-queue. KRYON_TEST_CC or CC selects the native compiler.
"""
import os
import shlex
from pathlib import Path
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, UI_MODULE_ARGS

OUT = ROOT / "build/scratch/bitmap-store/verify"
ENV = dict(os.environ)
for name in ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "DBUS_SESSION_BUS_ADDRESS",
             "SESSION_MANAGER", "DESKTOP_SESSION"):
    ENV.pop(name, None)
ENV["HARMONY_DESKTOP_RECOVERY"] = "0"

def run(args):
    subprocess.run(list(map(str, args)), cwd=ROOT, env=ENV, check=True)

def native(ir, entry, generated):
    generated.mkdir(parents=True, exist_ok=True)
    for suffix in ("*.c", "*.h"):
        for old in generated.glob(suffix):
            old.unlink()
    run([ZIRAN.with_name("zi2c"), "--entry", entry, "--root", ir, "-o", generated,
         *sorted(ir.glob("*.zir"))])
    executable = generated / entry.split(":")[0]
    compiler=shlex.split(os.environ.get("KRYON_TEST_CC",os.environ.get("CC","cc")))
    run([*compiler,
         "-std=c11", "-O2", "-ffunction-sections", "-fdata-sections",
         "-I" + str(ZIRAN_ROOT / "include"), "-iquote", generated,
         *sorted(generated.glob("*.c")), "-Wl,--gc-sections", "-lm", "-o", executable])
    run([executable])

def verify(name, bindings=(), portable=True):
    entry = name + ":main"
    source = ROOT / "tests" / (name + ".zi")
    directory = OUT / name
    initial = directory / "ir"
    initial.mkdir(parents=True, exist_ok=True)
    for clean in (initial,directory/"saved-ir"):
        for old in clean.glob("*.zir"):
            old.unlink()
    run([ZIRAN, "ir", "--project", "--root", ROOT / "tests",
         "-o", initial, source])
    for form in ("source", "saved"):
        root = ROOT / "tests" if form == "source" else initial
        input_file = source if form == "source" else initial / (name + ".zir")
        ir = initial if form == "source" else directory / "saved-ir"
        if form == "saved":
            ir.mkdir(parents=True, exist_ok=True)
            run([ZIRAN, "ir", "--root", root, *UI_MODULE_ARGS,
                 "--module-path", ZIRAN_ROOT / "std",
                 "-o", ir, input_file])
        native(ir, entry, directory / form)
        if portable:
            bundle = directory / (form + ".zx")
            run([ZIRAN, "bundle", "--project", "--root", root, "--entry", entry,
                 *bindings, "-o", bundle, input_file])
            completed = subprocess.run([str(ZIRAN), "run", str(bundle)], cwd=ROOT,
                                       env=ENV, check=True, text=True, capture_output=True)
            print(completed.stdout, end="")
            assert completed.stdout.rstrip().splitlines()[-1] == "0", completed.stdout[-1000:]
        if not portable:
            break

if __name__ == "__main__":
    verify("bitmap_store_test")
    bindings = []
    for source, destination in (
        ("raster:RasterLine", "pixmap:RasterLine"),
        ("raster_shape:RasterRoundedRectangle", "pixmap:RasterRoundedRectangle"),
        ("raster_shape:RasterRoundedRectangleOutline", "pixmap:RasterRoundedRectangleOutline"),
        ("raster_text:RasterText", "pixmap:RasterText"),
        ("raster_text:RasterTextClipped", "pixmap:RasterTextClipped"),
        ("raster_text:RasterTextRotated", "pixmap:RasterTextRotated"),
        ("paint_queue:RasterImage", "pixmap:RasterImage"),
        ("paint_queue:RasterImageMesh", "pixmap:RasterImageMesh"),
        ("image_raster:ImageWidth", "pixmap:ImageWidth"),
        ("image_raster:ImageHeight", "pixmap:ImageHeight"),
        ("font_metrics:MeasureGlyphWidth", "pixmap:MeasureGlyphWidth"),
        ("font_metrics:MeasureGlyphLineHeight", "pixmap:MeasureGlyphLineHeight")):
        bindings.extend(("--bind", source + "=" + destination))
    verify("bitmap_image_test", bindings)
    verify("bitmap_budget_test", portable=False)
    print("Bitmap exact alpha, source/saved native+portable, Image lifecycle and budgets passed")

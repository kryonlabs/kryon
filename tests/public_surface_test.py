"""Compile the public Widgets API as an ordinary downstream application."""
import os
import shutil
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, kryon_test_project

project = ROOT / "build/scratch/public-surface/project"
package_id = kryon_test_project(project)
source = project / "src/public_widgets.zi"
source.write_bytes((ROOT / "tests/public_widgets.zi").read_bytes())
subprocess.run([str(ZIRAN), "ir", "--project", "--offline", "--root", str(source.parent),
                "-o", str(ROOT / "build/ziran/public/widgets"), str(source)],
               cwd=project, check=True)
print("Public Widgets API consumer compiled")

renderers = project / "src/public_renderers.zi"
renderers.write_bytes((ROOT / "tests/public_renderers.zi").read_bytes())
bundle = ROOT / "build/scratch/public-surface/renderers.zx"
host = project / "src/cui_overlays_raster_host.zi"
host.write_text((ROOT / "tests/cui_overlays_raster_host.zi").read_text()
                .replace('#import "drawing_props"', '#import "kryon/drawing_props"')
                .replace('#import "geometry"', '#import "kryon/geometry"')
                .replace('#import "pixmap"', '#import "kryon/pixmap"'))
bindings = []
for module, function, target in [
    ("font_metrics", "MeasureGlyphWidth", "CuiOverlaysMeasureGlyphWidth"),
    ("font_metrics", "MeasureGlyphLineHeight", "CuiOverlaysMeasureGlyphLineHeight"),
    ("image_raster", "ImageWidth", "CuiOverlaysImageWidth"),
    ("image_raster", "ImageHeight", "CuiOverlaysImageHeight"),
    ("raster_text", "RasterText", "CuiOverlaysRasterText"),
    ("raster_text", "RasterTextClipped", "CuiOverlaysRasterTextClipped"),
    ("raster_text", "RasterTextRotated", "CuiOverlaysRasterTextRotated"),
    ("raster_shape", "RasterRoundedRectangle", "CuiOverlaysRasterRoundedRectangle"),
    ("raster_shape", "RasterRoundedRectangleOutline", "CuiOverlaysRasterRoundedRectangleOutline"),
    ("raster", "RasterLine", "CuiOverlaysRasterLine"),
    ("raster_triangles", "RasterTriangles", "CuiOverlaysRasterTriangles"),
    ("paint_queue", "RasterImage", "CuiOverlaysRasterImage"),
    ("paint_queue", "RasterImageMesh", "CuiOverlaysRasterImageMesh"),
]:
    bindings += ["--bind", f"{package_id}_{module}:{function}=cui_overlays_raster_host:{target}"]
work = ROOT / "build/scratch/public-surface"
ir = work / "ir"
shutil.rmtree(ir, ignore_errors=True)
subprocess.run([str(ZIRAN), "ir", "--project", "--offline", "--root", str(renderers.parent),
                "-o", str(ir), str(renderers)], cwd=project, check=True)
bundles = []
for mode in ("source", "saved"):
    source_args = (["--project", "--offline", "--root", str(renderers.parent)] if mode == "source"
                   else ["--root", str(ir), "--module-path", str(ir)])
    module = renderers if mode == "source" else ir / "public_renderers.zir"
    bundle = work / (mode + ".zx")
    subprocess.run([str(ZIRAN), "bundle", *source_args, *bindings,
                    "--entry", "public_renderers:main", "-o", str(bundle), str(module)],
                   cwd=project, check=True)
    result = subprocess.check_output([str(ZIRAN), "run", str(bundle)], cwd=project, text=True).strip()
    assert result == "0", (mode, result)
    bundles.append(bundle)
    generated = work / ("c-" + mode)
    shutil.rmtree(generated, ignore_errors=True)
    subprocess.run([str(ZIRAN), "build", "--target=c", *source_args, *bindings,
                    "--entry", "public_renderers:main", "-o", str(generated), str(module)],
                   cwd=project, check=True)
    includes = [ZIRAN_ROOT / "include", generated,
                *sorted({p.parent for p in generated.rglob("*.h")})]
    subprocess.run([os.environ.get("CC", "cc"), "-O1", "-std=c11", "-ffunction-sections",
                    "-fdata-sections", "-Wl,--gc-sections", *["-I" + str(p) for p in includes],
                    *map(str, sorted(generated.rglob("*.c"))), str(ZIRAN_ROOT / "build/libziran.a"),
                    "-lm", "-lpthread", "-o", str(generated / "test")], check=True)
    subprocess.run([str(generated / "test")], check=True)
assert bundles[0].read_bytes() == bundles[1].read_bytes()
print("Public renderer local tests: matching source/saved ZX, VM/native raster, retained disabled EmptyState/Banner controls and pointer/keyboard reenable passed")

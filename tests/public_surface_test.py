"""Compile the public widget packages as an ordinary downstream application."""
import os
import shutil
import subprocess
from toolchain import ROOT, ZIRAN, ZIRAN_ROOT, kryon_test_project

project = ROOT / "build/scratch/public-surface/project"
shutil.rmtree(project, ignore_errors=True)
kryon_test_project(project)
source = project / "src/public_widgets.tsx"
source.write_bytes((ROOT / "tests/public_widgets.tsx").read_bytes())
widget_ir = ROOT / "build/ziran/public/widgets"
shutil.rmtree(widget_ir, ignore_errors=True)
subprocess.run([str(ZIRAN), "ir", "--project", "--offline", "--root", str(source.parent),
                "-o", str(widget_ir), str(source)],
               cwd=project, check=True)
print("Public widget package consumer compiled")

work = ROOT / "build/scratch/public-surface"

# A dependency consumer must share material types across all portable scene
# exports. Listing the optional native GPU export must not pull EGL into it.
materials = project / "src/public_scene_materials.zi"
materials.write_bytes((ROOT / "tests/public_scene_materials.zi").read_bytes())
material_ir = work / "scene-ir"
shutil.rmtree(material_ir, ignore_errors=True)
subprocess.run([str(ZIRAN), "ir", "--project", "--offline", "--root", str(materials.parent),
                "-o", str(material_ir), str(materials)], cwd=project, check=True)
material_bundles = []
for mode in ("source", "saved"):
    source_args = (["--project", "--offline", "--root", str(materials.parent)] if mode == "source"
                   else ["--root", str(material_ir), "--module-path", str(material_ir)])
    module = materials if mode == "source" else material_ir / "public_scene_materials.zir"
    bundle = work / ("scene-" + mode + ".zx")
    subprocess.run([str(ZIRAN), "bundle", *source_args, "--entry", "public_scene_materials:main",
                    "-o", str(bundle), str(module)], cwd=project, check=True)
    assert subprocess.check_output([str(ZIRAN), "run", str(bundle)], cwd=project, text=True).strip() == "0"
    material_bundles.append(bundle)
    generated = work / ("scene-c-" + mode)
    shutil.rmtree(generated, ignore_errors=True)
    subprocess.run([str(ZIRAN), "build", "--target=c", *source_args,
                    "--entry", "public_scene_materials:main", "-o", str(generated), str(module)],
                   cwd=project, check=True)
    includes = [ZIRAN_ROOT / "include", generated,
                *sorted({p.parent for p in generated.rglob("*.h")})]
    subprocess.run([os.environ.get("CC", "cc"), "-O2", "-std=c11",
                    *["-I" + str(p) for p in includes], *map(str, sorted(generated.rglob("*.c"))),
                    "-lm", "-lpthread", "-o", str(generated / "test")], check=True)
    subprocess.run([str(generated / "test")], check=True)
assert material_bundles[0].read_bytes() == material_bundles[1].read_bytes()
print("Public scene/material/GLSL exports: matching source/saved ZX and native/VM pixels passed without EGL")

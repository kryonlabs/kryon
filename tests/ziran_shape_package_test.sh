#!/bin/sh
# A source check misses invalid C slot thunks in imported package modules.
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/shape-package"
mkdir -p "$work"
mkdir "$work/.running"
trap 'rmdir "$work/.running"' EXIT HUP INT TERM
project="$work/project"
mkdir -p "$project/src"
raylib=$(cd "$repo" && "$ziran" pkg path raylib)

# Only the fixture lives here; dependency sources stay in the canonical
# checkout and the selected toolchain is never updated or rebuilt.
python3 - "$repo" "$project" "$ziran_root" "$raylib" <<'PY'
import json
from pathlib import Path
import sys
repo, project, toolchain, raylib = map(Path, sys.argv[1:])
(project / "src/app.zi").write_text(
    (repo / "tests/ziran_shape_package_test.zi").read_text())
for name in ("ziran_shape_widget_host", "text_input_platform_host"):
    source = (repo / f"tests/{name}.zi").read_text()
    for module in ("drawing_props", "geometry", "session"):
        source = source.replace(f'#import "{module}"', f'#import "kryon/{module}"')
    (project / f"src/{name}.zi").write_text(source)
(project / "ziran.toml").write_text('''[package]
name = "shape_consumer"
module_roots = ["src"]
[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"
[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"
''')
(project / "ziran.local.toml").write_text('[overrides]\n' + ''.join(
    f'{name} = {json.dumps(str(path))}\n'
    for name, path in (("kryon", repo), ("ziran", toolchain), ("raylib", raylib))))
PY

cd "$project"
"$ziran" lock --offline
"$ziran" build --project --offline --target=c --prune-stale --exe --entry app:main \
    -o "$work/c" src/app.zi
"$work/c/app"

set --
for effect in raster_triangles:RasterTriangles \
    raster_shape:RasterRoundedRectangle raster_shape:RasterRoundedRectangleOutline \
    raster:RasterLine raster_text:RasterText raster_text:RasterTextClipped \
    paint_queue:RasterImage paint_queue:RasterImageMesh; do
    set -- "$@" --bind "kryon/$effect=ziran_shape_widget_host:${effect#*:}"
done
"$ziran" bundle --project --offline "$@" --entry app:main \
    -o "$work/app.zx" src/app.zi
test "$("$ziran" run "$work/app.zx" | tail -1)" = 0
printf 'External package Shape C execution and portable clipping passed.\n'

#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/grid-multi"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER ICEAUTHORITY
export YUE_DESKTOP_RECOVERY=0 GOCACHE="$work/go-cache"
source="$repo/tests/grid_multi_test.zi"
python3 - "$repo" "$work" "$ziran_root" <<'PUBLIC'
import pathlib, shutil, sys, tomllib
repo, work, ziran_root = map(pathlib.Path, sys.argv[1:])
consumer = work / 'consumer'
consumer.mkdir(exist_ok=True)
shutil.copyfile(repo / 'tests/grid_multi_public_test.zi', consumer / 'grid_multi_public_test.zi')
(consumer / 'ziran.toml').write_text('[package]\nname = "grid_consumer"\nentry = "grid_multi_public_test.zi"\nmodule_roots = ["."]\n[toolchain]\ngit = "https://github.com/ziranlang/ziran.git"\nref = "master"\n[dependencies.kryon]\ngit = "https://github.com/kryonlabs/kryon.git"\nref = "master"\n')
overrides = {'kryon': repo, 'ziran': ziran_root}
local = repo / 'ziran.local.toml'
if local.exists():
    for name, path in tomllib.loads(local.read_text()).get('overrides', {}).items():
        overrides[name] = (repo / path).resolve()
(consumer / 'ziran.local.toml').write_text('[overrides]\n' + ''.join(f'{name} = "{path}"\n' for name, path in overrides.items()))
PUBLIC
(cd "$work/consumer" && "$ziran" lock)
(cd "$work/consumer" && "$ziran" ir --project --root . -o "$work/public-ir" grid_multi_public_test.zi)
"$ziran" ir --root "$work/public-ir" --module-path "$work/public-ir" -o "$work/public-saved-ir" "$work/public-ir/grid_multi_public_test.zir"
cmp "$work/public-ir/grid_multi_public_test.zir" "$work/public-saved-ir/grid_multi_public_test.zir"
set -- \
 --bind image_raster:ImageWidth=pixmap:ImageWidth \
 --bind image_raster:ImageHeight=pixmap:ImageHeight \
 --bind font_metrics:MeasureGlyphWidth=pixmap:MeasureGlyphWidth \
 --bind font_metrics:MeasureGlyphLineHeight=pixmap:MeasureGlyphLineHeight \
 --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
 --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
 --bind raster:RasterLine=pixmap:RasterLine \
 --bind raster_text:RasterText=pixmap:RasterText \
 --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
 --bind raster_text:RasterTextRotated=pixmap:RasterTextRotated \
 --bind paint_queue:RasterImage=pixmap:RasterImage \
 --bind paint_queue:RasterImageMesh=pixmap:RasterImageMesh
"$ziran" ir --module-path "$repo/src/ui" --module-path "$repo/src/backend" --module-path "$ziran_root/std" --root "$repo/tests" -o "$work/ir" "$source"
for input in source saved; do
 if test "$input" = source; then
  module=$source; root=$repo/tests; modules=$repo/src/ui; project=
 else
  module=$work/ir/grid_multi_test.zir; root=$work/ir; modules=$work/ir; project=
 fi
 "$ziran" bundle $project --root "$root" --module-path "$modules" --module-path "$repo/src/backend" --module-path "$ziran_root/std" "$@" --entry grid_multi_test:main -o "$work/$input.zx" "$module"
 "$ziran" run "$work/$input.zx" > "$work/$input.result"
 cat "$work/$input.result"
 test "$(tail -1 "$work/$input.result")" = 0
 for target in c go; do
  package=; if test "$target" = go; then package='--pkg main'; fi
  "$ziran" build $project --root "$root" --module-path "$modules" --module-path "$repo/src/backend" --module-path "$ziran_root/std" --target="$target" $package --exe --entry grid_multi_test:main "$@" -o "$work/$target-$input" "$module"
  if test "$target" = c; then "$work/$target-$input/grid_multi_test"
  else GO111MODULE=off go run "$work/$target-$input"/*.go; fi
 done
done
cmp "$work/source.zx" "$work/saved.zx"

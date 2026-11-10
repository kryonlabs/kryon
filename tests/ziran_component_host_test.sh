#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
mkdir -p "$repo/build/scratch/component-host"
work=$(mktemp -d "$repo/build/scratch/component-host/run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Internal tests register generated modules in the owning canonical package.
cd "$repo"

for name in counter_v1 counter_v2 status bad_schema bad_resource bad_entry bad_init bad_migrated bad_abi; do
    cd "$repo"
    source_root="$repo/tests/fixtures/component_$name"
    generated="--module-path $source_root"
    if [ "$name" = bad_abi ]; then
        project="$work/bad-abi-project"
        python3 - "$repo" "$project" <<'PYFIXTURE'
from pathlib import Path
import sys
repo, project = map(Path, sys.argv[1:])
sys.path.insert(0, str(repo / 'tests'))
from toolchain import kryon_test_project
kryon_test_project(project)
for name in ('component', 'component_protocol'):
    (project / 'src' / (name + '.zi')).write_bytes(
        (repo / 'tests/fixtures/component_bad_abi' / (name + '.zi')).read_bytes())
PYFIXTURE
        cd "$project"
        source_root="$project/src"
        generated=
    fi
    source="$source_root/component.zi"
    ir="$work/$name-ir"
    "$ziran" ir --project --offline --root "$source_root" --define KRYON_HOSTED_UI \
        $generated -o "$ir" "$source"
    "$ziran" bundle --root "$ir" --module-path "$ir" --define KRYON_HOSTED_UI \
        --entry component:Frame -o "$work/$name.zx" "$ir/component.zir"
    "$ziran" bundle --project --offline --root "$source_root" --define KRYON_HOSTED_UI \
        $generated --entry component:Frame \
        -o "$work/$name-source.zx" "$source"
    cmp "$work/$name.zx" "$work/$name-source.zx"
done

cd "$repo"

for variant in v1 v2 bad; do
    define=
    if [ "$variant" = v2 ]; then define='--define COMPONENT_INPUTS_V2'; fi
    if [ "$variant" = bad ]; then define='--define COMPONENT_INPUTS_BAD'; fi
    ir="$work/inputs-$variant-ir"
    # shellcheck disable=SC2086
    "$ziran" ir --project --offline --root "$repo/tests" --define KRYON_HOSTED_UI $define \
        --module-path "$repo/tests" -o "$ir" "$repo/tests/component_inputs_guest.zi"
    "$ziran" bundle --root "$ir" --module-path "$ir" --define KRYON_HOSTED_UI \
        --entry component_inputs_guest:Frame -o "$work/inputs_$variant.zx" "$ir/component_inputs_guest.zir"
    # shellcheck disable=SC2086
    "$ziran" bundle --project --offline --root "$repo/tests" --define KRYON_HOSTED_UI $define \
        --module-path "$repo/tests" --entry component_inputs_guest:Frame \
        -o "$work/inputs-$variant-source.zx" "$repo/tests/component_inputs_guest.zi"
    cmp "$work/inputs_$variant.zx" "$work/inputs-$variant-source.zx"
done

counter_size=$(wc -c < "$work/counter_v1.zx")
status_size=$(wc -c < "$work/status.zx")
counter_gzip=$(gzip -c "$work/counter_v1.zx" | wc -c)
status_gzip=$(gzip -c "$work/status.zx" | wc -c)
if [ "$status_size" -ge "$counter_size" ]; then
    echo "selective import did not reduce bundle size: status=$status_size counter=$counter_size" >&2
    exit 1
fi

python3 "$repo/scripts/generate-widget-codecs.py" --complete \
    --uses "$repo/src/backend/widget_codec.zi" --import '"widget_codec"' \
    --output "$work/component_test_codecs.zi"

binds="
--bind font_metrics:MeasureGlyphWidth=component_raster_host:ComponentMeasureGlyphWidth
--bind font_metrics:MeasureGlyphLineHeight=component_raster_host:ComponentMeasureGlyphLineHeight
--bind image_raster:ImageWidth=component_raster_host:ComponentImageWidth
--bind image_raster:ImageHeight=component_raster_host:ComponentImageHeight
--bind raster_text:RasterText=component_raster_host:ComponentRasterText
--bind raster_text:RasterTextClipped=component_raster_host:ComponentRasterTextClipped
--bind raster_text:RasterTextRotated=component_raster_host:ComponentRasterTextRotated
--bind raster_shape:RasterRoundedRectangle=component_raster_host:ComponentRasterRoundedRectangle
--bind raster_shape:RasterRoundedRectangleOutline=component_raster_host:ComponentRasterRoundedRectangleOutline
--bind raster:RasterLine=component_raster_host:ComponentRasterLine
--bind raster_triangles:RasterTriangles=component_raster_host:ComponentRasterTriangles
--bind system_clipboard:SystemClipboardOpenHost=component_raster_host:ComponentSystemClipboardOpenHost
--bind system_clipboard:SystemClipboardByteHost=component_raster_host:ComponentSystemClipboardByteHost
--bind system_clipboard:SystemClipboardWriteHost=component_raster_host:ComponentSystemClipboardWriteHost
--bind paint_queue:RasterImage=component_raster_host:ComponentRasterImage
--bind paint_queue:RasterImageMesh=component_raster_host:ComponentRasterImageMesh"
# shellcheck disable=SC2086
"$ziran" build --project --offline --target=c --no-main --entry component_host_test:main --root "$repo/tests" \
    --module-path "$repo/tests" --module-path "$work" $binds -o "$work/c" \
    "$repo/tests/component_host_test.zi"
"${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections \
    -I"$work/c" -I"$ziran_root/include" "$work"/c/*.c \
    "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" \
    -Wl,--gc-sections -lm -lpthread -o "$work/run"
cd "$work"
if [ -x /usr/bin/time ]; then
    timer=/usr/bin/time
else
    timer="busybox time"
fi
$timer -f 'component-host wall_s=%e max_rss_kb=%M' ./run
printf 'component-bundle counter_bytes=%s counter_gzip=%s status_bytes=%s status_gzip=%s\n' \
    "$counter_size" "$counter_gzip" "$status_size" "$status_gzip"
echo 'Portable component registry, staged replacement, rollback, state migration and raster continuity passed'

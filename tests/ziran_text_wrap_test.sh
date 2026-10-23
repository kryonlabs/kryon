#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_wrap_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_text_wrap_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_text_wrap_host:MeasureGlyphLineHeight

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"

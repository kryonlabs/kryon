#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_wrap_test.zi

set -- \
    --bind font_metrics:MeasureGlyphWidth=ziran_text_wrap_host:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=ziran_text_wrap_host:MeasureGlyphLineHeight

widget_std="$ziran_root/std"
. "$repo/tests/widget_matrix.sh"

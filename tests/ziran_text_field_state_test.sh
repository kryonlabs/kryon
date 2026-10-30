#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_text_field_state_test.zi
widget_std=$ziran_root/std
widget_native=1
widget_run_saved=1
. "$repo/tests/widget_matrix.sh"

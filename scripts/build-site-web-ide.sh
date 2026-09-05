#!/bin/sh
set -eu

site_dir=${1:-build/site}
tool_dir="$site_dir/ide-tools"
EMCC=${EMCC:-emcc}

mkdir -p "$tool_dir"

if ! command -v "$EMCC" >/dev/null 2>&1; then
    printf '%s\n' '{"error":"emcc not found; web IDE compiler modules were not built"}' > "$tool_dir/unavailable.json"
    exit 0
fi

build_failed()
{
    printf '%s\n' '{"error":"emcc failed; web IDE compiler modules were not built"}' > "$tool_dir/unavailable.json"
    rm -f "$tool_dir/k2kir.js" "$tool_dir/k2kir.wasm" \
        "$tool_dir/k2b.js" "$tool_dir/k2b.wasm" \
        "$tool_dir/k2c.js" "$tool_dir/k2c.wasm" \
        "$tool_dir/k2go.js" "$tool_dir/k2go.wasm" \
        "$tool_dir/k2js.js" "$tool_dir/k2js.wasm" \
        "$tool_dir/krb-web.js" "$tool_dir/krb-web.wasm"
    exit 0
}

"$EMCC" -O0 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createK2irModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sSTACK_SIZE=5242880 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -Icmd/kir \
    cmd/k2kir/main.c cmd/kir/kir.c cmd/kir/kir_parse.c cmd/kir/kir_text.c \
    cmd/kir/kir_token.c cmd/kir/kir_expr.c cmd/kir/kir_cleanup.c cmd/kir/kir_check.c cmd/kir/kir_emit.c \
    -o "$tool_dir/k2kir.js" || build_failed

"$EMCC" -O0 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createK2bModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sSTACK_SIZE=5242880 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -Iinclude -Icmd/k2b -Icmd/kir \
    cmd/k2b/*.c \
    cmd/kir/kir.c cmd/kir/kir_parse.c cmd/kir/kir_text.c \
    cmd/kir/kir_token.c cmd/kir/kir_expr.c cmd/kir/kir_cleanup.c cmd/kir/kir_check.c cmd/kir/kir_emit.c \
    -o "$tool_dir/k2b.js" || build_failed

"$EMCC" -O0 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createK2cModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sSTACK_SIZE=5242880 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -Icmd/k2c -Icmd/kir \
    cmd/k2c/*.c \
    cmd/kir/kir.c cmd/kir/kir_parse.c cmd/kir/kir_text.c \
    cmd/kir/kir_token.c cmd/kir/kir_expr.c cmd/kir/kir_cleanup.c cmd/kir/kir_check.c cmd/kir/kir_emit.c \
    -o "$tool_dir/k2c.js" || build_failed

"$EMCC" -O0 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createK2gModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sSTACK_SIZE=5242880 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -Icmd/k2go -Icmd/kir \
    cmd/k2go/*.c \
    cmd/kir/kir.c cmd/kir/kir_parse.c cmd/kir/kir_text.c \
    cmd/kir/kir_token.c cmd/kir/kir_expr.c cmd/kir/kir_cleanup.c cmd/kir/kir_check.c cmd/kir/kir_emit.c \
    -o "$tool_dir/k2go.js" || build_failed

"$EMCC" -O0 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createK2jsModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sSTACK_SIZE=5242880 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -Icmd/k2js -Icmd/kir \
    cmd/k2js/*.c \
    cmd/kir/kir.c cmd/kir/kir_parse.c cmd/kir/kir_text.c \
    cmd/kir/kir_token.c cmd/kir/kir_expr.c cmd/kir/kir_cleanup.c cmd/kir/kir_check.c cmd/kir/kir_emit.c \
    -o "$tool_dir/k2js.js" || build_failed

"$EMCC" -Wall -Wextra -Os -Iinclude \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createKrbWebModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sSTACK_SIZE=5242880 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_FUNCTIONS=_krb_web_mouse,_krb_web_button,_krb_web_wheel,_krb_web_text,_krb_web_start,_main \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -o "$tool_dir/krb-web.js" \
    cmd/krb-web/main.c \
    src/krb/krb.c src/krb/krb_caps.c src/backend/kry_backend.c \
    src/backend/kry_sw.c src/backend/kry_sw_png.c src/backend/kry_backend_rec.c \
    || build_failed

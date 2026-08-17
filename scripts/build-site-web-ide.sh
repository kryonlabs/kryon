#!/bin/sh
set -eu

site_dir=${1:-build/site}
tool_dir="$site_dir/ide-tools"

mkdir -p "$tool_dir"

if ! command -v emcc >/dev/null 2>&1; then
    printf '%s\n' '{"error":"emcc not found; web IDE compiler modules were not built"}' > "$tool_dir/unavailable.json"
    exit 0
fi

build_failed()
{
    printf '%s\n' '{"error":"emcc failed; web IDE compiler modules were not built"}' > "$tool_dir/unavailable.json"
    rm -f "$tool_dir/k2ir.js" "$tool_dir/k2ir.wasm" "$tool_dir/k2b.js" "$tool_dir/k2b.wasm"
    exit 0
}

emcc -O0 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createK2irModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -Icmd/kir \
    cmd/k2ir/main.c cmd/kir/kir.c cmd/kir/kir_parse.c \
    -o "$tool_dir/k2ir.js" || build_failed

emcc -O0 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=createK2bModule \
    -sINVOKE_RUN=0 \
    -sEXIT_RUNTIME=0 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -sEXPORTED_RUNTIME_METHODS="['FS','callMain']" \
    -Iinclude -Icmd/k2b -Icmd/kir \
    cmd/k2b/main.c cmd/k2b/k2b_parse.c cmd/k2b/k2b_util.c cmd/k2b/k2b_krb.c \
    cmd/kir/kir.c cmd/kir/kir_parse.c \
    -o "$tool_dir/k2b.js" || build_failed

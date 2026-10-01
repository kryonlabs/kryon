#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$root/tests/toolchain.sh"
bin=${1:-"$(dirname "$ziran")"}
emcc=${2:-"$HOME/emsdk/upstream/emscripten/emcc"}
work=$(mktemp -d "$root/build/canvas-text-os-wasm.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
run_compiler() {
    if [ -n "${ZIRAN_RUNNER:-}" ]; then
        # Optional shell script used to serialize compiler work in shared builds.
        sh "$ZIRAN_RUNNER" "$@"
    else
        "$@"
    fi
}
run_compiler "$bin/zi2c" --no-main --define PLATFORM_WEB --root "$root" \
    --module-path "$root/tests/page_texture_fake" --module-path "$root/src/backend" --module-path "$root/src/ui" --module-path "$ziran_root/std" \
    -o "$work/c" "$root/tests/canvas_text_os_behavior.zi"
cat > "$work/fixture.js" <<'JS'
const loadedFaces = new Set();
globalThis.FontFace = class { async load() { return this; } };
const glyphContext = {
  // A 16px line with this face's 12 + 3 ascent and descent is drawn at
  // 16 * 16 / 15 CSS pixels.
  set font(value) {
    if (!/^(16\.00|17\.07)px /.test(value)) throw new Error(`unexpected glyph font ${value}`);
  },
  measureText: () => ({width: 6, actualBoundingBoxLeft: 1, actualBoundingBoxRight: 6,
    actualBoundingBoxAscent: 12, actualBoundingBoxDescent: 3,
    fontBoundingBoxAscent: 12, fontBoundingBoxDescent: 3}),
  clearRect: () => {}, fillText: () => {},
  getImageData: (_x, _y, width, height) => ({data: new Uint8ClampedArray(width * height * 4).fill(255)})
};
globalThis.document = {
  fonts: {add: face => loadedFaces.add(face), delete: face => loadedFaces.delete(face)},
  createElement: () => ({width: 64, height: 64, getContext: () => glyphContext})
};
globalThis.__testDrop = ['drop-a.txt', 'drop-b.txt'].map(name => ({
  name, arrayBuffer: () => Promise.resolve(new ArrayBuffer(3))}));
Module.printErr = text => console.error(text);
globalThis.__canvasTestFaces = loadedFaces;
JS
find "$work/c" -type f -name '*.c' -exec \
    "$emcc" -O1 -I"$ziran_root/include" -I"$work/c" -I"$work/c/tests" \
    --js-library "$ziran_root/web/ziran_web.js" -sEXPORTED_RUNTIME_METHODS=FS \
    --pre-js "$work/fixture.js" \
    -sASYNCIFY -sEXIT_RUNTIME=1 -sENVIRONMENT=node -sWASM_ASYNC_COMPILATION=0 \
    -o "$work/test.js" {} +
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY node "$work/test.js"
echo "Canvas text, font lifetime, filesystem, callbacks and varargs Wasm test passed"

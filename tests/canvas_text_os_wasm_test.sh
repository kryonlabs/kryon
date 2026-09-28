#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=${1:-"$root/../ziran/build/bin"}
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
    --module-path "$root/src/ui" --module-path "$root/../ziran/std" \
    -o "$work/c" "$root/tests/canvas_text_os_behavior.zi"
cat > "$work/fixture.js" <<'JS'
const loadedFaces = new Set();
globalThis.FontFace = class { async load() { return this; } };
const glyphContext = {
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
globalThis.__kryonCanvas = { dropped: ['/tmp/drop-a.txt', '/tmp/drop-b.txt'], clipboard: '' };
Module.printErr = text => console.error(text);
globalThis.__canvasTestFaces = loadedFaces;
JS
cat > "$work/effects.js" <<'JS'
addToLibrary({
  js_canvas_now: () => 0,
  $CanvasTestTextures: { next: 1, live: null },
  js_texture_from_rgba__deps: ['$CanvasTestTextures'],
  js_texture_from_rgba: (pointer, width, height) => {
    if (width !== 512 || height < 64 || HEAPU8[pointer] !== 0 ||
        HEAPU8[pointer + (width + 1) * 4 + 3] !== 255) throw new Error('atlas pixels');
    if (!CanvasTestTextures.live) CanvasTestTextures.live = new Set();
    const id = CanvasTestTextures.next++;
    CanvasTestTextures.live.add(id);
    return id;
  },
  js_texture_free__deps: ['$CanvasTestTextures'],
  js_texture_free: id => { CanvasTestTextures.live.delete(id); },
  test_canvas_host_finished__deps: ['$CanvasTestTextures'],
  test_canvas_host_finished: () => {
    if (CanvasTestTextures.live.size || globalThis.__canvasTestFaces.size) return 20;
    return 0;
  }
});
JS
find "$work/c" -type f -name '*.c' -exec \
    "$emcc" -O1 -I"$root/../ziran/include" -I"$work/c" -I"$work/c/tests" \
    --js-library "$root/web/canvas_text.js" --js-library "$root/web/canvas_os.js" \
    --js-library "$work/effects.js" --pre-js "$work/fixture.js" \
    -sASYNCIFY -sEXIT_RUNTIME=1 -sENVIRONMENT=node -sWASM_ASYNC_COMPILATION=0 \
    -o "$work/test.js" {} +
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u GDK_DISPLAY node "$work/test.js"
echo "Canvas text, font lifetime, filesystem, callbacks and varargs Wasm test passed"

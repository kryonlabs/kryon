# Headless frame capture and replay

`EndFrameCapture(session)` is an opt-in alternative to `EndFrame(session)`. It commits a valid
tree, reports each committed node and queued paint command through explicit
`FrameNode` and `FramePaint` values through declared host capabilities, then
emits the paint effects. A rejected tree or
paint overflow produces no capture or raster effects. Ordinary `EndFrame()`
does not require capture bindings.

`tools/frame-replay.sh` runs a portable `.zib` entry once per line of a pointer
trace. The entry must take no arguments, return an integer, call a module-local
`PollPointer() -> PointerFrame` exactly once, and call `EndFrameCapture(session)` once.
The script binds fixed font measurements, a headless raster sink, and the four
capture capabilities. It removes `DISPLAY` and `WAYLAND_DISPLAY` from the
runner environment. It writes JSON with each frame's input, committed tree,
paint commands, result, and raster effect count.

To run the included button example after `make`:

```sh
ziran=../ziran/build/bin/ziran
mkdir -p build/ziran/frame-capture-test
"$ziran" bundle --root tests/fixtures --module-path src/ui \
    --entry frame_replay:Frame \
    -o build/ziran/frame-capture-test/source.zib \
    tests/fixtures/frame_replay.zi
tools/frame-replay.sh build/ziran/frame-capture-test/source.zib \
    frame_replay tests/fixtures/frame_replay.trace \
    build/ziran/frame-capture-test/source.json
```

Trace lines contain `x y down pressed released`, with Boolean fields written
as `0` or `1`. Blank lines and `#` comments are allowed. The included trace
moves a button through idle, press, release, and clicked frames. Run
`tests/ziran_frame_capture_test.sh` to compare source and saved-IR bundles and
check the recorded behavior.

Run the same trace against a proposed bundle and compare the two JSON files
to see changes in widget behavior, semantics, geometry, or paint decisions.

This records UI behavior under the supplied inputs and fixed host measurements.
It does not persist application state across process restarts or decide whether
a candidate UI should be adopted. Those decisions belong to the application.

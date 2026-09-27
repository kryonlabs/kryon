# Web host status

Kryon's current web host is the Canvas2D backend in `src/backend/`.
Applications compile Ziran to C, then Emscripten links that generated C with
small browser effect libraries. The Ziran host implementation remains in
`.zi`; JavaScript supplies only browser effects that Ziran foreign imports
declare. See [BACKENDS.md](BACKENDS.md) and
[Canvas2D browser host](canvas.md).

The semantic DOM profile adds a real browser document layer over that
Canvas raster host; see [Semantic DOM browser host](dom.md). Both browser
profiles generate from the selected host plus the Canvas raster adapter and
without entry pruning. This is not a revival of the removed `.kry` compiler.

Kryon has no separate JavaScript widget runtime or source-language compiler.
The files above are the current web support documentation.

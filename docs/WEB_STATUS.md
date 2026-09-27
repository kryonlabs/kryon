# Web host status

Kryon's current web host is the Canvas2D backend in `src/backend/`.
Applications compile Ziran to C, then Emscripten links that generated C with
small browser effect libraries. The Ziran host implementation remains in
`.zi`; JavaScript supplies only browser effects that Ziran foreign imports
declare. See [BACKENDS.md](BACKENDS.md) and
[Canvas2D browser host](canvas.md).

The former `.kry` to JavaScript compiler and handwritten JavaScript widget
runtime remain removed. They are not compatibility paths. Historical plans and
test ledgers may still describe that old experiment; the files above are the
current web support documentation.

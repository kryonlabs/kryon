# Ziran example

`hello.zi` is a small Ziran application that imports Kryon's ordinary
`Button` widget module. It builds a retained tree, paints the button, and
returns its node count. `hello_host.c` provides raw font measurements and SVG
raster effects through the public portable host bindings. No window or display
server is opened.

From the Kryon repository root, with Ziran checked out beside it:

```sh
make -C examples
make -C examples test
```

The first command builds `build/examples/ziran/hello.zib`, runs it, and writes
`build/examples/ziran/hello.svg`. Open the SVG in an image viewer to see the
result. The test also bundles the saved `.zir`, verifies that both `.zib`
files match, runs both, compares their SVG output, and checks that the widget
produced a shape and its label.

This example shows the intended application boundary: Ziran bundles only the
modules the application imports; Kryon supplies widget decisions; a separate
host supplies font and drawing effects. Native app input loops, platform text
services, and the remaining widget migration are still in progress.

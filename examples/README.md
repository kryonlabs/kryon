# Ziran example

`hello.zi` is a small Ziran application that imports Kryon's ordinary
`Button` and pointer modules. It builds a retained tree, handles a press and
release, and changes the button label after the click. `hello_host.c` supplies
four raw pointer samples through Kryon's `PointerBinding`, plus font
measurements and SVG raster effects through
declared host bindings. No window or display server is opened.

From the Kryon repository root, with Ziran checked out beside it:

```sh
make -C examples
make -C examples test
```

The first command builds `build/examples/ziran/hello.zib`, runs one persistent
bundle instance across four frames, and writes
`build/examples/ziran/hello.svg`. Open the SVG in an image viewer to see the
idle, pressed, released, and clicked frames side by side. The test also bundles
the saved `.zir`, verifies that both `.zib` files match, runs both, compares
their SVG output, and checks that exactly one click changed the label.

This example shows the intended application boundary: Ziran bundles only the
modules the application imports; Kryon supplies widget decisions; a separate
host supplies raw input, font, and drawing effects. Production window loops,
platform text services, and the remaining widget migration are still in
progress.

An optional SDL2/Cairo desktop host runs the same bundle in a real window.
Build it with `make -C examples desktop`. Its repeatable four-frame check runs
on a private display with `make -C examples desktop-test` and writes
`build/examples/ziran/hello-desktop.png`. The check captures the clicked state
without contacting the current desktop display.
`make -C examples desktop-image-test` renders the canonical
`Image(ImageProps)` with a path-backed PNG and red tint, then writes
`build/examples/ziran/image-desktop.png`. This host currently handles shapes,
text, and path-backed PNG images. Texture handles and other image formats
remain open. It is a desktop integration proof, not the finished platform
renderer.

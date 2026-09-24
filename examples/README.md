# Ziran example

`hello.zi` is a small Ziran application that imports Kryon's ordinary
`Button` and pointer modules. It builds a retained tree, handles a press and
release, and changes the button label after the click. `hello_host.zi` supplies
four raw pointer samples, font measurements, and SVG drawing effects. The
application and host are both Ziran source. No window or display server is
opened.

From the Kryon repository root, with Ziran checked out beside it:

```sh
make -C examples
make -C examples test
```

The first command builds `build/examples/ziran/hello.zib`, generates and links
a native program from the two `.zi` sources, and writes
`build/examples/ziran/hello.svg`. Open the SVG in an image viewer to see the
idle, pressed, released, and clicked frames side by side. The test also bundles
the saved `.zir`, verifies that both `.zib` files match, and checks the native
host's four-frame click result and SVG content. The portable bundle is built
and compared here; its execution is covered by the separate portable runtime
tests.

This example shows the intended application boundary: Ziran compiles only the
modules the application imports; Kryon supplies widget decisions; a separate
Ziran host supplies raw input, font, and drawing effects. Production window
loops, platform text services, and the remaining widget migration are still in
progress.

An optional Ziran desktop host links the application with SDL2 and Cairo
through declared foreign calls. Build it with `make -C examples desktop`.
Run `build/examples/ziran/hello-desktop` from `examples/` to interact with its
button. Its repeatable four-frame check runs on a private display with
`make -C examples desktop-test` and writes
`build/examples/ziran/hello-desktop.png` without contacting the current
desktop display. `make -C examples desktop-image-test` renders the canonical
`Image(ImageProps)` with a path-backed PNG and red tint, then writes
`build/examples/ziran/image-desktop.png`. The host imports Ziran's generic
`c_string` module for safe C string arguments. It handles shapes, text, and
path-backed PNG images. Texture handles and other image formats remain open.

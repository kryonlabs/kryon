# Kryon examples

## Widget gallery

The maintained gallery is the ordinary Ziran package in `gallery/`. Its
`src/scenes.zi` supplies the same core scenes to the native application,
Canvas/Wasm build, and widget review tool. Each optional package has its own
integration (`src/plot_examples.zi`, `src/data_views_examples.zi`). The module
list in `modules.toml` registers only dependencies declared in `ziran.toml`;
an omitted dependency contributes no imports, renderer or browser compiler
sources. Core UI and 2D drawing are separate categories. TableView renders
real headers and cells, with selection, sorting, column resizing and scrolling.

The build generates `build/catalog/catalog.zi` and the website's module data
and editable sources from those maintained Ziran compositions. Use the Make
targets below so registration is prepared before either host compiles. The
website integrates each example's source with the existing live editor:
edits compile in the browser and run in the same Canvas preview host.

From the Kryon root:

```sh
make -C examples widget-catalog      # desktop
make -C examples widget-catalog-web  # Canvas/Wasm
make gallery-zx                    # one complete portable ZX per example
make site-gallery                   # stage the gallery for the existing site
make site                           # rebuild the gallery, editor and initial demo
```

Run `./build/gallery-desktop` from `examples/gallery/`. The web output is
`examples/gallery/build/gallery-web.html`; the website entry is
`docs/site/widgets.html`. Emscripten is required for the web profile. Package
and toolchain commits are pinned in `gallery/ziran.lock`. Canonical local
sources can be selected in its ignored `ziran.local.toml`.

`build/gallery-zx/index.json` lists the exported bundles. Each includes the
example's behavior and assets and opens in the generic desktop, browser or
Android ZX player. The gallery's `portable` profile also builds the complete
catalog as one ZX. See [ZX applications](../docs/PROJECTS.md#zx-applications)
for player and platform build commands. The website build stages a download
and a browser-player link for every example; a local build does not publish it.

The desktop host supports `KRYON_CATALOG_SCENE` and `KRYON_CAPTURE_PATH` for
repeatable captures. Scrub the inherited display and session environment and
use a private `xvfb-run -a` display for all captures and browser checks.

## Small host probes

The files below exercise custom host contracts and native/portable parity.
Their small probe bundles use those test hosts. Use the gallery exports above
for complete applications that open in the generic ZX players.

`hello.zi` is a small Ziran application that imports Kryon's ordinary
`Button` and pointer modules. It builds a retained tree, handles a press and
release, and changes the button label after the click. `hello_host.zi` supplies
four raw pointer samples, font measurements, and SVG drawing effects. The
application and host are both Ziran source. No window or display server is
opened.

From the Kryon repository root, with the `ziran` command installed:

```sh
make -C examples
make -C examples test
```

The first command builds `build/examples/hello.zx`, generates and links
a native program from the two `.zi` sources, and writes
`build/examples/hello.svg`. Open the SVG in an image viewer to see the
idle, pressed, released, and clicked frames side by side. The test also bundles
the saved `.zir`, verifies that both `.zx` files match, and checks the native
host's four-frame click result and SVG content. The portable bundle is built
and compared here; its execution is covered by the separate portable runtime
tests.

This example shows the intended application boundary: Ziran compiles only the
modules the application imports; Kryon supplies widget decisions; a separate
Ziran host supplies raw input, font, and drawing effects. Applications with a
window use one of the hosts in [BACKENDS.md](../docs/BACKENDS.md) instead.

An optional Ziran desktop host links the application with SDL2 and Cairo
through declared foreign calls. Build it with `make -C examples desktop`.
Run `build/examples/hello-desktop` from `examples/` to interact with its
button. Its repeatable four-frame check runs on a private display with
`make -C examples desktop-test` and writes
`build/examples/hello-desktop.png` without contacting the current
desktop display. `make -C examples desktop-image-test` renders the canonical
`Image(ImageProps)` with a path-backed PNG and red tint, then writes
`build/examples/image-desktop.png`. The host imports Ziran's generic
`c_string` module for safe C string arguments. It handles shapes, text, and
path-backed PNG images. Texture handles and other image formats remain open.

`make -C examples desktop-text-test` renders the plain `Text(TextProps)`
example under a private display and saves
`build/examples/text-desktop.png`. It also checks that source and saved
`.zir` produce the same portable `text.zx`. Run
`../build/examples/text-desktop` from `examples/` to open the interactive
window, or open `examples/text_hello.zi` in an editor to see its Ziran source.

`make -C examples time-picker` builds `time_picker_demo.zi`, Kryon's
`TimePicker` without a time, with one, disabled and in a small form, with a
Light and Dark switch. Run `../build/examples/time-picker-desktop` from
`examples/` to try it with the mouse, touch and keyboard.
`make -C examples time-picker-shots` plays scripted scenes on a private
display and saves them in `build/time-picker-shots/`.

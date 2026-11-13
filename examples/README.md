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

## Hello world

`hello.zi` is the complete example: one program with an explicit `main`, one
desktop window, and one text label that says `Hello, world!`. It uses named,
package-qualified imports so the application boundary stays visible:

```zi
App  :: #import "kryon/app";
Text :: #import "kryon/text";

main :: () -> s32 {
    window := App.Window.{ .title = "Hello", .width = 320, .height = 160 }
    return App.Run(window, Draw)
}

Draw :: (frame: App.Frame) {
    Text.Draw(frame, Text.Props.{ .text = "Hello, world!", .size = 32 })
}
```

From the Kryon repository root, with the `ziran` command installed:

```sh
make -C examples
make -C examples test
```

Both commands build the native executables at `build/examples/hello` and
`build/examples/window`. Run either from `examples/` to open it. `window.zi`
is the smallest empty-window program. Neither example contains an image,
button, pointer, capture, or test-driver code. Button behavior remains covered
by the dedicated button examples and widget regression tests. The `test`
target also compiles both programs for Go, C/Web, and portable ZX.

`make -C examples desktop-image-test` renders the canonical
`Image.Draw(Image.Props)` with a path-backed PNG and red tint, then writes
`build/examples/image-desktop.png`. The host imports Ziran's generic
`c_string` module for safe C string arguments. It handles shapes, text, and
path-backed PNG images. Texture handles and other image formats remain open.

`make -C examples time-picker` builds `time_picker_demo.zi`, Kryon's
`TimePicker` without a time, with one, disabled and in a small form, with a
Light and Dark switch. Run `../build/examples/time-picker-desktop` from
`examples/` to try it with the mouse, touch and keyboard.
`make -C examples time-picker-shots` plays scripted scenes on a private
display and saves them in `build/time-picker-shots/`.

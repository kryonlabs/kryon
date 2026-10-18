# Kryon examples

## Interactive widget catalog

Build the catalog from the Kryon repository root, then run it from `examples/`:

```sh
make -C examples widget-catalog
cd examples
../build/examples/widget-catalog-desktop
```

`widget_catalog.zi` has 54 previews of the current core widget surface:
editable text, buttons, selection controls, lists, navigation, menus, dialogs,
time and color pickers, images, shapes, and layout containers. Choose a preview
from the scrolling list. Controls retain their values; the check and switch
below the preview show disabled controls and a dark palette. Plot charts and
DataViews tables and trees belong to their separate packages.

`desktop_widget_catalog.zi` runs the example with the SDL/Cairo host. For a
repeatable capture, choose a zero-based `KRYON_CATALOG_SCENE` from 0 to 53
and set `KRYON_CAPTURE_PATH`. Always scrub `DISPLAY`, `WAYLAND_DISPLAY`,
`XAUTHORITY`, and `DBUS_SESSION_BUS_ADDRESS` before running captures on a
private `xvfb-run -a` display.

## Small examples

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

The first command builds `build/examples/hello.zib`, generates and links
a native program from the two `.zi` sources, and writes
`build/examples/hello.svg`. Open the SVG in an image viewer to see the
idle, pressed, released, and clicked frames side by side. The test also bundles
the saved `.zir`, verifies that both `.zib` files match, and checks the native
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
`.zir` produce the same portable `text.zib`. Run
`../build/examples/text-desktop` from `examples/` to open the interactive
window, or open `examples/text_hello.zi` in an editor to see its Ziran source.

`make -C examples time-picker` builds `time_picker_demo.zi`, Kryon's
`TimePicker` without a time, with one, disabled and in a small form, with a
Light and Dark switch. Run `../build/examples/time-picker-desktop` from
`examples/` to try it with the mouse, touch and keyboard.
`make -C examples time-picker-shots` plays scripted scenes on a private
display and saves them in `build/time-picker-shots/`.

# Kryon examples

## Widget gallery

The maintained gallery is the ordinary package in `examples/gallery/`. Its
screen composition is authored in TSX: the core scenes are the maintained
widget examples (`examples/<widget>/<widget>.tsx`, sharing composition in
`examples/shared/screens/`), and optional packages publish their scenes as
exported TSX examples listed in `modules.toml`. Shared scene state and scene
names stay in Ziran (`src/scenes.zi`, `src/plot_examples.zi`,
`src/data_views_examples.zi`), so the native application, Canvas/Wasm build and
widget review tool agree on one state. The module list registers only
dependencies declared in `ziran.toml`; an omitted dependency contributes no
imports, renderer or browser compiler sources. Core UI and 2D drawing are
separate categories. TableView renders real headers and cells, with selection,
sorting, column resizing and scrolling.

The build generates `build/catalog/catalog.zi`, which imports and dispatches
those maintained TSX scenes, plus the website's module data and editable
sources. Use the Make targets below so registration is prepared before either
host compiles. The website integrates each example's TSX source with the
existing live editor: edits compile in the browser and run in the same Canvas
preview host.

From the Kryon root:

```sh
make -C examples widget-catalog      # desktop gallery binary
make -C examples widget-catalog-web  # gallery build and website screenshots
make gallery-zx                    # one complete portable ZX per example
make site-gallery                   # stage the gallery for the existing site
make site                           # package the website, gallery and editor
```

Run `build/examples/gallery/gallery-desktop` from the Kryon root. `make site`
builds the Canvas/Wasm gallery at `build/examples/gallery/gallery-web.html` and
packages the website around it; the entry page is `docs/site/widgets.html`.
Emscripten is required for the web profile. Package and toolchain commits are
pinned in `examples/gallery/ziran.lock`. Canonical local sources can be
selected in its ignored `ziran.local.toml`.

`build/gallery-zx/index.json` lists the exported bundles. Each includes the
example's behavior and assets and opens in the generic desktop, browser or
Android ZX player. The gallery's `portable` profile also builds the complete
catalog as one ZX. See [ZX applications](../docs/PROJECTS.md#zx-applications)
for player and platform build commands. The website runs every example live
on its Components page instead of publishing these bundles.

The site's Apps page lists each Kryon Labs app from
`examples/gallery/src/site_apps.zi`: a web app opens its published browser
build, a desktop app carries a `desktop_reason`. When an app gains a browser
build, add its `SiteWebApp` entry and card image in
`examples/gallery/assets/apps/`, then move it from `DESKTOP_APPS` to
`WEB_APPS` in `tests/site_browser_test.py`.

The desktop host supports `KRYON_CATALOG_SCENE` and `KRYON_CAPTURE_PATH` for
repeatable captures. Scrub the inherited display and session environment and
use a private `xvfb-run -a` display for all captures and browser checks.

## Hello world

`hello.tsx` is the complete example: one program with an explicit `main`, one
desktop window, and one text label that says `Hello, world!`. It uses named,
package-qualified imports so the application boundary stays visible:

```tsx
import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Text } from "kryon/tsx/text";

function Draw(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Text session={session} props={{text: "Hello, world!", size: s32(32)}}/>;
}

export function main(): s32 {
    return App.Run({title: "Hello", width: s32(320), height: s32(160)}, Draw);
}
```

From the Kryon repository root, with the `ziran` command installed:

```sh
make -C examples
make -C examples test
```

Both commands build the native executables at `build/examples/hello` and
`build/examples/window`. Run either from `examples/` to open it. `window.tsx`
is the smallest empty-window program. Neither example contains an image,
button, pointer, capture, or test-driver code. Button behavior remains covered
by the dedicated button examples and widget regression tests. The `test`
target also compiles both programs for Go, C/Web, and portable ZX.

`make -C examples ../build/examples/motion_chart` builds the animated revenue
chart from `motion_chart.tsx`. Its native helper retains the plotting and
tween logic. Run `build/examples/motion_chart` from the repository root and click
**Switch week**. One `kryon/motion` tween drives the full series. Clicking again
mid-animation reverses smoothly from the currently displayed values.

`make -C examples desktop-image-test` renders the canonical
typed `Image` TSX widget in `demo/image_demo.tsx` with a path-backed PNG and red tint, then writes
`build/examples/image-desktop.png`. The host imports Ziran's generic
`c_string` module for safe C string arguments. It handles shapes, text, and
path-backed PNG images. See [image host support](../docs/BACKENDS.md#webp-images) for image formats.

`make -C examples time-picker` builds `demo/time_picker_demo.tsx`, Kryon's
`TimePicker` without a time, with one, disabled and in a small form, with a
Light and Dark switch. Run `../build/examples/time-picker-desktop` from
`examples/` to try it with the mouse, touch and keyboard.
`make -C examples time-picker-shots` plays scripted scenes on a private
display and saves them in `build/time-picker-shots/`.
The root `desktop_image.zi` and `desktop_time_picker.zi` programs are native
host, capture and scripted input harnesses for these TSX demos. State, text
formatting and platform callbacks live under `native/`; the Makefile adds
that directory to the ordinary project module search path. `zx_libraries.zi`
remains the low-level static/dynamic library ABI fixture.

`make -C examples ../build/examples/loading_demo` builds the TSX loading
preview. Its native host accepts width, height, dark mode and measured
progress as numeric arguments, for example `390 780 1 1` on a private display.
The hosted portable counter and text editor are authored in
`hosted_surface.tsx`; use the [hosted surface build command](../docs/BACKENDS.md#hosted-zx-software-surface)
and the existing `make zx-surface-test` smoke target for its presentation,
input, resize and clock variants.

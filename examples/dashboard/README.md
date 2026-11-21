# Dashboard Starter

A reusable, editable native Kryon dashboard starter demonstrating:

- Responsive wide/narrow layout (sidebar rail on wide viewports, compact navigation strip on narrow viewports)
- Live CSS styling via `kryonlabs/css` (`:root` custom properties, `hwb()` colors, `%` opacity, `min-block-size`, `@media (prefers-color-scheme: dark)`, `@media (-kryon-density: compact)`, `#id`, `.class`, `:hover`, `:selected`, `:focus`)
- Searchable results (`TextField.Bind` + filter chips + scrollable rows + detail inspector)
- Explicit loading (`LoadingIndicator`), empty (`EmptyState`), and error (`Alert` + retry) states
- Keyboard-operable settings modal (`Modal.Draw`, `Toggle`, `Checkbox`, `TextField.Bind`, `Tab` / `Shift+Tab`, `Enter`, `Escape`)

## Quick start

Run these commands from `examples/dashboard/` with `ziran` and `kryon` on your path.

```sh
ziran fetch --locked
mkdir -p build/previews
kryon preview default --theme light --width 1120 --height 720 --output build/previews/default
```

Open `build/previews/default.png` for the rendered dashboard and `build/previews/default.json` for its widget tree. The checked-in lockfile pins the Ziran toolchain and gives the app and its stylesheet the same Kryon package. When updating CSS or Kryon, match the dashboard’s Kryon revision to the one declared by CSS before regenerating the lock.

## Develop against local packages

Create a `ziran.local.toml` in `examples/dashboard/` to exercise changes in the canonical working directories.

```toml
[overrides]
kryon = "../.."
ziran = "../../../../ziranlang/ziran"
css = "../../../packages/css"
image_codec = "../../../../ziranlang/packages/image-codec"
avif = "../../../../ziranlang/packages/avif"
webp = "../../../../ziranlang/packages/webp"
```

Run `ziran pkg paths kryon css ziran` to confirm the local sources. Use ordinary project commands during local development to leave the shared lockfile intact.

## Typecheck, build, and test

From `examples/dashboard/`:

```sh
for f in src/model.zi src/styles.zi src/dashboard.zi src/app.zi src/desktop_host.zi previews/*.zi tests/dashboard_test.zi; do
  ziran check --project "$f" || exit 1
done

KRYON_TEST_DRIVER=1 ../../build/bin/kryon build
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER \
  ziran run --project --target=c tests/dashboard_test.zi
```

## Named preview fixtures

Each fixture in `previews/` exports `Frame` for `kryon preview`.

Pass comma-separated widths to build the fixture once and capture each
viewport. This writes `default-light-460.png`, `default-light-768.png`, and
`default-light-1120.png`, each with a JSON widget tree of the same name.

```sh
mkdir -p build/previews
kryon preview default --theme light --width 460,768,1120 --height 760 --output build/previews/default-light
```

```sh
mkdir -p build/previews
kryon preview default  --theme light --width 1120 --height 720 --output build/previews/default-light-wide
kryon preview default  --theme dark  --width 460  --height 760 --output build/previews/default-dark-narrow
kryon preview loading  --theme light --width 1120 --height 720 --output build/previews/loading
kryon preview empty    --theme light --width 1120 --height 720 --output build/previews/empty
kryon preview error    --theme light --width 1120 --height 720 --output build/previews/error
kryon preview settings --theme dark  --width 1120 --height 720 --output build/previews/settings-dark
```

## CSS viewport grids and portable ZX

[`previews/responsive.zi`](previews/responsive.zi) uses CSS for the KPI grid,
chart panel tracks, padding, colors and headline size. The metrics flow into
four columns at 1200 pixels and two at 550. The first chart panel spans the
row, and the remaining two stack below 820 pixels. The trade ledger keeps a
shared column grid inside a horizontal scroll viewport. Chart slots are
ready for Plot, with native API equivalents in the
[HTML/CSS mapping](../../docs/HTML_TO_KRYON.md#author-a-responsive-dashboard).

Use the checkout overrides above or update both Kryon and CSS in your
project to the matching revisions from CSS's manifest and run `ziran lock`.
From the Kryon root, generate native snapshots, compile the same `.zi` frame
into ZX, and exercise the bundle on private Xvfb:

```sh
make -j1 build/bin/kryon zx-player
python3 tests/responsive_dashboard_test.py
```

The fixture writes two PNGs, two widget trees and `responsive.zx` under
`build/scratch/dashboard-responsive/`. The test checks measured placement,
clipping, colors, typography, and button input before and after a resize.
For an app, make the dashboard frame your entry and run
`kryon build --profile portable` to produce its `.zx` bundle.

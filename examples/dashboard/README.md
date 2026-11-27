# Dashboard Starter

A reusable, editable Kryon dashboard starter authored in TSX. Native domain
state and edit buffers live in `src/dashboard_state.zi`; the model stays in
`src/model.zi`. The frame
([`src/dashboard.tsx`](src/dashboard.tsx)) only says what goes where: layout
containers place and size every child, and the stylesheet
([`src/styles.zi`](src/styles.zi)) sets the sidebar width, the KPI and
inspector cards, gaps, padding, fonts and the 760 px breakpoint. There is no
rectangle arithmetic and no computed key in the frame. It demonstrates:

- Responsive wide/narrow layout (sidebar rail on wide viewports, compact navigation strip on narrow viewports)
- Live CSS styling via `kryonlabs/css` (`:root` custom properties, `hwb()` colors, `%` opacity, `min-block-size`, `@media (prefers-color-scheme: dark)`, `@media (-kryon-density: compact)`, `#id`, `.class`, `:hover`, `:selected`, `:focus`)
- Searchable results (`TextField` + filter chips + scrollable rows + detail inspector)
- Explicit loading (`LoadingIndicator`), empty (`EmptyState`), and error (`Alert` + retry) states
- Keyboard-operable settings modal (`ResolvedModal`, `Toggle`, `Checkbox`, `TextField`, `Tab` / `Shift+Tab`, `Enter`, `Escape`)

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
css = "../../../packages/css"
```

Override Kryon and CSS only: the lock's checked packages belong to the pinned
Ziran, so overriding Ziran too is refused. Run `ziran pkg paths kryon css` to
confirm the local sources. Use ordinary project commands during local development to leave the shared lockfile intact.

## Typecheck, build, and test

From `examples/dashboard/`:

```sh
for f in src/model.zi src/dashboard_state.zi src/styles.zi src/dashboard.tsx src/app.tsx src/desktop_host.zi previews/*.zi previews/*.tsx tests/dashboard_test.zi; do
  ziran check --project "$f" || exit 1
done

KRYON_TEST_DRIVER=1 ../../build/bin/kryon build
env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER \
  ziran run --project --target=c tests/dashboard_test.zi
```

## How the layout is written

| Part | Containers and CSS |
| --- | --- |
| Shell | `Row.shell` at the viewport; children stretch to its height |
| Sidebar | `Column.sidebar { width: 216px }`; nav buttons with `.full_width = true` |
| Main | `Column.main` takes the width the sidebar left |
| Header | `Row.header` with `Row.header-actions { width: 246px; margin-left: auto }` |
| KPI cards | `Grid.kpis` (3 tracks, 2 under 760 px) of `Column.kpi` cards that paint their background and border and size to their text |
| Body | `Column.body { flex: 1 }` takes the remaining height; a measured `Row.split` gives the list `flex: 1 1 0` and the inspector `flex: 0 0 340px`, or stacks them under 760 px |
| Alert, EmptyState, spinner | a `Layout.Stack` slot of fixed height, drawn with `slot.content` |

## Screenshots

```sh
kryon run --png build/shots/light.png --width 1120 --height 720 --theme light
kryon run --png build/shots/dark.png --width 1120 --height 720 --theme dark
kryon run --png build/shots/narrow.png --width 550 --height 800 --theme light
```

At the pinned Kryon and CSS, the 1120x720 shots decode to these pixel
digests (SHA-256 of the RGB rows, as `tests/templates_test.py` computes
them): light `5591e70c81e697696871a1d72862864c499e0ea68506c7120695df1361bf72d3`,
dark `19701865acddf4ad2be826fe0f8faeb12d6caf69b3a7684e450d98732cb02e94`; the
550x800 narrow shot `3ee5c72d50ee64bfdb7029d675a4b6a5b8c68d278a0f248f92df3f432bb00eb4`.

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

[`previews/responsive.tsx`](previews/responsive.tsx) uses CSS for the KPI grid,
chart panel tracks, padding, colors and headline size. The metrics flow into
four columns at 1200 pixels and two at 550. The first chart panel spans the
row, and the remaining two stack below 820 pixels. The trade ledger keeps a
shared column grid inside a horizontal scroll viewport. Chart slots are
ready for Plot, with native API equivalents in the
[HTML/CSS mapping](../../docs/HTML_TO_KRYON.md#author-a-responsive-dashboard).

Use the checkout overrides above or update both Kryon and CSS in your
project to the matching revisions from CSS's manifest and run `ziran lock`.
From the Kryon root, generate native snapshots, compile the same `.tsx` frame
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

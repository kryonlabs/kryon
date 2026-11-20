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

```sh
mkdir -p build/previews
kryon preview default  --theme light --width 1120 --height 720 --output build/previews/default-light-wide
kryon preview default  --theme dark  --width 460  --height 760 --output build/previews/default-dark-narrow
kryon preview loading  --theme light --width 1120 --height 720 --output build/previews/loading
kryon preview empty    --theme light --width 1120 --height 720 --output build/previews/empty
kryon preview error    --theme light --width 1120 --height 720 --output build/previews/error
kryon preview settings --theme dark  --width 1120 --height 720 --output build/previews/settings-dark
```

# Dashboard Starter

A reusable, editable native Kryon dashboard starter demonstrating:

- Responsive wide/narrow layout (sidebar rail on wide viewports, compact navigation strip on narrow viewports)
- Live CSS styling via `kryonlabs/css` (`:root` custom properties, `hwb()` colors, `%` opacity, `min-block-size`, `@media (prefers-color-scheme: dark)`, `@media (-kryon-density: compact)`, `#id`, `.class`, `:hover`, `:selected`, `:focus`)
- Searchable results (`TextField.Bind` + filter chips + scrollable rows + detail inspector)
- Explicit loading (`LoadingIndicator`), empty (`EmptyState`), and error (`Alert` + retry) states
- Keyboard-operable settings modal (`Modal.Draw`, `Toggle`, `Checkbox`, `TextField.Bind`, `Tab` / `Shift+Tab`, `Enter`, `Escape`)

## Reproducible working-tree setup (Kryon / CSS / Ziran)

When running inside this Kryon checkout before starter/library changes are tagged on remote `master`, create `examples/dashboard/ziran.local.toml` (gitignored) so both `dashboard` and `css` resolve against the **same** local Kryon package identity (`https://github.com/kryonlabs/kryon.git` in `ziran.toml` and `packages/css/ziran.toml`):

```toml
[overrides]
kryon = "../.."
ziran = "../../../../ziranlang/ziran"
css = "../../../packages/css"
image_codec = "../../../../ziranlang/packages/image-codec"
avif = "../../../../ziranlang/packages/avif"
webp = "../../../../ziranlang/packages/webp"
```

Then lock and verify the single shared Kryon identity and overrides from `examples/dashboard/`:

```sh
ziran lock
ziran pkg list
ziran pkg path kryon
ziran pkg path Css
```

(`ziran.lock` is listed in `examples/dashboard/.gitignore` so local development locks are kept out of commits pinning unreleased Kryon changes.)

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

Fixtures in `previews/` (`default`, `loading`, `empty`, `error`, `settings`) export `Frame` for `kryon preview`:

```sh
mkdir -p ../../build/examples/dashboard/previews
../../build/bin/kryon preview default  --theme light --width 1120 --height 720 --output ../../build/examples/dashboard/previews/default-light-wide
../../build/bin/kryon preview default  --theme dark  --width 460  --height 760 --output ../../build/examples/dashboard/previews/default-dark-narrow
../../build/bin/kryon preview loading  --theme light --width 1120 --height 720 --output ../../build/examples/dashboard/previews/loading
../../build/bin/kryon preview empty    --theme light --width 1120 --height 720 --output ../../build/examples/dashboard/previews/empty
../../build/bin/kryon preview error    --theme light --width 1120 --height 720 --output ../../build/examples/dashboard/previews/error
../../build/bin/kryon preview settings --theme dark  --width 1120 --height 720 --output ../../build/examples/dashboard/previews/settings-dark
```

# Kryon Architecture

Kryon is a reusable UI runtime: a small C support library with a
raylib-compatible surface plus the shared pieces downstream apps need — UI
widgets, layout, text, themes, locale, desktop services, and a portable
`.kry` language that compiles to native C, C++, and Go.

The architecture keeps **portable policy** (widget behavior, layout, style,
interaction) authored in `.kry` and compiled into every target, while
**hosts** supply the device services those policies call on (font shaping,
rasterization, input, clipboard, IME, time, storage).

## Layering

| Layer | Location | Role |
|---|---|---|
| Portable runtime policy | `runtime/*.kry` | Shared widget declarations, layout, style, text, theme, KSS, accessibility, popup, and input policy. One source of truth, compiled to C, C++, Go, and (paused) JavaScript. |
| Native UI | `src/ui/*.kry` | Native widget entry points and retained-tree implementation, compiled to C by `k2c`. |
| C runtime | `src/` | Platform and raster services, backends, serialization, sync/update, scene, filesystem. |
| Native Go runtime | `go/kryon/` | Pure-Go host (no cgo): frame operations, software rasterizer, host adapters for the generated `.kry` policy. |
| Public headers | `include/` | App-facing API: `kryon.h`, widget props (generated), platform and format modules. |
| Tools | `cmd/` | `k2kir`, `k2c`, `k2cpp`, `k2go`, `k2b`, `krb-run`, `krb-sdl`, `kryon-preview`, `kssfmt`, `kt`, `kryon`. |

## Ziran migration in progress

`zi/geometry.zi` defines the portable rectangle record. `zi/layout.zi` ports
the pure policy from `runtime/layout.kry`, including content insets, automatic
child placement, and flex spacing and alignment. `zi/group.zi` composes those
policies into group bounds. A separate Ziran program imports these modules
normally; the
[layout test](../tests/ziran_layout_test.sh) checks source and saved `.zir`
builds in C, C++, and Go without linking a UI runtime. It runs scalar and
record subsets and the complete layout calculation test from `.zib`, built
from both `.zi` and saved `.zir`.

`zi/widget_kind.zi`, `zi/accessibility_props.zi`, and
`zi/accessibility_policy.zi` port the widget-kind and accessibility decisions.
The [accessibility test](../tests/ziran_accessibility_test.sh) builds the
ordinary imported modules from `.zi` and saved `.zir` for C, C++, and Go.
`make ziran-test` runs both migration tests with an adjacent Ziran checkout.
The active Kryon build still uses the corresponding `.kry` modules; the `.zi`
modules are not yet wired into widget rendering or downstream applications.

## Core contracts

### One widget, one declaration

Every widget has exactly one canonical declaration in `.kry`, which owns its
typed props and defaults, per-instance state, events, measurement and layout
policy, child composition, and appearance. Generated C and native Go consume
the same declaration. Adding an application-defined widget must not require
editing compiler name tables, backend props tables, or handwritten widget
implementations.

Variations belong in props, not parallel widgets: `Button` takes a size prop
rather than shipping a separate small-button entry point; `Text(TextProps)` and
`Image(ImageProps)` are each one public signature.

### Shared policy, host services

`.kry` policy decides *what* a widget does; hosts decide *how* the device
carries it out. Hosts provide font shaping and rasterization, drawing, input
delivery, clipboard, IME, time, and storage. Shared modules include:

- `runtime/button.kry` (+ `button_props.kry`, `control_props.kry`) — Button
  measurement, input interpretation, motion, and paint sequencing.
- `runtime/layout.kry` — flex distribution and inset/centering.
- `runtime/material.kry` / `runtime/surface.kry` — material surface layers and
  focus bloom.
- `runtime/paragraph.kry` / `runtime/text.kry` / `runtime/text_input.kry` /
  `runtime/text_rows.kry` — text tokenization, wrapping, editing, and rows.
- `runtime/theme.kry` — theme role derivation (contrast, tone, disabled alpha).
- `src/ui/theme_color.kry` — native theme color lookup, mixing, contrast,
  semantic roles, and mode defaults; C retains platform theme discovery and
  active theme storage.
- `runtime/kss_parser.kry` / `runtime/kss_formatter.kry` /
  `runtime/style_sheet.kry` — the KSS language: parsing, cascade, formatting.
- `runtime/accessibility_policy.kry` — accessibility action eligibility.
- `runtime/popup_policy.kry` / `runtime/popup_ownership.kry` — popup lifecycle,
  capture, autofocus, and retirement.
- `runtime/instance.kry` — per-instance state retention and expiration.
- `runtime/drawing_props.kry` / `runtime/input_props.kry` — shared record
  contracts (`Vector2`, `Rectangle`, `Color`, `Texture2D`; the input sample).

### Generated C/Go parity

`runtime/*.kry` is one checked module set. Changes to a widget or semantic must
land in both generated runtimes and in parity coverage in the same change.
`make generated-runtime-parity-test` executes generated C and Go fixtures with
interaction assertions and final-state comparisons.

### Generated code is build output

Generated C headers/sources and generated Go/JavaScript modules are build
artifacts, not tracked sources. Edit the `.kry` owner and regenerate; never
hand-edit generated output. See `docs/BOUNDARIES.md` for the full ownership
lines.

## Backends

Backend selection is link-time via `KRYON_BACKEND`:

- `raylib` — default desktop, Android, Windows, and WebGL path.
- `libdraw` — plan9port libdraw/devdraw backend.
- `termi` — terminal-cell backend.
- `canvas` / `dom` — HTML5 Canvas2D and DOM backends (paused web target).
- `null` — generated no-op stubs for headless tests.

Backends implement Kryon behavior through the same public/runtime contracts;
support is documented in `docs/BACKEND_CAPABILITIES.json`, `docs/BACKENDS.md`,
and `docs/FEATURE_MATRIX.md`.

## Formats and tools

The `.kry`, KIR, generated C, generated Go, and KRB paths are Kryon-owned
tooling surfaces:

```text
k2kir app.kry        # .kry -> .kir
k2c  app.kry|app.kir # .kry -> C
k2cpp app.kry        # .kry -> C++ (extern "C" over the C runtime)
k2go  app.kry        # .kry -> Go (native Go runtime, no cgo)
k2b  app.kry|app.kir # .kry -> .krb cartridge
```

Tool changes update the matching specs (`docs/KRY_LANGUAGE_SPEC.md`,
`docs/KRB_FORMAT.md`), conformance matrix data, examples, and generated
documentation.

## Live preview

`cmd/kryon-preview/` owns build processes, session directories, diagnostics,
and loaded app hosts. The watch loop rebuilds an app host in the background,
`dlopen`s it, and swaps it in on success while preserving the previous host on
failure. Kryon owns the preview tooling; it must not depend on Krait.

## Tests and matrices

- boundary and naming checks for repository hygiene
- public API snapshot checks for app-facing identifier drift
- public header compile checks for include hygiene
- examples manifest checks for example inventory and exactness fixtures
- backend capability checks for backend inventory drift
- generated-file checks for docs, compatibility headers, icons, and matrices
- parser, runtime, sync, update, platform, and widget tests
- conformance and visual matrix checks across renderers and runtime paths

Use `make preflight` before committing focused changes, `make test` for the
broader regression suite, `make test-asan` / `make test-ubsan` for
sanitizer-backed runs, and `make release-preflight` before publishing.

## Downstream integration

Downstream applications vendor Kryon as a submodule. Permanent Kryon changes
must be made and committed here first, then brought into apps by updating the
submodule pointer. Never edit a downstream `vendor/kryon` tree as the source of
a Kryon change.

## Migration status

The canonical `.kry`-authored runtime is the migration target; some handwritten
C widget sources and host responsibilities are still being consolidated. Track
remaining work in `plan/` (see `plan/README.md` and `plan/COMPLETION.md`) and
`docs/COMPLETION_EVIDENCE.md`, not in this document.

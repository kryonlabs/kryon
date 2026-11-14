# Kryon package imports

Status: approved for full migration on 2026-11-12.

## Target

Kryon applications use named, package-qualified Ziran imports:

```zi
Text :: #import "kryon/text"

Text.Draw(session, Text.Props.{
    .key = cast(u64)2,
    .bounds = Rectangle.{24.0, 48.0, 272.0, 64.0},
    .text = "Hello, world!",
})
```

This keeps Ziran's Jai-inspired `::` declarations and `#import` syntax while
making a library name and its origin visible at every use. Go is only the
clarity model. An ordinary flat import is convenient inside one package, but
it is confusing at a library boundary because `Text`, `Props`, or `Draw` no
longer says which package supplied it. Kryon examples and consumers therefore
use named imports, without `using`.

Each widget package owns:

- `Props` for its primary input record;
- `Draw` for its primary retained-widget call;
- short package-local names for closely related records and enums when the
  old name repeats the package name;
- descriptive names for additional operations when one package contains more
  than one widget.

There are no public `*_props` packages, `WidgetProps` names, `Widget.Widget`
calls, compatibility aliases, or legacy import spellings after the migration.
Internal schema files may remain separate only where codec generation or
import-cycle boundaries require them; the widget package re-exports their
public declarations and users never import them.

Ziran already has the required language support: package-qualified imports,
named imports, and public `using` import re-exports. No UI-specific compiler
feature is planned.

## Widget inventory and rename groups

The current split input modules are:

`box`, `breadcrumb`, `button`, `card`, `checkbox`, `collapsible`,
`colorpicker`, `commandpalette`, `confirmpopover`, `dropdown`, `fieldset`,
`gridmulti`, `guidedtour`, `hovercard`, `image`, `layout`, `link`, `listbox`,
`menu`, `modal`, `navigation`, `overlay`, `page`, `panedview`, `press`,
`progress`, `radio`, `reorder`, `scroll`, `segmented`, `selectable`,
`separator`, `shape`, `sheet`, `slider`, `spinbox`, `svgicon`, `swiperow`,
`tabs`, `text`, `timepicker`, `titlebar`, `toast`, `toggle`, `toolbar`, and
`tooltip`.

Shared/core input records (`control`, `drawing`, `input`, `text_input`,
`accessibility`, `canvas`, `grid`, `router`, `scene_tree`, and `swipe`) remain
in explicitly named core packages rather than pretending to be widgets.

The mechanical rename is `WidgetProps` to `Props` and `Widget` to `Draw`
inside the widget's named package. Related types lose the redundant package
prefix where qualification already provides it, for example
`Text.Wrap`/`Text.Range`, `Dropdown.Option`/`Dropdown.Result`, and
`TabBar.Tab`/`TabBar.Result`. Multi-widget modules are split or keep
unambiguous operation names so a package never exposes names such as
`Modal.ModalFrameProps`.

Codec functions retain wire-schema names where those names are part of the
host protocol; their generated source imports the owning widget package and
refers to `Package.Props`. Wire field names and serialized compatibility are
not changed merely for source spelling.

## Consumer inventory

The source search under `/mnt/storage/Projects` found these consumer groups.
Repository ownership and active sessions are checked before editing each one:

- `kryonlabs/kryon`: core, examples, templates, docs, tests, website demo;
- `harness/harmony`, `harness/music`;
- `kryonlabs/browser`, `explorer`, `github`, `rill`, `taskmgr`, `terminal`,
  `workbook`;
- `kryonlabs/packages/{css,data-views,game2d,plot}`;
- `waozixyz/apps/{breeze,photos,werk}`;
- `waozixyz/atr`;
- `waozixyz/catalog/retro/{scraper,site}`;
- `waozixyz/games/{apple,arcade,choano,katalis,shark,shovey,wurm,zoombinis}`;
- `waozixyz/learning/jungle`;
- `waozixyz/modules/{apps,auth,dew,entity,gamekit}`;
- `ziranlang/ziran-vscode` fixtures that classify Kryon hello programs.

Generated output, `build/`, `vendor/`, `backup/`, archives, and package caches
are not migration sources. Downstream vendor copies are never edited.

## Order

1. [ ] Establish widget package exports, `Props`/`Draw` names, and focused
   source/saved-IR/package-resolution tests in Kryon.
2. [ ] Update codec generation and regenerate `widget_codec.zi`, preserving
   concurrent pointer-validation work.
3. [ ] Convert Kryon core call sites, examples, templates, tests, API docs, and
   the website demo to named package imports.
4. [ ] Replace the old hello module/host pair with one runnable
   `examples/hello.zi`; remove its automated screenshot path and keep other
   capture examples unchanged.
5. [ ] Migrate each external consumer with a local Kryon override and run its
   own focused and full local test targets through `build-queue`.
6. [ ] Run Kryon's focused tests, `make test`, and `make push-check` through
   `build-queue`; run the hello window only on a private scrubbed Xvfb.
7. [ ] Reduce the auto-discovered Ziran behavior suite by about 20% (223 to
   178 scripts), removing redundant or shallow cases while retaining the
   clearer integration, regression, backend, and button-behavior coverage.
8. [ ] Commit each verified repository once. Push the complete compatible set
   together; do not release or bump versions.
9. [ ] Add the portable lowercase `kryon/app` application boundary:
   `App.Window`, `App.Frame`, and `App.Run(window, draw)`. Application source
   imports only `kryon/app` and widget packages; the app package selects its
   host from the build target and diagnoses targets without a host clearly.
10. [ ] Put platform hosts behind lowercase `kryon/host/desktop`,
    `kryon/host/web`, `kryon/host/android`, and `kryon/host/terminal` package
    names. Keep platform-only code such as Linux GIO accessibility inside its
    host so other targets never parse its C ABI.
11. [ ] Remove public names that differ only by case or stutter:
    `Desktop`/`desktop`, `desktop_run`, `*_widget`, and `*_props`. Keep one
    lowercase package name per thing and no compatibility aliases.
12. [ ] Keep `examples/hello.zi` human-readable: a multi-line `main` declares
    `App.Window`, then a void `Draw(App.Frame)` submits only `Text.Draw` with
    `Text.Props.{.text = "Hello, world!", .size = 32}`. The frame supplies the
    session and default bounds; no explicit widget key or status return.
13. [ ] Add `examples/window.zi`, the smallest program that calls
    `App.Run(window)` with no draw callback, and verify that native window on a
    private scrubbed Xvfb.

## Risks

- Public `using` re-export behavior must remain identical in source, saved IR,
  native C, Go, and portable bundles.
- Renaming records affects generated codec symbols and host bindings; source
  spelling must change without accidentally changing the wire contract.
- Some modules contain several related widgets and need deliberate package
  boundaries instead of a blind prefix removal.
- Concurrent dirty work exists in Kryon and several consumers. Overlapping
  files are serialized with their owning task, and only migration hunks are
  staged.
- Package-qualified imports require project builds or an explicit
  `--module-path kryon=...`; standalone example and editor commands must carry
  that mapping.
- `kryon/app` is portable but its hosts are target-specific. Native C selects
  the SDL/Cairo desktop host, Wasm selects the Canvas host, Android selects its
  activity host, and other targets either select an explicit host or stop with
  a named `kryon/app` diagnostic. Linux GIO and other C-only declarations must
  never be reachable from Go, Wasm, or ZX checking.


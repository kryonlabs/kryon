# Kryon

Kryon is a small C support library for raylib-style applications. It keeps a
raylib-compatible public surface available through `kryon.h`, then adds the
pieces shared by downstream apps: UI controls, Tk-style toolkit widgets, layout
helpers, text rendering, themes, embedded assets, locale loading, file dialogs,
desktop tray support, runtime asset downloads, and Ksync account/sync helpers.

## Layout

- `include/` - public headers, including `kryon.h` and generated raylib
  compatibility declarations
- `src/` - Kryon implementation files, with reusable UI modules under `src/ui/`
- `icons/` and `pfp/` - PNG icon inputs embedded into `src/ui/ui_icon_assets.c`
- `themes/` - built-in theme files for the runtime theme loader
- `fonts/noto/` - bundled Noto Sans TTF/OTF font assets
- `mk/` - Make fragments for native, web, Android, Windows, packaging, and
  vendored dependency builds
- `scripts/` - asset embedding, icon embedding, raylib preparation, and boundary
  check helpers
- `examples/` - small programs that exercise Kryon UI features
- `tests/` - focused C tests for account, sync, and transition helpers
- `vendor/raylib`, `vendor/curl`, `vendor/liboqs` - source submodules used by
  downstream app builds

## Building

Kryon builds with the repository Makefile:

```bash
make
```

On FreeBSD, use the repository `makefile` entrypoint. It delegates to the GNU
Make build while keeping `make` as the command downstream apps can run:

```bash
make bsd-check
```

Run the focused tests with:

```bash
make test
```

Backend selection is link-time via `KRYON_BACKEND`. The default is `raylib`;
`canvas`, `null`, and `libdraw` are also available for their target
environments. The plan9port path is:

```bash
make KRYON_BACKEND=libdraw PLAN9PORT_DIR=/mnt/storage/Projects/plan9port
make libdraw-test
```

Build a distributable static-library archive with:

```bash
make dist-static
```

The archive is an extracted SDK layout. It contains `libkryon.a`,
`libraylib.a`, vendored static dependency libraries, public headers, API docs,
example consumers, CMake and pkg-config metadata, third-party notices, a package
manifest, and the submodule revisions used to build it. Validate it with:

```bash
make check-static-package
```

Run the full release readiness gate with:

```bash
make release-preflight
```

`libkryon.a` stays focused on Kryon object files. Companion libraries are shipped
beside it and linked through `lib/pkgconfig/kryon.pc`. OpenSSL is still treated
as an external system/toolchain dependency unless explicit OpenSSL static library
paths are supplied at build time.

## Releases

Every successful CI run on `master` automatically advances the patch version,
commits `include/kryon_version.h`, and starts the tag-driven `Release` workflow.
The workflow validates the version, creates an annotated tag, builds and tests
Kryon, and publishes both the static SDK and a checksummed native tools bundle.
The tools bundle contains `k2c`, `k2g`, `k2js`, `k2ir`, `k2b`, `kt`, `kryon`,
`kryon-preview`, `krb-run`, and `krb-sdl`. The renderer workflow also attaches
the Linux, Windows, and macOS `krb-run` builds plus the web player to the same
release.

Maintainers can still run `Release` manually for the checked-in version. Use
`scripts/bump-version.sh minor` / `major` before pushing when a non-patch bump is
required. Pushing a matching `v*` tag manually uses the same build-and-publish
path; releases reject tags that disagree with the public version header.

## Integration

To integrate Kryon into your project:

1. Add `include/` to your header search paths
2. Include `kryon.h` for the raylib-compatible API plus Kryon modules
3. Compile Kryon sources from `src/` and `src/ui/`, or use the `mk/` fragments
   to let Kryon assemble source lists and platform flags
4. Use `scripts/embed-icons.sh` and `scripts/embed-assets.sh` when your app
   needs custom embedded icons, locale files, fonts, themes, or image/audio
   assets. Kryon's checked-in `icons/` tree is the source of truth for shared
   UI, platform, payment, language, tile, profile, and first-party project
   icons; project logos live under `icons/proj/`. Downstream apps can call
   `vendor/kryon/scripts/sync-icons.sh` to copy those icons into web asset
   directories without keeping separate icon originals. Run `make
   icons-generate` after changing generated pixel icons.
5. Include `mk/vendor.mk` when your app enables curl or liboqs-backed features

Applications should keep build artifacts in their own build directories, but
the dependency source of truth and common build recipes should live under Kryon.
For TLS-enabled curl builds, `mk/vendor.mk` passes the `OPENSSL_*` make
variables through to the vendored curl CMake build.

## App Builds

Kryon owns the app command surface through `kryon`. From an app repository:

```sh
kryon build native
kryon build web
kryon build android-debug
kryon package linux-desktop
kryon package appimage
kryon preview
```

For local development against a sync backend, run a Ksync server in the
foreground. `dev-backend` locates the server source at `$KSYNC_DIR` or as a
sibling checkout (`../ksync`), isolates its data under `<project>/.kryon/`, and
prints the sync URL to point your app at. Tokens are regenerated each start,
so it is for local development only:

```sh
kryon dev-backend          # serves http://127.0.0.1:8080
```

App `project.kryon` files should use `target` entries that call `kryon`
rather than embedding platform-specific build commands directly. Existing app
Makefiles can remain as backend glue while repeated native, web, Android, and
packaging logic moves into Kryon `mk/` fragments.

## Preview Projects

Krait, the standalone Kryon IDE (a separate `kryonlabs/krait` repo that vendors
Kryon), previews `.kry` source by compiling it to an app host and loading it
into the embedded viewport. On each source change Krait rebuilds the project's
app host (`make kryon-host`, producing `build/kryon/app_host.so`), `dlopen`s
it, and resolves `CreateAppHost`/`DestroyAppHost`. The build runs in the
background (`fork` + non-blocking pipe, drained each frame) so the window stays
responsive during the compile; once it finishes the new app host is loaded and
the preview updates. Source changes are polled a few times per second. Kryon
itself owns the preview host tooling (`kryon-preview`); it must not depend on
Krait.

A project can add `project.kryon` metadata such as `preview_size`,
`preview_asset_root`, and `preview_scene` to control the embedded viewport.

## Conventions

Kryon follows raylib-style C conventions where practical: lowercase module
filenames, simple C structs, and public functions named like raylib APIs
(`InitWindow`, `DrawTexture`, `BeginUIFrame`, `ButtonNode`). Internal helpers
stay private to `src/` unless a downstream app needs the API in `include/`.

## Documentation

- `docs/API.md` documents the public API.
- `docs/KRY_LANGUAGE_SPEC.md` is the canonical `.kry` language and KIR
  contract.
- `docs/FEATURE_MATRIX.md` maps widget and feature support across the C, Go,
  and KRB targets and every renderer backend.
- `docs/BACKENDS.md` documents the backend architecture and selection.
- `docs/BACKEND_CAPABILITIES.json` is the checked backend inventory used by
  backend drift checks.
- `docs/ARCHITECTURE.md` maps the main Kryon subsystems and ownership lines.
- `docs/BOUNDARIES.md` defines what belongs in Kryon and what stays in
  downstream applications.
- `docs/PUBLIC_API_SNAPSHOT.txt` tracks public Kryon identifiers for API drift
  checks.
- `docs/site/` contains the static documentation website.
- `docs/AGENTS.md` documents how downstream apps should use Kryon, including
  modal/input capture and submodule update rules.

## Toolkit Direction

The `ui_tk.h` layer is Kryon's pragmatic Tk replacement surface. It stays in the
raylib style: one direct struct-and-call path per widget, caller-owned state,
immediate-mode drawing, and no builder objects or scripting runtime. The numbered
examples `09_geometry` through `18_accessibility` demonstrate each toolkit
feature family.

## Kry Language

`docs/KRY_LANGUAGE_SPEC.md` is the canonical Kry language contract. Kry source
lowers into KIR, a debuggable intermediate representation with source spans.
From there `k2c` emits readable C for native apps, `k2g` emits pure Go source
against the native Go runtime, while `k2b` emits a portable `.krb` cartridge
(`docs/KRB_FORMAT.md`) for renderers that implement the Kryon runtime contract.
The intended tool set is Unix-shaped:

```text
k2ir app.kry        # .kry -> .kir
k2c  app.kry|app.kir
k2g  app.kry        # .kry -> Go (native Go runtime, no cgo)
k2b  app.kry|app.kir
```

Generated Go imports `github.com/waozixyz/kryon/go/kryon` as `kryon` and uses
clean qualified widget names such as `kryon.Button`, `kryon.TextField`,
`kryon.Text`, `kryon.Row`, `kryon.Column`, `kryon.BeginFrame`, and
`kryon.EndFrame`. Pure Kry and Go-native externs must not use `import "C"`,
the removed bridge package, injected runtime objects, dot-imported runtime
names, or generated calls to stale prefixed C APIs. Explicit C externs with a
`c.` target, such as `#extern "c.abs"`, are the opt-in exception: `k2g` emits
their `import "C"` bridge in a separate generated `*_cgo.go` file.

Handwritten Go can use the same package directly. Generated code keeps the
explicit props form (`kryon.Button(kryon.ButtonProps{...})`,
`kryon.TextField(kryon.TextFieldProps{...})`) for deterministic layout/state,
while app code may use shorter direct calls such as `kryon.Button("Save")` and
`kryon.TextField("Name", &name)`.

The native Go runtime records each frame as pure Go `FrameOp` values available
through `FrameOps()`. That operation stream is the host boundary for native Go
windows/renderers: it carries resolved bounds, text, colors, focus, button
state, and redacted secure text without importing cgo or the removed bridge.
`RenderFrame` and `RenderCurrentFrame` provide a dependency-free software
renderer that turns those operations into an `image.RGBA`.
`NewHost` owns a persistent native Go runtime, runs generated or handwritten
frame functions, queues input, exposes frame operations, and renders frames
without cgo.

`k2g` output is compiled against that native runtime by the test suite, and the
generated Go/C parity tests drive both runtimes through the same scripted input.
This is an executable compatibility gate, not only a textual generated-source
check.

`k2c`, `k2js`, and `k2b` accept `.kry` source and run the KIR frontend
internally. Native platform, storage, and performance-sensitive C code remains
first-class through the C backend and through explicit KRB capabilities or host
imports.

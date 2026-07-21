# Kryon

Kryon is a small C support library for raylib-style applications. It keeps a
raylib-compatible public surface available through `kryon.h`, then adds the
pieces shared by downstream apps: UI controls, Tk-style toolkit widgets, layout
helpers, text rendering, themes, embedded assets, locale loading, file dialogs,
desktop tray support, runtime asset downloads, and Lyra account/sync helpers.

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

`libkryon.a` stays focused on Kryon object files. Companion libraries are shipped
beside it and linked through `lib/pkgconfig/kryon.pc`. OpenSSL is still treated
as an external system/toolchain dependency unless explicit OpenSSL static library
paths are supplied at build time.

## Releases

Kryon releases are tag-driven. Run the `Release` GitHub Actions workflow with a
version such as `v0.1.0`; the workflow validates the version, creates and pushes
the annotated tag, checks out that tag, builds and tests Kryon, packages the
static-library archive, uploads it as an artifact, and attaches it to the GitHub
release. Pushing a `v*` tag manually uses the same build-and-publish path.

## Integration

To integrate Kryon into your project:

1. Add `include/` to your header search paths
2. Include `kryon.h` for the raylib-compatible API plus Kryon modules
3. Compile Kryon sources from `src/` and `src/ui/`, or use the `mk/` fragments
   to let Kryon assemble source lists and platform flags
4. Use `scripts/embed-icons.sh` and `scripts/embed-assets.sh` when your app
   needs custom embedded icons, locale files, fonts, themes, or image/audio
   assets
5. Include `mk/vendor.mk` when your app enables curl or liboqs-backed features

Applications should keep build artifacts in their own build directories, but
the dependency source of truth and common build recipes should live under Kryon.
For TLS-enabled curl builds, `mk/vendor.mk` passes the `OPENSSL_*` make
variables through to the vendored curl CMake build.

## App Builds

Kryon owns the app command surface through `kryon-app`. From an app repository:

```sh
kryon-app build native
kryon-app build web
kryon-app build android-debug
kryon-app package appimage
kryon-app preview
```

App `project.kryon` files should use `target` entries that call `kryon-app`
rather than embedding platform-specific build commands directly. Existing app
Makefiles can remain as backend glue while repeated native, web, Android, and
packaging logic moves into Kryon `mk/` fragments.

## Preview Projects

Kryon IDE can host richer project previews without requiring the project to be
rewritten as `.kry` screens. A project can add `project.kryon` metadata such as
`preview_size`, `preview_asset_root`, and `preview_scene`, then provide a
`build_live` command that builds `build/kryon/live_preview.so`. The live module
continues to expose the normal `CreateKryonLivePreview`/`AppHost` entry point,
and can optionally expose `CreateKryonPreviewHost` for future scene/layer editing
metadata. This keeps gameplay or simulation code in the app while letting Kryon
IDE select scenes, draw interactive previews, and eventually edit shared preview
layers.

## Conventions

Kryon follows raylib-style C conventions where practical: lowercase module
filenames, simple C structs, and public functions named like raylib APIs
(`InitWindow`, `DrawTexture`, `BeginUIFrame`, `DrawUIButton`). Internal helpers
stay private to `src/` unless a downstream app needs the API in `include/`.

## Documentation

- `docs/API.md` documents the public API.
- `docs/site/` contains the static documentation website.
- `docs/AGENTS.md` documents how downstream apps should use Kryon, including
  modal/input capture and submodule update rules.

## Toolkit Direction

The `ui_tk.h` layer is Kryon's pragmatic Tk replacement surface. It stays in the
raylib style: one direct struct-and-call path per widget, caller-owned state,
immediate-mode drawing, and no builder objects or scripting runtime. The numbered
examples `09_geometry` through `18_accessibility` demonstrate each toolkit
feature family.

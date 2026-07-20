# Flint

Flint is a small C support library for raylib-style applications. It keeps a
raylib-compatible public surface available through `kryon.h`, then adds the
pieces shared by downstream apps: UI controls, Tk-style toolkit widgets, layout
helpers, text rendering, themes, embedded assets, locale loading, file dialogs,
desktop tray support, runtime asset downloads, and Lyra account/sync helpers.

## Layout

- `include/` - public headers, including `kryon.h` and generated raylib
  compatibility declarations
- `src/` - Flint implementation files, with reusable UI modules under `src/ui/`
- `icons/` and `pfp/` - PNG icon inputs embedded into `src/ui/ui_icon_assets.c`
- `themes/` - built-in theme files for the runtime theme loader
- `fonts/noto/` - bundled Noto Sans TTF/OTF font assets
- `mk/` - Make fragments for native, web, Android, Windows, packaging, and
  vendored dependency builds
- `scripts/` - asset embedding, icon embedding, raylib preparation, and boundary
  check helpers
- `examples/` - small programs that exercise Flint UI features
- `tests/` - focused C tests for account, sync, and transition helpers
- `vendor/raylib`, `vendor/curl`, `vendor/liboqs` - source submodules used by
  downstream app builds

## Building

Flint builds with the repository Makefile:

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

The archive is an extracted SDK layout. It contains `libflint.a`,
`libraylib.a`, vendored static dependency libraries, public headers, API docs,
example consumers, CMake and pkg-config metadata, third-party notices, a package
manifest, and the submodule revisions used to build it. Validate it with:

```bash
make check-static-package
```

`libflint.a` stays focused on Flint object files. Companion libraries are shipped
beside it and linked through `lib/pkgconfig/flint.pc`. OpenSSL is still treated
as an external system/toolchain dependency unless explicit OpenSSL static library
paths are supplied at build time.

## Releases

Flint releases are tag-driven. Run the `Release` GitHub Actions workflow with a
version such as `v0.1.0`; the workflow validates the version, creates and pushes
the annotated tag, checks out that tag, builds and tests Flint, packages the
static-library archive, uploads it as an artifact, and attaches it to the GitHub
release. Pushing a `v*` tag manually uses the same build-and-publish path.

## Integration

To integrate Flint into your project:

1. Add `include/` to your header search paths
2. Include `kryon.h` for the raylib-compatible API plus Flint modules
3. Compile Flint sources from `src/` and `src/ui/`, or use the `mk/` fragments
   to let Flint assemble source lists and platform flags
4. Use `scripts/embed-icons.sh` and `scripts/embed-assets.sh` when your app
   needs custom embedded icons, locale files, fonts, themes, or image/audio
   assets
5. Include `mk/vendor.mk` when your app enables curl or liboqs-backed features

Applications should keep build artifacts in their own build directories, but
the dependency source of truth and common build recipes should live under Flint.
For TLS-enabled curl builds, `mk/vendor.mk` passes the `OPENSSL_*` make
variables through to the vendored curl CMake build.

## Conventions

Flint follows raylib-style C conventions where practical: lowercase module
filenames, simple C structs, and public functions named like raylib APIs
(`InitWindow`, `DrawTexture`, `BeginUIFrame`, `DrawUIButton`). Internal helpers
stay private to `src/` unless a downstream app needs the API in `include/`.

## Documentation

- `docs/API.md` documents the public API.
- `docs/site/` contains the static documentation website.
- `docs/AGENTS.md` documents how downstream apps should use Flint, including
  modal/input capture and submodule update rules.

## Toolkit Direction

The `ui_tk.h` layer is Flint's pragmatic Tk replacement surface. It stays in the
raylib style: one direct struct-and-call path per widget, caller-owned state,
immediate-mode drawing, and no builder objects or scripting runtime. The numbered
examples `09_geometry` through `18_accessibility` demonstrate each toolkit
feature family.

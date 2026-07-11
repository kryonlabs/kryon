# Flint

Flint is a small C support library for raylib-style applications. It keeps a
raylib-compatible public surface available through `flint.h`, then adds the
pieces shared by downstream apps: UI controls, layout helpers, text rendering,
themes, embedded assets, locale loading, file dialogs, desktop tray support,
runtime asset downloads, and Lyra account/sync helpers.

## Layout

- `include/` - public headers, including `flint.h` and generated raylib
  compatibility declarations
- `src/` - Flint implementation files, with reusable UI modules under `src/ui/`
- `icons/` and `pfp/` - PNG icon inputs embedded into `src/ui_icon_assets.c`
- `themes/` - built-in theme files for the runtime theme loader
- `assets/fonts/` - generated locale font atlas outputs
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

## Integration

To integrate Flint into your project:

1. Add `include/` to your header search paths
2. Include `flint.h` for the raylib-compatible API plus Flint modules
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

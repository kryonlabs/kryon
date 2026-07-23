# Kryon App Build Plan

Goal: Kryon owns the repeatable build and packaging flow for every Kryon app.
App repositories should provide app metadata, native sources, `.kry` sources, and
assets. Kryon should provide the compiler, runtime, dependency builds, packaging
helpers, IDE preview hooks, and diagnostics.

## App Contract

Each app should define one project file, `project.kryon`, with the portable
metadata Kryon needs:

- app id, name, title, version, license, maintainer, website, categories
- source roots for C and `.kry`
- asset roots for images, sounds, fonts, locales, and generated embedded assets
- platform knobs such as Android id/activity, Linux desktop metadata, package
  names, and signing-key variables
- build feature flags such as file dialog, tray, sync, audio, and web runtime

The app repo may keep small glue files for platform entry points, but target
selection, dependency build rules, generated Kryon C output, and package layout
should come from Kryon.

## Unified Commands

Kryon should expose one command surface for all apps:

- `kryon build native`
- `kryon build web`
- `kryon build android-debug`
- `kryon build android-release`
- `kryon package linux`
- `kryon package appimage`
- `kryon package deb`
- `kryon package rpm`
- `kryon package flatpak`
- `kryon package snap`
- `kryon package click`
- `kryon package windows`
- `kryon preview`
- `kryon test`

Makefiles in app repos should become thin wrappers that call these commands.

## Build Ownership

Kryon owns:

- strict `.kry` parsing and file:line:column diagnostics
- generated C and headers under the app build directory
- runtime source selection for native, web, Windows, Click, and Android
- Raylib, curl, liboqs, and other shared vendor build rules
- embedded asset generation
- Linux desktop files, icons, metainfo, AppImage, deb, rpm, Flatpak, Snap, and
  Click packaging helpers
- Android CMake, Gradle property generation, APK/AAB assembly hooks, and asset
  copy helpers
- web/WASM HTML, JS, data-file generation, cache busting, and smoke-test hooks
- Podman-backed Linux packaging where host tools are missing or platform
  isolation is needed

Apps own:

- product code and app-specific C modules
- `.kry` pages, widgets, and screens
- translations and media assets
- app metadata values in `project.kryon`
- optional platform-specific shims that cannot be generalized cleanly

## Migration Phases

1. Rename old build variables and paths from Flint to Kryon in every app.
   `vendor/kryon` is the only runtime submodule path.
2. Keep app Makefiles working, but make them use `KRYON_DIR` and Kryon-owned
   `mk/*.mk` includes exclusively.
3. Move repeated Makefile logic from Inbe into Kryon `mk/app.mk`,
   `mk/android.mk`, `mk/web.mk`, and `mk/package-linux.mk`.
4. Add a Kryon CLI wrapper that reads `project.kryon` and invokes the shared
   make fragments with normalized variables.
5. Convert app Makefiles to compatibility shims, then remove duplicated app-side
   package/build rules.
6. Make the IDE use the same build graph for preview/run so errors from manual
   build, auto preview, and package targets use one diagnostic format.
7. Add template checks so new Kryon apps start with the unified layout.

## Vendor Rule

Vendored Kryon directories are pinned dependencies. App builds must not modify
files under `vendor/kryon`. Regenerated Kryon runtime assets, generated icon
tables, and shared packaging helpers must be changed in the root Kryon
repository, committed there, and consumed by updating the app submodule pointer.

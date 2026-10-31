# Ziran projects with Kryon

Kryon is a Ziran package. Application source imports its public modules, and
the selected Kryon host links the application's `Frame` function. No UI syntax
or widget handling is built into the Ziran compiler.

## Start a project

Install `ziran` (`make install-user` in the Ziran repository) and the `kryon`
command (`make install-user` here), then:

```sh
kryon new hello              # a desktop app; kryon new hello --template tui
cd hello
kryon run                    # the default profile; kryon run tui picks another
```

| Template | What it starts | Profiles |
| --- | --- | --- |
| `app` (default) | counter with buttons, a step slider, and progress | `desktop`, `tui`, `web` (canvas) |
| `tui` | keyboard-driven task list | `tui`, `desktop` |
| `pages` | Home, Library, and Settings behind a tab row, sharing state | `desktop`, `tui`, `web` (canvas) |
| `web` | semantic page: headings, paragraphs, links, a button | `web` (DOM), `canvas`, `desktop` |

Each template's `src/layout.zi` stacks rows in pixels in a window or browser
and in character cells on the terminal host, so one `Frame` serves every
profile. `kryon init --template tui` adds a template to an existing package:
Ziran merges it into `ziran.toml`, keeps the package's own values, and never
overwrites a file, so a command-line program keeps its `main` and gains a
window.

`kryon` is a front door to `ziran`: `kryon new` and `kryon init` are
`ziran new` and `ziran init` with Kryon's templates (`--template NAME` for
Kryon's, or any `SOURCE[:NAME]`), `kryon run|build|check|profiles` is
`ziran tool kryon …`, and `kryon install` is `ziran install`. Each project
therefore runs the Kryon its `ziran.lock` pins, whatever version of the
`kryon` command is installed. `KRYON_TEMPLATES=DIR` takes the templates from
a local Kryon checkout.

## Package manifest

A Kryon application is a Ziran package whose `ziran.toml` hands its program
to Kryon:

```toml
[package]
name = "Example"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
tool = "kryon"

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

[dependencies.kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"

[tool.kryon]
default_profile = "desktop"

[tool.kryon.profiles.desktop]
backend = "desktop"
```

`tool = "kryon"` makes `ziran run`, `ziran build`, `ziran check`, and
`ziran install` the same as the `kryon` commands. `bridge_modules` exposes
the app's `app` module to the selected host. `[tool.kryon] entry` names the
module with `Frame` when it is not the package entry, as in a command-line
package that also has a window. The app imports `Kryon` for the small core
surface or `Widgets` for all core widgets:

```zi
using UI :: #import "Widgets";

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    text: TextProps
    text.text = "Hello, Kryon"
    Text(session, text)
    return 0
}
```

Style sheets, charts, tables, trees, and source coloring are separate Git
packages: add `https://github.com/kryonlabs/kss.git`,
`https://github.com/kryonlabs/plot.git`,
`https://github.com/kryonlabs/data-views.git`, or
`https://github.com/ziranlang/syntax.git` with `ziran add` and import
`kss/Kss`, `plot/Plot`, `data_views/TableView`, `data_views/TreeView`, or
`syntax/Syntax`. Kryon exports `geometry`, `drawing_props`, and the other
building blocks listed in `ziran.toml` for code that needs those primitives
without importing the widget catalog. Native and browser profiles resolve
imports the same way, so an app's Git dependencies and package-qualified
imports such as `kryon/Widgets` work in Canvas and DOM builds too.

From the app directory, run:

```sh
kryon check
kryon build
kryon run desktop            # or: kryon run --profile desktop, ziran run desktop
kryon profiles               # the profiles, their hosts, and the default
```

Commit `ziran.lock` to pin exact package and toolchain revisions. `ziran
fetch` fills the shared cache. For local development, `ziran.local.toml` can
override the Kryon and Ziran roots; it is not the published dependency
manifest. See the [Ziran package
guide](https://github.com/ziranlang/ziran/blob/master/docs/PACKAGES.md)
for lock, cache, and override details.

## Profiles and hosts

### Foreign renderer artifacts

Builders can request Kryon's foreign libraries and standard resources through
the generic Ziran artifact API (toolchain revision 5461202417d48e2b31122354419179b262d7530c
or later):

```sh
ziran pkg artifact kryon:renderer-raylib --platform linux-x86_64-gnu --locked
CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar \
  ziran pkg artifact kryon:renderer-raylib --platform windows-x86_64-mingw --locked
ziran pkg artifact kryon:renderer-desktop --platform linux-x86_64-gnu --locked
ziran pkg artifact kryon:renderer-web --platform emscripten-wasm32-emscripten --locked
```

Omit `--locked` when using `ziran.local.toml` overrides. Success stdout is one
absolute `artifact.tsv` path. Verify its declared SHA-256 output rows and resolve
their paths beside that descriptor. `renderer.json` has schema 1, the artifact
and platform, ordered `archives` and `link_flags`, `compile_flags`, Ziran
`defines`, `include_directories`, and `resources`. Paths in those fields are
relative to the descriptor directory; system flags are argv tokens. Pass archive
paths after application objects, followed by `link_flags`. Pass compile flags
and resolved include directories when compiling native code; pass defines to
Ziran's ordinary compiler invocation. No generated Ziran source tree or object
list is part of this contract.

Every recipe includes `resources/kryon-font.ttf` and its Liberation license.
`renderer-raylib` also includes `lib/libraylib.a`, the four public Raylib headers
under `include/`, and the Raylib license. Linux uses SDL2/GLES2; Windows uses
RGFW/OpenGL 1.1. Both preserve JPEG, TrueType and audio support. Linux
`renderer-desktop` declares the existing SDL/Cairo/FreeType/Pango/Fontconfig/GIO
system dependencies rather than redistributing system libraries.

`renderer-web` supplies `web/ziran_web.js` from the selected Ziran toolchain,
`js_libraries`, browser link flags and `preload_files` entries (`path`, `mount`)
for the standard font at `/kryon-font.ttf`. Feed these to Emscripten's
`--js-library` and `--preload-file` arguments. Game resources remain separate.

The Raylib recipe also accepts `android-aarch64-bionic` and
`android-armv7-bionic`. Set `[tool.kryon.renderer] android_ndk` to the installed
NDK directory and `android_api` to a decimal API level (default 21, minimum 21).
Set `CC` to that ABI/API's NDK clang executable so compiler identity is keyed,
for example `aarch64-linux-android21-clang`. The provider uses Kryon's existing
Android Raylib helper with its GLES2 batching, JPEG/PNG/TTF and OGG/audio options.
NativeActivity glue, the application shared library, edition, packaging and
signing remain with the app packaging build. Unsupported recipe/platform pairs
fail explicitly. Other hosts and architectures are not yet provider targets.

Widget and renderer Ziran code remains ordinary `kryon/...` imports. These
artifacts do not provide a separately compiled Ziran component ABI: the current
generic artifact API stages provider files but does not define such a language
component interface. Builders must not rewrite generated C/H or reconstruct a
private module/object list to manufacture one.

Run `make renderer-artifact-test` for real outputs, hashes, dirty isolation and
a maintained Raylib renderer compiled and run on private Xvfb. Add `--cross`
to `python3 tests/renderer_artifact_test.py` for installed MinGW and Android
toolchains. After pushing, `--clean --revision COMMIT` uses normal immutable
package resolution and requires two independent consumers to receive the exact
same descriptor. A dirty canonical provider deliberately cannot prove reuse.

Define each profile under `[tool.kryon.profiles.NAME]`. It requires `backend`;
`codegen` defaults to `c99`. Supported backends are `terminal`, `desktop`,
`raylib`, `canvas`, `dom`, `pixmap`, and `android`. Set `default_profile` under `[tool.kryon]` when there
are several profiles. `--profile NAME` selects another. `run` asks for a
profile when there are several, none is specified, and it runs in a
terminal; otherwise it uses the default.

| Backend | Host | Requirements |
| --- | --- | --- |
| `terminal` | 80 × 24 terminal cells | TTY for interactive input; redirected output renders once |
| `desktop` | SDL2 window with Cairo raster | SDL2, Cairo, and FreeType development libraries |
| `raylib` | raylib SDL2/OpenGL ES 2 window | the locked raylib source package and SDL2, DRM, EGL, GLESv2 |
| `canvas` | Canvas2D browser page | Emscripten with Asyncify; see [Canvas host](canvas.md) |
| `dom` | semantic DOM page | Emscripten with Asyncify; see [DOM host](dom.md) |
| `pixmap` | headless 960 × 600 frame printed as a PPM image | nothing; `kryon run shot > frame.ppm` |
| `android` | installable APK through a generated `droid/` Gradle project | a JDK, the Android SDK with NDK `28.2.13676358` and CMake `3.22.1`, the locked raylib source package, and `[tool.kryon.android] package`; see [Android host](BACKENDS.md#android-host) |

The SDL desktop host presents its already rasterized pixels through the
software renderer and an unaccelerated window surface by default. Custom
SDL pixel hosts can import `kryon/pixmap_sdl` and call
`CreatePixmapRenderer(window)` on the window's owner thread before creating
their texture. The host retains ownership of its window, renderer, texture
and input loop; its raster workers are independent of this presentation choice.
Explicit `SDL_RENDER_DRIVER` and `SDL_FRAMEBUFFER_ACCELERATION` environment
preferences, or normal/override priority SDL hints, take precedence. The
helper leaves other hosts and their graphics APIs untouched.

These settings live only in `ziran.toml`. Kryon declares them under
`[options]` in its own `ziran.toml`. Ziran checks the application's
`[tool.kryon]` tables against that declaration, merges `ziran.local.toml` over
them, and hands the kryon tool the result, so a typo is reported with its file
and line. To try another default profile on one machine, set it in the ignored
`ziran.local.toml`:

`static_archive` links one project-relative `.a` file directly. The project's
own build prepares the archive, and Kryon adds that exact path as both a build
dependency and linker input. This is for reproducible source-package artifacts;
it performs no `-l` lookup and never selects an operating-system package. The
path cannot be absolute or lexically escape the project directory. Kryon also
resolves the archive and rejects a symlink that leaves the project or a
non-regular file. Browser profiles reject the setting instead of silently
omitting it. `library` remains the existing system-library escape hatch and
should be avoided by reproducible applications.

Native `c99` profiles can instead request dependency-owned foreign support
with `native_artifacts` and an explicit `native_platform`:

```toml
[tool.kryon.profiles.desktop]
backend = "desktop"
native_artifacts = "kryon:renderer-raylib"
native_platform = "linux-x86_64-gnu"
```

This adds Raylib's foreign support, for example for `MediaAudio`, to the
existing SDL/Cairo profile. It keeps that profile's host and rendering path.
Use space-separated `package:artifact` selections for several providers.
Selections come from the application's resolved package graph; no cache
paths or prebuilt archive copies belong in its manifest. The toolchain must
provide Ziran's [shared artifact consumer](https://github.com/ziranlang/ziran/commit/ee86330414a2febb0135d86506b505af5e6a660b).

Each build requests and verifies the artifacts before linking, so changed
provider inputs or local overrides cannot leave an old executable in place.
Kryon's renderer metadata or a provider's ordered `link.txt` supplies its
archives and prerequisite/system link arguments. These appear after the
application objects and before the profile's existing system libraries.
Provider failures, malformed metadata and digest mismatches fail the build;
the existing executable is replaced only after the complete link succeeds.
These options require a native `c99` profile; browser, Android and portable
profiles use their existing target-specific packaging paths. Compile options
and application asset packaging remain the selected profile's responsibility.

```toml
[tool.kryon]
default_profile = "tui"
```

## Zx applications

Desktop, raylib, Canvas and Android profiles can package an ordinary `Frame`
application as the same portable program with its resources:

```toml
[tool.kryon.profiles.portable]
backend = "desktop"
codegen = "zx"
assets = "assets"
```

Use `backend = "canvas"` with `codegen = "zx"` for the same portable app
and a browser player. The build writes both `.zx` and `.html` files; serve
the HTML with its generated JavaScript and WebAssembly files. The browser
shell interprets the bundle and uses Canvas2D for rendering. It includes a
file picker for opening another downloaded ZX without recompiling its UI.
Unsupported required services are reported before replacing the running app.

`backend = "raylib"` with `codegen = "zx"` builds a generic native player
beside the bundle. `make zx-raylib` builds that player directly; pass a ZX
path to it or use `--check FILE.zx` to validate without opening a window.
Images are decoded directly from the bundle, with no resource extraction.
The native player carries its host font beside the executable. Linux uses
SDL/GLES; Windows and macOS use GLFW/OpenGL. HTTP uses libcurl; install its
development package or supply `ZX_RAYLIB_HTTP_LIBS` for your target.
Cross builds can supply
`ZX_RAYLIB_CC`, `ZX_RAYLIB_AR`, `ZX_RAYLIB_OBJCOPY`,
`ZX_RAYLIB_ARCHIVE`, `ZX_RAYLIB_DEFINES` and `ZX_RAYLIB_LIBS`.
Use a separate `ZX_RAYLIB_DIR` for each target. macOS uses the same renderer
and has platform build settings, but has not been executed in this checkout.

An Android profile with `codegen = "zx"` builds its portable guest first,
then packages `app.zx`, the VM and the native renderer into the APK. Its
`Frame` is not compiled into native application code. The canonical scaffold
handles display density, safe-area insets, touch, hardware/soft-keyboard input
and graphics restoration without resetting bundle state. It also registers a
ZX file opener: Android document URIs are copied into private cache and
validated before replacing the running app. Invalid files retain the current
instance. Existing customized Android scaffolds must adopt the updated
Gradle/CMake and activity/manifest glue to use this profile.
`make zx-android-test` builds ARM64, ARMv7 and x86-64 shells. Runtime testing
uses `python3 tests/zx_android_test.py --serial emulator-NNNN` and accepts
only an explicitly selected disposable emulator.

`make zx-canvas` builds a generic browser player with no embedded app.
`ZX_CANVAS_BUNDLE=/absolute/path/app.zx` optionally embeds a default app.
A same-origin relative `?bundle=path/app.zx` opens a hosted download.
The browser player supports ordinary `Frame` bundles and standalone
`AppContext` apps, drawing, input, fonts, embedded images and the browser
clipboard. Native, Android and browser players provide packed audio,
asynchronous HTTP audio/text and a monotonic clock. Requests accept only
HTTP/HTTPS; guest paths cannot read host files. Android ZX scaffolds include
a network manifest overlay; ordinary native builds do not use that overlay.

Standalone `AppContext` apps use the existing lifecycle and hosted widget
contracts. They receive a private document directory per declared app ID,
wall time, local time offset, locale, embedded assets and clipboard services.
Native apps save on close; browser apps save periodically to IndexedDB and
restore on reload. Native documents live under `$XDG_DATA_HOME/kryon/apps`
(or `$HOME/.local/share/kryon/apps`), `%LOCALAPPDATA%/kryon/apps` on Windows,
and Android's private application storage. `KRYON_APP_DATA` can select a
different native parent directory. Account/device services that the host
does not provide return the existing explicit unavailable response.

`make gallery-zx` exports every enabled gallery example with its full source
behavior and embedded assets under `build/gallery-zx/`; `index.json` maps
example IDs to their bundle paths. The entire gallery also has a `portable`
profile. The website build stages both these downloads and the generic
browser player; generating them does not publish the website.
The exporter and player use the gallery's configured Ziran toolchain, including
its lock or local override. A ZX's embedded IR format must match its player;
the installed compiler on `PATH` can use a different format.
`make gallery-zx-test` validates and renders all exported examples on a
private X server. `make zx-canvas-test` checks that the desktop and browser
bundles are identical and exercises browser startup, images, clicks, Unicode,
resizing, file replacement and rejection of invalid files.
`make zx-services-test` exercises real media and HTTP on native and browser
players. `make zx-hosted-test` checks hosted widgets, packed images and
document persistence. `make zx-headless-test` runs VM drawing, packed PNG,
retained input and Unicode without a display. The Android service and hosted
tests require `--android-serial emulator-NNNN` and `--serial emulator-NNNN`
respectively and verify the disposable `KryonZx` AVD before installing.

Custom native hosts can load `kryon/bundle_player`, prepare embedded assets,
attach `kryon/bundle_raster` callbacks and then start the instance. This lets
initializers use the selected renderer and assets. `PlayerFrameNative` runs
inside the caller's ordinary frame. `PlayerFrame` retains the CPU-surface
route for hosts that supply pixel buffers.

`kryon build portable` writes `build/<package-name>-portable.zx`.
All four project templates include this profile.
`kryon run portable` builds it and launches the reusable desktop player.
After downloading that file, `kryon run app.zx` works without the app's source,
manifest, lock, or compiler-generated native code. Install the player with
`make install-user` in Kryon. The player needs SDL2, Cairo and FreeType.

`assets` is an optional project-relative directory or file, embedded under that
same path. Images retain their ordinary `ImageProps.asset_path` values. The
player extracts validated resources into a private temporary directory and
removes it when the app exits. Exports rebuild resource content on every build,
including file removals. A `pixmap` profile also accepts `codegen = "zx"` and
runs through the display-free Ziran interpreter.

The desktop player supplies shapes, text, path-backed PNG images, window
visibility/close handling, clipboard, pointer buttons, wheel, modifiers,
keyboard and Unicode input. App globals and retained sessions persist between
frames. Unsupported capabilities fail before a window opens. Native library
and static archive settings cannot be linked into a portable program.
Applications with custom native storage, audio, pointer casts or C callbacks
must first give those effects portable host interfaces. The desktop player
does not yet replace these application-specific hosts.

`make zx-test` exports a package-qualified app, copies only its Zx file to a
download folder, and checks embedded images, installation, native pixel parity,
persistent click state, keyboard/Unicode input, malformed files and missing
capabilities on private displays. All four starter apps also have their Zx
pixels compared with native output.

`kryon install` also installs a Zx profile: its portable program goes to
`share/kryon/<bin>/app.zx`, and a reusable native player at `bin/<bin>` launches
it. The usual desktop menu and autostart settings apply. Embedded files need
no source checkout after installation.

### Apps shared between harnesses

An app can import `kryon/app_context` and `kryon/app_protocol` instead of a
product's host API. On `APP_IDENTIFY` it calls `AppIdentity(id, minimum_api)`
and `AppDescribe(name, icon)`, then returns `APP_PROTOCOL`. It draws hosted
Kryon widgets on `APP_FRAME`, handles its own state, and saves pending changes
on `APP_SAVE`. A supervising harness supplies `AppContext`, its service
allowlist and callbacks from `kryon/app_context_host`. Identification cannot
read or write user data. A bundle requiring a newer interface is rejected
before its first frame.

`AppSupports(name)` queries optional services; `AppService(name, request)`
returns `{"status":"unavailable"}` when the harness does not grant one.
`AppSupports("kryon.input.desktop")` is true when the device's main input is
a keyboard and mouse: native desktop hosts answer yes, Android and the web
no, and a harness that runs inside another host passes the question on.
Older hosts answer no, so an app keeps its touch behaviour there.
An app that imports `kryon/app_theme` follows the host's theme:
`AppThemeColor(token, fallback)` returns the host stylesheet's colour for
`canvas`, `surface`, `border`, `text`, `accent` or `link`, and `fallback` for
a token the host lacks or on a host older than the module.
An app with a look of its own, such as a game, imports `kryon/widget_palette`
instead: `WidgetPaletteUse(session, palette)` asks the host to draw the
widgets that follow in that palette's colours (as `SystemPalette`), until the
host's frame for the app ends. A host that is itself a bundle passes the
palette on to the host that draws, so it needs a host with the call.
Imports alone do not grant storage, network, clipboard or secret access.
Hosts keep those permissions and account boundaries. `DocumentRead`,
`DocumentStatus` and `DocumentWrite` distinguish missing files from read
errors; an app must preserve a document it cannot read.

The desktop player runs this contract directly. Its local provider uses
private documents under `$XDG_DATA_HOME/kryon/apps/<id>` (or
`~/.local/share/kryon/apps/<id>`), exclusive app-directory locking, atomic
writes, the clipboard, embedded locales and a desktop image picker. Set
`KRYON_APP_DATA` to choose another private app directory. Harnesses can
provide their own account-scoped storage and sync through the same calls.
Local documents do not automatically sync between different harnesses.

For example, Inner Breeze's `diary.zx` runs in its original harness,
Werkzeug's `werk --bundle diary.zx`, or `kryon run diary.zx`; it does not
load `inbe.zx`. Werkzeug's Calculator uses the same interface. Apps that
need device secrets or timed clipboard ownership still require a harness
that supplies those capabilities. The common player currently has a Linux
desktop provider; hosted app release watching is not supported yet.

## Installing

`kryon install` (or `ziran install`) builds one profile and installs it for
the current user:

```toml
[package]
tool = "kryon"

[install]
bin = "example"

[tool.kryon.install]
profile = "desktop"            # default: default_profile
name = "Example"               # menu and window name; default: package name
comment = "What it does"
icon = "assets/icon.png"
categories = "Utility;"
working_directory = "."        # start in the project directory
autostart = true               # also start at login
autostart_env = "EXAMPLE_HIDDEN=1"
```

Every window and browser page is named after the app: this `name`, or else
the package name. A build keeps it beside the program as
`build/NAME-PROFILE.title`, an install as `share/kryon/example/app.title`,
and a page in its HTML `<title>`. A ZX player names a bundle by the `.title`
beside it or by its file name, and a program without a title by its own file
name.

The program is copied to `~/.local/bin/example` (or `--prefix DIR`). The copy
replaces the old file in one step, so a running copy is never touched and
later builds in `build/` do not change the installed program. The icon goes to
`share/pixmaps/`, the menu entry to `share/applications/example.desktop`, and
with `autostart` the login entry to `~/.config/autostart/example.desktop`.
Setting `autostart = false` removes an autostart entry that `ziran install`
wrote earlier. Browser profiles cannot be installed.

The raylib profile builds its static raylib dependency in Kryon's `build/`
directory. The Kryon host chooses and owns that backend. Game code belongs in
the separate Game2D package; applications using both should rely on Kryon's
selected backend and raylib revision.

The project binary is `build/<package-name>-<profile>`. Checked Ziran modules
are saved in `build/generated/<profile>/ir/` as `.zir`; generated C is in
`build/generated/<profile>/c/`. `ziran inspect FILE.zir` displays a saved
module.

Canvas and DOM projects use short host modules such as `canvas_run.zir`,
`dom_run.zir`, and `canvas_raster.zir`. Their generator roots the selected
host plus `canvas_raster.zi` and emits every saved module; this preserves the
host entry point and raster adapters that the paint queue reaches indirectly.
Native profiles keep the package-qualified entry-pruned route.

Every host draws text in Kryon's font, Liberation Sans
(`assets/fonts/LiberationSans-Regular.ttf`): a `font`-pixel line box from the
top of the ascent to the bottom of the descent, glyphs at their unhinted
widths. The desktop host loads it from `KRYON_FONT_PATH`, which
`kryon run` sets, else from the system's copy. The `pixmap` host rasterizes
the font's outlines with integer arithmetic, so every Ziran target prints the
same bytes for the same frame, and its text matches the desktop window's. It
renders two frames and prints the second, for screenshots in documentation and
CI without a display. `make pixmap-parity-test` builds every template and
example for C, Go, and the portable runner and checks that
all three images match; `build/pixmap-parity/grid.png` shows them side by side.
`tools/image_tools.zi`, compiled to C, compares two PPM images, converts
them to PNG, and lays out such grids.

Kryon's `make desktop-project-test` and `make raylib-project-test` exercise the graphical hosts on private Xvfb
displays, including keyboard and Unicode text input. `make canvas-project-test`
builds a real Canvas2D project and loads it in private headless Chromium.
`make dom-project-test` builds a real semantic DOM project and checks its
mounted tags and attributes in another private headless Chromium process. Do
not run graphical tests on the developer's live display.

### Application-owned hosts

A profile can name an application's own Ziran host when it supplies additional
platform capabilities, such as private storage or clipboard handling:

```toml
[tool.kryon.profiles.desktop]
backend = "desktop"
host = "src/desktop.zi"
```

The host exports `main` and imports the application's widgets and platform
modules. Kryon builds and runs that program with the profile's backend libraries.
The host must be a `.zi` file inside the project, including when reached through
a symlink. Profiles without `host` use Kryon's standard host.

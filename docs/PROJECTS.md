# Ziran projects with Kryon

Kryon is a Ziran package. Applications compose its widgets in TSX using Ziran's
native TypeScript frontend. The selected host runs the application's frame loop.

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
| `web` | page: headings, paragraphs, links, a button | `web` (Canvas2D), `desktop` |

Each template's `src/app.tsx` uses the same `Frame` across its profiles. The
frame's viewport measures pixels in a window or browser and character cells on
the terminal host. `kryon init --template tui` adds a template to an existing package:
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
entry = "src/app.tsx"
module_roots = ["src"]
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
`ziran install` the same as the `kryon` commands. The selected UI entry must
end in `.tsx`. Use `[package] entry` for an application, or set
`[tool.kryon] entry = "src/window.tsx"` when the package also has a separate
command-line entry. A program entry exports `main` and passes its frame callback
to `App.Run`. Export that callback as `Frame` to support `kryon run --png`.

```tsx
import * as App from "kryon/app";
import { Session } from "kryon/session";
import { Text } from "kryon/tsx/text";

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Text session={session} props={{text: "Hello, world!", size: s32(32), bounds: frame.viewport}}/>;
}

export function main(): s32 {
    return App.Run({title: "Hello", width: s32(320), height: s32(160)}, Frame);
}
```

Style sheets, charts, tables, trees, and source coloring are separate Git
packages: add `https://github.com/kryonlabs/css.git`,
`https://github.com/kryonlabs/plot.git`,
`https://github.com/kryonlabs/data-views.git`, or
`https://github.com/ziranlang/syntax.git` with `ziran add` and import
`css/Css`, `plot/tsx/charts`, `data_views/tsx/table_view`,
`data_views/tsx/tree_view`, or `syntax/Syntax`. Kryon exports `geometry`,
`drawing_props`, and the other building blocks listed in `ziran.toml` for code that needs those primitives
without importing the widget catalog. Native and browser profiles resolve
imports the same way, so an app's Git dependencies and package-qualified
named imports such as `kryon/tsx/text` work in Canvas builds too.

From the app directory, run:

```sh
kryon check
kryon build
kryon run desktop            # or: kryon run --profile desktop, ziran run desktop
kryon profiles               # the profiles, their hosts, and the default
kryon run desktop --png build/shot.png --width 390 --height 844 --theme dark
kryon run --watch            # rebuild and relaunch after every saved change
```

`kryon run [PROFILE] --watch` builds a desktop or terminal C99 profile into
`build/watch/PROFILE`, runs it, and polls the project's files (all but
`build/` and hidden entries) four times a second. A change stops the app,
rebuilds and runs it again; a failed build waits for the next change. The
app shares kryon's process group, so Ctrl-C stops both.

`kryon run [PROFILE] --png FILE.png` renders the app's own entry offscreen,
with no display and no preview fixture, and writes `FILE.png` and its widget
tree `FILE.json` (the [preview JSON](FRAME_REPLAY.md#native-fixture-preview)).
`--width` and `--height` take 1–4096 pixels (960 × 600 by default); a comma
list of widths writes `FILE-PX.png` per width. `--theme light|dark` picks the
palette (light by default). The named profile must be a desktop C99
profile, and the entry must export `Frame`; the desktop renderer draws the
picture, so it matches the window. The capture build lives in
`build/shot/PROFILE`.

Commit `ziran.lock` to pin exact package and toolchain revisions. `ziran
fetch` fills the shared cache. For local development, `ziran.local.toml` can
override the Kryon and Ziran roots; it is not the published dependency
manifest. See the [Ziran package
guide](https://github.com/ziranlang/ziran/blob/master/docs/PACKAGES.md)
for lock, cache, and override details.

## Profiles and hosts

### Foreign renderer artifacts

Builders can request Kryon's foreign libraries and standard resources through
the generic Ziran artifact API:

```sh
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
Linux desktop builds use SDL2, Cairo, FreeType, Pango, Fontconfig and GIO.

`renderer-web` supplies `web/ziran_web.js` from the selected Ziran toolchain,
`js_libraries`, browser link flags and `preload_files` entries (`path`, `mount`)
for the standard font at `/kryon-font.ttf`. Feed these to Emscripten's
`--js-library` and `--preload-file` arguments. Game resources remain separate.

Widget and renderer Ziran code remains ordinary `kryon/...` imports. These
artifacts do not provide a separately compiled Ziran component ABI: the current
generic artifact API stages provider files but does not define such a language
component interface. Builders must not rewrite generated C/H or reconstruct a
private module/object list to manufacture one.

Define each profile under `[tool.kryon.profiles.NAME]`. It requires `backend`;
`codegen` defaults to `c99`. Supported backends are `terminal`, `desktop`,
`canvas`, and `android`. Set `default_profile` under `[tool.kryon]` when there
are several profiles. `--profile NAME` selects another. `run` asks for a
profile when there are several, none is specified, and it runs in a
terminal; otherwise it uses the default.

| Backend | Host | Requirements |
| --- | --- | --- |
| `terminal` | 80 × 24 terminal cells | TTY for interactive input; redirected output renders once |
| `desktop` | SDL2 window with Cairo raster | SDL2, Cairo, and FreeType development libraries |
| `canvas` | Canvas2D browser page | Emscripten with Asyncify; see [Canvas host](canvas.md) |
| `android` | installable APK through a generated `droid/` Gradle project and direct NativeActivity host | a JDK, the Android SDK with NDK `28.2.13676358` and CMake `3.22.1`, the locked codec source packages, and `[tool.kryon.android] package`; see [Android host](BACKENDS.md#android-host) |

The SDL desktop host presents its already rasterized pixels through the
software renderer and an unaccelerated window surface by default. Custom
SDL pixel hosts can import `kryon/pixmap_sdl` and call
`CreatePixmapRenderer(window)` on the window's owner thread before creating
their texture. The host retains ownership of its window, renderer, texture
and input loop; its raster workers are independent of this presentation choice.
Explicit `SDL_RENDER_DRIVER` and `SDL_FRAMEBUFFER_ACCELERATION` environment
preferences, or normal/override priority SDL hints, take precedence. The
helper leaves other hosts and their graphics APIs untouched.

Such a host can also paint on the GPU by importing `kryon/pixmap_sdl_gpu`.
`PixmapGpuOpen(renderer)` records the same paint commands as textured triangles,
including text and terminal cells through a glyph atlas, and returns false on a software renderer or software GL
driver, where the host keeps uploading pixels. While it is on, create the
presented texture with `PixmapGpuCreateTarget(width, height)` and call
`PixmapGpuDraw()` instead of uploading damage, then copy and present as usual.
`KRYON_PIXMAP_GPU=0` keeps CPU painting; `=1` also uses a software GL driver.

For a custom **SDL3** host, the experimental exports are `kryon/pixmap_sdl3`
and `kryon/pixmap_sdl3_gpu`; they require an SDL3 window and explicit `gpu`
renderer. There is no `backend = "sdl3"` profile. From an application's package
graph, request the pinned SDK with:

```sh
ziran pkg artifact kryon:native-sdl3 --platform linux-x86_64-gnu --locked
```

Use `--locked` when there is no `ziran.local.toml`. With local overrides, run
`ziran lock` after the overridden package advances before requesting artifacts.
The native artifact is
currently Linux x86_64 GNU only. Verify the returned descriptor with Ziran's
`Artifact` API; `Artifact.link_flags()` supplies the ordered static linker
arguments. Verify and extract `Artifact.output("resources/SDL3-headers.tar")`
into the consumer's build area for the complete include directory. The loose
SDK headers are not all declared outputs, so use the verified archive for
compilation. This SDK is native support for custom hosts, not a replacement
renderer selection for an existing desktop profile. See
[experimental SDL3 usage and tests](BACKENDS.md#experimental-sdl3-gpu-renderer).

These settings live only in `ziran.toml`. Kryon declares them under
`[options]` in its own `ziran.toml`. Ziran checks the application's
`[tool.kryon]` tables against that declaration, merges `ziran.local.toml` over
them, and hands the kryon tool the result, so a typo is reported with its file
and line. To try another default profile on one machine, set it in the ignored
`ziran.local.toml`:

Native C profiles also link every system library declared in the root
`[package].links` list, in manifest order. Use system basenames such as
`links = ["gtk-3", "gdk-3", "gio-2.0", "gdk_pixbuf-2.0"]`; the build uses the
system linker's normal search paths. Portable and browser profiles do not
consume this list.

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
native_artifacts = "kryon:renderer-desktop"
native_platform = "linux-x86_64-gnu"
```

This adds the desktop codec support and resources to the SDL/Cairo profile.
`kryon:renderer-desktop` decodes AVIF with dav1d only; an app that compresses
AVIF selects `kryon:renderer-desktop-encode` instead, which adds libaom.
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

Desktop, Canvas and Android profiles can package an ordinary `Frame`
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

An Android profile with `codegen = "zx"` builds its portable guest first,
then packages `app.zx`, the VM and the NativeActivity host into the APK. Its
`Frame` is not compiled into native application code. The canonical scaffold
handles display density, safe-area insets, touch, hardware/soft-keyboard input
and graphics restoration without resetting bundle state. It also registers a
ZX file opener: Android document URIs are copied into private cache and
validated before replacing the running app. Invalid files retain the current
instance. `make zx-android-test` builds ARM64, ARMv7 and x86-64 shells. Runtime testing
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
profile. Each bundle embeds the gallery's assets, so the website runs the
examples live on its Components page rather than publishing these files.
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
After downloading that file, `ziran run app.zx` works without the app's source,
manifest, lock, or compiler-generated native code. Install the player with
`make install-user` in Kryon; this also registers the provider with Ziran.
The player needs SDL2, Cairo and FreeType. `kryon run app.zx` forwards to the
same Ziran runner.
For a custom install prefix, add its `share/ziran/hosts` directory to
`ZIRAN_HOST_PATH` or its `share` directory to `XDG_DATA_DIRS`.

`assets` is an optional project-relative directory or file, embedded under that
same path. Images retain their ordinary `Image.Props.asset_path` values. The
player extracts validated resources into a private temporary directory and
removes it when the app exits. Exports rebuild resource content on every build,
including file removals.

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
An app that imports `kryon/app_theme` follows the host's theme:
`AppThemeColor(token, fallback)` returns the host stylesheet's colour for
`canvas`, `surface`, `border`, `text`, `accent` or `link`, and `fallback` for
a token the host lacks.
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
Werkzeug's `werk --bundle diary.zx`, or `ziran run diary.zx`; it does not
load `inbe.zx`. Werkzeug's Calculator uses the same interface. Apps that
need device secrets or timed clipboard ownership still require a harness
that supplies those capabilities. The common player currently has a Linux
desktop provider.

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
`share/pixmaps/`. To use the desktop's icon theme instead, set
`icon_name = "system-file-manager"` and omit `icon`; the launcher stores that
theme name and copies no asset. The two options are mutually exclusive.
The menu entry goes to `share/applications/example.desktop`, and
with `autostart` the login entry to `~/.config/autostart/example.desktop`.
Setting `autostart = false` removes an autostart entry that `ziran install`
wrote earlier. Browser profiles cannot be installed.

The project binary is `build/<package-name>-<profile>`. Checked Ziran modules
are saved in `build/generated/<profile>/ir/` as `.zir`; generated C is in
`build/generated/<profile>/c/`. `ziran inspect FILE.zir` displays a saved
module.

Canvas projects resolve package-qualified hosts and bind the selected
source through the shared frame capability. Their generator also roots
`canvas_raster.zi` and lowers every saved module, preserving raster adapters
and browser callbacks. Native profiles select the package-qualified host
entry as well. The saved IR carries the frame binding into native and portable
builds.

Every host draws text in Kryon's font, Liberation Sans
(`assets/fonts/LiberationSans-Regular.ttf`): a `font`-pixel line box from the
top of the ascent to the bottom of the descent, glyphs at their unhinted
widths. The desktop host loads it from `KRYON_FONT_PATH`, which
`kryon run` sets, else from the system's copy. Offscreen screenshots use
`kryon run desktop --png FILE.png` and the same desktop renderer as the window.
`tools/image_tools.zi`, compiled to C, compares two PPM images, converts
them to PNG, and lays out such grids.

Kryon's `make desktop-project-test` exercises the graphical host on private Xvfb
displays, including keyboard and Unicode text input. `make canvas-project-test`
builds a real Canvas2D project and loads it in private headless Chromium. Do
not run graphical tests on the developer's live display.

### Application-owned hosts

A profile can name an application's own Ziran host when it supplies additional
platform capabilities, such as private storage or clipboard handling:

```toml
[tool.kryon.profiles.desktop]
backend = "desktop"
host = "src/desktop.zi"
```

The host exports `main` and imports the application's TSX frame and the
platform modules it needs. Kryon builds and runs that program with the profile's
backend libraries.
The host must be a `.zi` file inside the project, including when reached through
a symlink. Profiles without `host` use Kryon's standard host.

## Native asset directories

A native profile's `assets = "assets"` ships that directory's contents beside
its built executable as `PROGRAM.assets/`. `ziran install` copies the contents
to `PREFIX/share/kryon/BIN/assets/`. The install keeps no dependency on the
source checkout or its working directory. Undeclaring assets removes only the
product's managed snapshot on the next build/install.

Packaged asset directories use mode 755 and data files (including the prefix
marker) use 644, even with a private source mode or caller umask. The install
creates missing product directories with mode 755 and executable files with 755;
desktop entries, icons and title metadata use 644. Existing directories are
never made more permissive: callers wanting a shared installation must supply
a prefix whose existing ancestors allow users to traverse it. An existing
private prefix remains private. Custom installers can use
`scripts/make-install-dirs.sh DIR...` and
`scripts/copy-native-assets.sh SOURCE DESTINATION ORIGINAL_PREFIX` for the same
directory-boundary and asset snapshot behavior.

Native Image and ReadAppAsset lookup first preserves an accessible literal
relative file and every absolute path. Packaged lookup then accepts paths
relative to the declared directory (`icons/ui.webp`) and its original project
prefix (`assets/icons/ui.webp`). Missing files keep their original path/error
behavior. Paths containing parent traversal or drive names are never redirected.
Web preloads, ZX bundled assets and Android APK lookup retain their existing routes.

Custom native hosts can import `kryon/app_asset_path` and call
`ResolveAppAsset(path: string, output: []u8) -> string`. Use a separate 4096-byte
buffer: a resolved path borrows it; an unchanged path borrows the original text.
`ResolveAppAssetForExecutable(path, executable, output)` supplies an explicit
executable for hosts/tests. Neither changes the process working directory.
The standard native image loaders use this resolver automatically;
a custom asset-reader callback still has exclusive ownership of its resources.

A host embedding another UI packages that UI's referenced files under its own
declared asset directory, retaining their logical names. For example, an embedded
Shelf's `ImageForIcon` needs `icons/ui.webp` under the Rill host's declared assets;
Rill's executable and install prefix determine the runtime location.

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

Define each profile under `[tool.kryon.profiles.NAME]`. It requires `backend`;
`codegen` defaults to `c99`. Supported backends are `terminal`, `desktop`,
`libdraw`, `raylib`, `canvas`, `dom`, and `pixmap`. Set `default_profile` under `[tool.kryon]` when there
are several profiles. `--profile NAME` selects another. `run` asks for a
profile when there are several, none is specified, and it runs in a
terminal; otherwise it uses the default.

| Backend | Host | Requirements |
| --- | --- | --- |
| `terminal` | 80 × 24 terminal cells | TTY for interactive input; redirected output renders once |
| `desktop` | SDL2 window with Cairo raster | SDL2, Cairo, and FreeType development libraries |
| `libdraw` | plan9port devdraw with Cairo | Cairo and plan9port: the one `$PLAN9` names, or `ziran add https://github.com/9fans/plan9port.git --source`, which Kryon then builds |
| `raylib` | raylib SDL2/OpenGL ES 2 window | the locked raylib source package and SDL2, DRM, EGL, GLESv2 |
| `canvas` | Canvas2D browser page | Emscripten with Asyncify; see [Canvas host](canvas.md) |
| `dom` | semantic DOM page | Emscripten with Asyncify; see [DOM host](dom.md) |
| `pixmap` | headless 960 × 600 frame printed as a PPM image | nothing; `kryon run shot > frame.ppm` |

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

```toml
[tool.kryon]
default_profile = "tui"
```

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
name = "Example"               # menu name; default: package name
comment = "What it does"
icon = "assets/icon.png"
categories = "Utility;"
working_directory = "."        # start in the project directory
autostart = true               # also start at login
autostart_env = "EXAMPLE_HIDDEN=1"
```

The program is copied to `~/.local/bin/example` (or `--prefix DIR`). The copy
replaces the old file in one step, so a running copy is never touched and
later builds in `build/` do not change the installed program. The icon goes to
`share/pixmaps/`, the menu entry to `share/applications/example.desktop`, and
with `autostart` the login entry to `~/.config/autostart/example.desktop`.
Setting `autostart = false` removes an autostart entry that `ziran install`
wrote earlier. Browser profiles cannot be installed.

The native Plan 9 libdraw raster is not a project profile yet. Rill uses it
through its Ziran application host and native `app/*.mk` recipes. The
display-free `libdraw-native-plan9-test` gate checks its ABI, while Taiji's
private native gate checks source/saved-IR screen rendering, PNG assets,
native Plan 9 images, and the actual Rill executables. Held modifier input
and selection through a project profile remain pending.

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
widths. The desktop and libdraw hosts load it from `KRYON_FONT_PATH`, which
`kryon run` sets, else from the system's copy. The `pixmap` host rasterizes
the font's outlines with integer arithmetic, so every Ziran target prints the
same bytes for the same frame, and its text matches the desktop window's. It
renders two frames and prints the second, for screenshots in documentation and
CI without a display. `make pixmap-parity-test` builds every template and
example for C, C++, Go, Rust, Python, and the portable runner and checks that
all six images match; `build/pixmap-parity/grid.png` shows them side by side.
`tools/image_tools.zi`, built for Python, compares two PPM images, converts
them to PNG, and lays out such grids.

Kryon's `make desktop-project-test`, `make raylib-project-test`, and
`make libdraw-project-test` exercise the graphical hosts on private Xvfb
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

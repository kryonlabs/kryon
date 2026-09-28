# Ziran projects with Kryon

Kryon is a Ziran package. Application source imports its public modules, and
the selected Kryon host links the application's `Frame` function. No UI syntax
or widget handling is built into the Ziran compiler.

## Package manifest

Install the Ziran command from the root Ziran repository with
`make install-user`. In an application, create `ziran.toml`:

```toml
[package]
name = "Example"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]

[toolchain]
git = "https://github.com/ziranlang/ziran.git"
ref = "master"

[dependencies.Kryon]
git = "https://github.com/kryonlabs/kryon.git"
ref = "master"

[tool.Kryon]
default_profile = "desktop"

[tool.Kryon.profiles.desktop]
backend = "desktop"
```

`bridge_modules` exposes the app's `app` module to the selected host. The app
imports `Kryon` for the small core surface or `Widgets` for all core widgets:

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

Other public imports are `PlotWidget`, `TableView`, `TreeView`, `Kss`, and
`Syntax`. Each is optional. Kryon exports `geometry` and `drawing_props` for
code that needs those primitives without importing the widget catalog.

From the app directory, run:

```sh
ziran lock
ziran tool Kryon check
ziran tool Kryon build
ziran tool Kryon run --profile desktop
```

Commit `ziran.lock` to pin exact package and toolchain revisions. `ziran
fetch` fills the shared cache. For local development, `ziran.local.toml` can
override the Kryon and Ziran roots; it is not the published dependency
manifest. See the [Ziran package
guide](https://github.com/ziranlang/ziran/blob/master/docs/PACKAGES.md)
for lock, cache, and override details.

## Profiles and hosts

Define each profile under `[tool.Kryon.profiles.NAME]`. It requires `backend`;
`codegen` defaults to `c99`. Supported backends are `terminal`, `desktop`,
`libdraw`, `raylib`, `canvas`, and `dom`. Set `default_profile` under `[tool.Kryon]` when there
are several profiles. `--profile NAME` selects another. `run` asks for a
profile when there are several and none is specified.

| Backend | Host | Requirements |
| --- | --- | --- |
| `terminal` | 80 × 24 terminal cells | TTY for interactive input; redirected output renders once |
| `desktop` | SDL2 window with Cairo raster | SDL2 and Cairo development libraries |
| `libdraw` | plan9port devdraw with Cairo | plan9port and Cairo; set `PLAN9PORT_DIR` if needed |
| `raylib` | raylib SDL2/OpenGL ES 2 window | the locked raylib source package and SDL2, DRM, EGL, GLESv2 |
| `canvas` | Canvas2D browser page | Emscripten with Asyncify; see [Canvas host](canvas.md) |
| `dom` | semantic DOM page | Emscripten with Asyncify; see [DOM host](dom.md) |

These settings live only in `ziran.toml`. Kryon declares them under
`[options]` in its own `ziran.toml`. Ziran checks the application's
`[tool.Kryon]` tables against that declaration, merges `ziran.local.toml` over
them, and hands the kryon tool the result, so a typo is reported with its file
and line. To try another default profile on one machine, set it in the ignored
`ziran.local.toml`:

```toml
[tool.Kryon]
default_profile = "tui"
```

## Installing

`ziran install` builds one profile and installs it for the current user:

```toml
[install]
tool = "Kryon"
bin = "example"

[tool.Kryon.install]
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

The native Plan 9 libdraw raster is not a project profile yet; it is covered
by the display-free `libdraw-native-plan9-test` ABI gate while its input and
presentation surface are completed.

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

Kryon's `make desktop-project-test`, `make raylib-project-test`, and
`make libdraw-project-test` exercise the graphical hosts on private Xvfb
displays, including keyboard and Unicode text input. `make canvas-project-test`
builds a real Canvas2D project and loads it in private headless Chromium.
`make dom-project-test` builds a real semantic DOM project and checks its
mounted tags and attributes in another private headless Chromium process. Do
not run graphical tests on the developer's live display.

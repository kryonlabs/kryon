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

[tool.kryon]
default_profile = "desktop"

[tool.kryon.profiles.desktop]
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

Define each profile under `[tool.kryon.profiles.NAME]`. It requires `backend`;
`codegen` defaults to `c99`. Supported backends are `terminal`, `desktop`,
`libdraw`, `raylib`, `canvas`, and `dom`. Set `default_profile` under `[tool.kryon]` when there
are several profiles. `--profile NAME` selects another. `run` asks for a
profile when there are several and none is specified.

| Backend | Host | Requirements |
| --- | --- | --- |
| `terminal` | 80 × 24 terminal cells | TTY for interactive input; redirected output renders once |
| `desktop` | SDL2 window with Cairo raster | SDL2 and Cairo development libraries |
| `libdraw` | plan9port devdraw with Cairo | plan9port and Cairo; set `PLAN9PORT_DIR` if needed |
| `raylib` | raylib SDL2/OpenGL ES 2 window | Kryon's raylib submodule and SDL2, DRM, EGL, GLESv2 |
| `canvas` | Canvas2D browser page | Emscripten with Asyncify; see [Canvas host](canvas.md) |
| `dom` | semantic DOM page | Emscripten with Asyncify; see [DOM host](dom.md) |

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

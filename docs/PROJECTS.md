# Ziran projects

## Package route

Install Ziran once with `make install-user` in a Ziran checkout. An app then
declares its compiler, Kryon dependency, and Kryon profile in one
`ziran.toml`:

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
default_profile = "cairo"

[tool.kryon.profiles.cairo]
backend = "desktop"
```

`ziran lock` writes exact Git commits to `ziran.lock`; commit that file with
the app. `ziran fetch` fills the shared cache. App source imports the public
module with `using UI :: #import "Kryon";` and exports
`Frame(session: Session, viewport: Rectangle) -> s32`. Run
`ziran tool Kryon check`, `ziran tool Kryon build`, or
`ziran tool Kryon run --profile cairo` from the app directory. The Cairo
profile needs SDL2 and Cairo development libraries. Project builds can use
`--locked` and `--offline` for exact, cached dependencies. Plot is the first
app using this route and also exports its own `Plot` module.

The compiler resolves short imports inside the importing package and its
direct dependencies. Kryon's host imports the app's `app` module through the
explicit `bridge_modules` entry. No app submodule or sibling checkout is
needed for this profile. See [Ziran packages](https://github.com/ziranlang/ziran/blob/master/docs/PACKAGES.md)
for aliases, multiple dependency revisions, and local development overrides.

## Legacy checkout route

Install the `kryon` command from a Kryon checkout next to a Ziran checkout:

```sh
make install-user
```

This puts a launcher at `~/.local/bin/kryon`. The launcher builds the command
from the current Kryon and Ziran source before it runs. Add `~/.local/bin` to
`PATH` if needed. Project builds also read the local Kryon UI and terminal host
sources, so editing either checkout updates the next build. No network fetch is
performed.

Put a `kryon.toml` at the root of a Ziran app:

```toml
[profiles.tui]
backend = "terminal"

[profiles.desktop]
backend = "raylib"
```

The directory name becomes the app name. The entry defaults to `src/app.zi`,
the paths default to `../kryon` and `../ziran`, and codegen defaults to `c99`.
A sole profile becomes the default; with several profiles, set
`default_profile` under `[project]` for `build` and `check`. When `run` sees
several profiles and no `--profile`, it asks you to choose a number.
You may override `name`, `entry`, and
`default_profile` under `[project]`, `kryon` and `ziran` under `[paths]`, and
`codegen` under each profile. Paths are relative to the app directory.

The entry imports Kryon UI modules
and exports `Frame(session: Session, viewport: Rectangle) -> s32`. The selected host owns `main`,
frame boundaries, platform effects, and presentation. App code can stay
independent of its backend. A `TextProps` value needs only its `text` field for
content at the origin; key, measured size, and wrap use widget defaults.

From the app directory, use `kryon run`, `kryon build`, or `kryon check`.
`kryon run --profile tui` selects a named profile without prompting. A `Makefile` may simply forward `run`,
`build`, and `check` to these commands.

The project route supports `terminal`, `desktop`, `libdraw`, and `raylib` with C99 code
generation. On a terminal, the terminal host keeps its 80 by 24 cell display
active until Ctrl-C and sends key presses to the app through `KeyboardTake`.
When standard input or output is redirected, it writes one frame and exits.
The `desktop` host opens a 960 by 600 SDL2/Cairo window; it requires the SDL2
and Cairo development packages. The `libdraw` host opens a plan9port/devdraw
window, takes its viewport from the actual window size, and uses the shared
Cairo raster for text, shapes, and PNG assets. It needs Cairo and a built
plan9port checkout next to Kryon. Set `PLAN9PORT_DIR` to another plan9port
checkout when needed. `kryon run` starts devdraw from that checkout; a binary
launched directly needs `PLAN9`, `DEVDRAW`, and plan9port's `bin` on `PATH`.
`make libdraw-project-test` checks pixels from the real devdraw window, asset
rendering, keyboard and pointer input, and resize on private Xvfb displays.
The `raylib` host opens a resizable 960 by 600
raylib window. It implements the frame loop, keyboard, typed character,
pointer, and wheel input,
text, shapes, and asset-backed images in Ziran. The host uses raylib's SDL2
platform and OpenGL ES 2 renderer. Initialize Kryon's dependency with
`git -C ../kryon submodule update --init vendor/raylib` and install the SDL2,
libdrm, GBM, EGL, and GLESv2 development packages. The first raylib build
compiles a static library in Kryon's `build/` directory; later builds reuse it.
Games use the separate Game2D Ziran package. Its `Raylib` module provides the
raylib C ABI surface independently of Kryon's raylib UI host.
`make -C ../kryon raylib-project-test` checks a captured frame and keyboard,
typed character, pointer, and wheel input on private Xvfb displays.
Project output is `build/<app-name>-<profile>`. The build saves checked Ziran
modules in `build/generated/<profile>/ir/*.zir` before producing C99 source
by module in `build/generated/<profile>/c/`. The linker removes unreachable
declarations from the selected host entry; reachable runtime branches remain.
The `.zir` files are
binary.
Use `../ziran/build/bin/ziran inspect build/generated/ir/app.zir` to read a
saved module, or add `--hex` for its raw bytes.

The manifest parser accepts the tables and quoted string keys shown above,
plus additional `[profiles.NAME]` tables. Names use letters, digits, `_`, or
`-`; paths also permit `.` and `/`. Comments beginning with `#` are allowed.
String escapes and other TOML value types are not supported yet.

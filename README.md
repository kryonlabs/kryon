# Flint

Shared Raylib application tooling and UI primitives.

Flint has two related parts:

- a small C library of UI, layout, theme, text, icon, DPI, and file-dialog helpers for Raylib applications
- a `flint` CLI for building, running, packaging, and publishing Raylib apps that use a `flint.toml` project manifest

It is intended to keep Raylib app repositories small while moving common UI code and repeated platform build logic into one shared place.

## Features

- Color manipulation utilities
- DPI-aware scaling helpers
- Layout calculation functions
- Icon assets and fallback drawing
- Theme loading and metadata helpers
- Text rendering and text layout helpers
- Immediate-mode UI controls
- Native file dialog helpers
- Reusable Make rules for native, Linux, Windows, web, and Android Raylib builds
- `flint` CLI commands for project health checks, builds, runs, distribution packages, Android devices, and itch.io publishing

## Building

Build the C library:

```bash
make
```

This creates `libflint.a`, which can be linked by any Raylib project.

Build only the CLI:

```bash
make cli
```

Install the CLI with the same local install pattern used by Flint apps:

```bash
make install
flint help
```

This installs the binary to `~/.local/share/flint/flint` and creates `~/bin/flint`.

## CLI

The `flint` command is a thin project command runner for Raylib apps. It finds the nearest `flint.toml`, validates declared platforms, and delegates the actual platform work to Flint's shared Make rules.

Common commands:

```bash
flint doctor
flint version
flint devices
flint build native
flint build linux
flint build windows
flint build web
flint build android
flint build-all
flint run native
flint run web
flint run android --device <adb-device-id>
flint dist linux
flint dist windows
flint dist android
flint dist android-bundle
flint dist itch
flint dist
flint clean
```

Use `flint doctor` inside an app repo to confirm that the manifest and required tools are visible. For web runs, `flint run web` builds the web target and serves `build/web` on localhost. For Android runs, `flint run android` builds a debug APK, installs it with `adb`, and launches the configured activity.

When an app has `flake.nix` and Raylib environment variables are not already present, the CLI enters `nix develop` before running Make targets.

## Project Manifest

Apps driven by the CLI and Make layer use a `flint.toml` manifest. A typical app declares metadata, source files, assets, Raylib options, platforms, and optional distribution settings:

```toml
[app]
name = "example"
title = "Example"
id = "xyz.example.app"
version_header = "src/version.h"

[paths]
raylib = "vendor/raylib/src"
flint = "vendor/flint"
android_project = "droid"

[sources]
app = ["src/*.c"]
include_dirs = ["src"]

[assets]
images = "assets/images/*"
themes = "vendor/flint/themes/*.ini"

[raylib]
audio = true
models = false
graphics = "opengl33"
formats = ["png"]

[platform.linux]
arches = ["x86_64"]

[platform.windows]
arches = ["x86_64"]

[platform.web]
shell = "src/web_shell.html"

[platform.android]
min_sdk = 23
target_sdk = 35
compile_sdk = 35
activity = "xyz.example.app.MainActivity"
```

Only declare platforms that the app actually supports. CLI commands such as `flint build web` and `flint dist android` fail early when the corresponding platform section is missing.

## Web Viewports

For web builds, do not CSS-scale a fixed-size raylib canvas to fit the page.
That stretches the framebuffer and makes text blurry or clipped. Instead, size
raylib from the browser viewport and keep it synced when the viewport changes:

```c
int width = flint_web_viewport_width(320);
int height = flint_web_viewport_height(560);
SetConfigFlags(flint_web_window_flags());
InitWindow(width, height, "App");

while(!WindowShouldClose()) {
    if(flint_web_sync_window_size()) {
        flint_set_view_size(GetScreenWidth(), GetScreenHeight());
    }

    BeginDrawing();
    /* draw at GetScreenWidth()/GetScreenHeight() */
    EndDrawing();
}
```

Use viewport-filling CSS for the canvas shell, but let raylib own the backing
framebuffer size through `InitWindow()` and `SetWindowSize()`.

## Make Layer

Flint also ships reusable Make rules for Raylib applications in `mk/`.
An app can keep a small compatibility Makefile and delegate common build targets to Flint:

```make
.DEFAULT_GOAL := all

FLINT_DIR ?= vendor/flint
FLINT_PROJECT ?= flint.toml

include $(FLINT_DIR)/mk/project.mk
```

The make layer preserves direct targets such as `make`, `make linux`, `make windows`, `make web`, and the Android targets. The CLI uses the same targets, so app repositories can support both direct Make usage and the higher-level `flint` commands.

## C Library Usage

Include the Flint headers in your project:

```c
#include "flint.h"
#include "flint_color.h"
#include "flint_scaling.h"
#include "flint_layout.h"
#include "flint_icons.h"
```

Link against `libflint.a` in your Makefile.

## Projects Using Flint

- Inbe
- Quest
- Uku
- Lotus

## Name Origin

Flint is the rock that creates sparks - foundational, solid, and ignites interactions.

# Flint UI Library

Shared UI primitives for Raylib-based applications.

Flint provides common UI components and utilities that can be shared across multiple Raylib projects, eliminating code duplication while maintaining project independence.

## Features

- Color manipulation utilities
- DPI-aware scaling helpers
- Layout calculation functions
- Icon fallback drawing
- Theme management (coming soon)
- Button components (coming soon)
- Text rendering (coming soon)

## Building

Flint compiles to a static library (`libflint.a`) that can be linked against any Raylib project.

```bash
make
```

Install the Flint CLI with the same local install pattern used by Flint apps:

```bash
make install
flint help
```

This installs the binary to `~/.local/share/flint/flint` and creates `~/bin/flint`.

From a Raylib app that has `flint.toml`:

```bash
flint doctor
flint build native
flint run native
flint build web
flint run web
flint build android
flint run android --device <adb-device-id>
flint dist linux
flint dist windows
flint dist android
flint dist android-bundle
flint dist itch
flint dist
```

## Raylib App Make Layer

Flint also ships reusable Make rules for Raylib applications in `mk/`.
An app can keep a small compatibility Makefile and delegate common build targets to Flint:

```make
.DEFAULT_GOAL := all

FLINT_DIR ?= vendor/flint
FLINT_PROJECT ?= flint.toml

include $(FLINT_DIR)/mk/project.mk
```

The current make layer preserves the existing `make`, `make linux`, `make windows`,
`make web`, and Android targets while moving the shared platform build logic out
of app repositories. `flint.toml` is the forward-compatible project manifest for
the future CLI.

## Usage

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

## Name Origin

Flint is the rock that creates sparks - foundational, solid, and ignites interactions.

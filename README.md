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

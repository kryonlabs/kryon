# Kryon Hare Bindings

Hare bindings for the Kryon UI framework. These bindings allow you to write user interfaces in Hare using a declarative DSL.

## Features

- **Type-safe DSL**: Builder pattern with type checking
- **Direct C FFI**: Zero-cost bindings to the Kryon IR library
- **Reactive**: Built-in support for reactive state management (coming soon)
- **High Performance**: Compiled to native code with no runtime overhead

## Prerequisites

- Hare compiler (latest stable from https://harelang.org/)
- Kryon IR library (libkryon_ir.so)
- SDL3 and Raylib (for desktop rendering)

## Installation

1. Install the Kryon library:
```bash
cd kryon
make -C cli install
```

This installs:
- `~/.local/lib/libkryon_ir.so` - The IR library
- `~/.local/share/kryon/bindings/hare/` - Hare bindings

## Usage

### Basic Example

```hare
use kryon::*;

export fn main() void = {
    // Initialize context
    init()!;

    // Build UI using the DSL
    const root = container { b => {
        b.width("100%");
        b.height("100%");
        b.background_color("#1a1a1a");

        b.child(column { c => {
            c.gap(20);
            c.align_items("center");
            c.justify_content("center");

            c.child(text { t => {
                t.text("Hello from Hare!");
                t.font_size(32);
                t.font_weight(700);
                t.color("#ffffff");
            }});

            c.child(button { btn => {
                btn.text("Click me");
                btn.width("200px");
                btn.height("50px");
            }});
        }});
    }};

    // Set as root and run
    set_root(root)!;
    run();
    cleanup();
};
```

### Building and Running

Compile your Hare program:
```bash
hare build -o myapp main.ha
```

Link against the Kryon library:
```bash
export LD_LIBRARY_PATH=$HOME/.local/lib:$LD_LIBRARY_PATH
./myapp
```

Or statically link:
```bash
hare build -o myapp -L $HOME/.local/lib -lkryon_ir main.ha
```

## API Reference

### Components

#### `container(fn: fn(*container_builder) void) *c::void`
Creates a container component.

#### `row(fn: fn(*row_builder) void) *c::void`
Creates a row (horizontal flex container).

#### `column(fn: fn(*column_builder) void) *c::void`
Creates a column (vertical flex container).

#### `text(fn: fn(*text_builder) void) *c::void`
Creates a text component.

#### `button(fn: fn(*button_builder) void) *c::void`
Creates a button component.

#### `input(fn: fn(*input_builder) void) *c::void`
Creates a text input component.

### Common Properties

All builders support these common methods:

- `width(value: str)` - Set width (e.g., "100px", "50%", "auto")
- `height(value: str)` - Set height
- `padding(value: u32)` - Set padding in pixels
- `background_color(value: str)` - Set background color (CSS format)
- `child(child: *c::void)` - Add a child component

### Flex Properties (Row/Column)

- `gap(value: u32)` - Set gap between children
- `align_items(value: str)` - "start", "center", "end", "stretch"
- `justify_content(value: str)` - "start", "center", "end", "space-between"

### Text Properties

- `text(value: str)` - Set text content
- `font_size(value: u32)` - Set font size in pixels
- `font_weight(value: u32)` - Set font weight (100-900)
- `color(value: str)` - Set text color
- `text_align(value: str)` - "left", "center", "right", "justify"

### Runtime Functions

#### `init() (*c::void | void)`
Initialize the Kryon context. Must be called before any other functions.

#### `create_app(width: u32, height: u32, title: str) (*c::void | void)`
Create a new application window.

#### `set_root(root: *c::void) (void | c::int)`
Set the root component of the application.

#### `run() void`
Start the application main loop.

#### `quit() void`
Exit the application.

#### `cleanup() void`
Clean up resources.

## Code Generation

You can also generate Hare code from KRY source files:

```bash
# Generate Hare code from .kry file
kryon codegen hare

# Generate from specific input
kryon codegen hare app.kir output/
```

This generates idiomatic Hare code using the DSL functions.

## Architecture

The Hare bindings are organized into several modules:

### `ffi.ha`
Direct C FFI bindings to the Kryon IR library. Uses Hare's `@cfunc` and `@symbol` attributes to call C functions.

### `types.ha`
Type definitions for components, properties, and other data structures.

### `dsl.ha`
High-level DSL functions for building UI components using the builder pattern.

### `runtime.ha`
Runtime functions for application lifecycle (init, run, cleanup).

### `init.ha`
Main entry point that re-exports all public APIs.

## Memory Management

Hare is a systems language with manual memory management:

- Components are created by the IR library and managed internally
- Use `cleanup()` at the end of your program to free resources
- The IR library handles component lifecycle
- Strings are converted between Hare and C using helper functions

## Limitations

- Event handlers (on_click, on_change) are not yet implemented (needs closures)
- Reactive state management is not yet implemented
- Some advanced components are missing (tabs, canvas, etc.)
- Error handling uses Hare's error types, but could be improved

## Contributing

Contributions are welcome! The main areas for improvement:

1. Event handlers with proper closure support
2. Reactive state management
3. More component types
4. Better error messages
5. Documentation and examples

## License

MIT License - see LICENSE file in the root Kryon repository.

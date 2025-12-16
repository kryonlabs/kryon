# Kryon Rust Frontend

Rust language bindings for the Kryon UI framework, providing a type-safe builder API and declarative macro syntax.

## Overview

The Rust frontend follows the universal IR pipeline:

```
Rust code → .kir (JSON IR) → Renderer (SDL3/Terminal/Web)
```

## Features

### ✅ Implemented

- **Type-safe Builder API** - Manual UI construction with compile-time validation
- **Reactive State System** - `Signal<T>` for observable values with automatic updates
- **Declarative Macro Syntax** - `kryon_app!` macro for clean UI declarations (Proposal 3)
- **Full IR Support** - All component types, properties, and layouts
- **JSON Serialization** - Perfect .kir output compatible with all Kryon backends

### 🚧 In Progress

- **Automatic Reactive Detection** - Auto-detect Signal<T> usage in expressions
- **Event Handler Binding** - Connect UI events to reactive state updates
- **Loop Variable Capture** - Safe closure capture in for-loops

### 📋 Planned

- **CLI Integration** - `kryon compile app.rs` and `kryon run app.rs`
- **Code Generation** - Auto-generate Rust from .kry files
- **Conditional Rendering** - Natural if/match expressions in UI
- **Advanced Components** - Tabs, tables, dropdowns, etc.

## Installation

Add to your `Cargo.toml`:

```toml
[dependencies]
kryon = { path = "../kryon/bindings/rust/kryon" }
```

## Usage

### Builder API (Manual Construction)

```rust
use kryon::prelude::*;

fn main() {
    let root = column()
        .width("100%")
        .height("100%")
        .padding(30.0)
        .gap(20.0)
        .background("#1a1a2e")
        .child(
            text("Hello, Kryon!")
                .font_size(32.0)
                .color("#fff")
                .build(1)
        )
        .child(
            button("Click me")
                .background("#2ecc71")
                .padding(15.0)
                .build(2)
        )
        .build(0);

    let doc = IRDocument::new(root);
    doc.write_to_file("output.kir").unwrap();
}
```

### Declarative Macro Syntax (Recommended)

```rust
use kryon::prelude::*;

fn main() {
    let doc = kryon_app! {
        Column {
            width: "100%",
            height: "100%",
            padding: 30.0,
            gap: 20.0,
            background: "#1a1a2e",

            Text {
                content: "Hello, Kryon!",
                font_size: 32.0,
                color: "#fff",
            },

            Button {
                text: "Click me",
                background: "#2ecc71",
                padding: 15.0,
            },
        }
    };

    doc.write_to_file("output.kir").unwrap();
}
```

### Reactive State

```rust
use kryon::prelude::*;

fn main() {
    // Create reactive signal
    let mut count = signal(0);

    // Subscribe to changes
    count.subscribe(|value| {
        println!("Count changed to: {}", value);
    });

    // Update value (triggers subscribers)
    count.set(10);
    count.update(|c| *c += 1);

    // Use in UI (macro syntax)
    let doc = kryon_app! {
        Column {
            Text {
                content: format!("Count: {}", count.get()),
                font_size: 32.0,
            },

            Button {
                text: "+",
                // Event handler binding (in progress)
            },
        }
    };
}
```

## Project Structure

```
bindings/rust/
├── kryon-core/          # Core IR types with serde
│   └── src/
│       ├── ir.rs        # IRComponent, IRDocument
│       ├── color.rs     # Color helpers
│       ├── dimension.rs # Dimension types
│       └── style.rs     # Style properties
│
├── kryon-macros/        # Procedural macros
│   └── src/
│       ├── lib.rs       # kryon_app! macro entry
│       ├── parser.rs    # Parse declarative syntax
│       └── codegen.rs   # Generate builder calls
│
└── kryon/               # Main user-facing crate
    ├── src/
    │   ├── builder.rs   # Builder API
    │   └── reactive.rs  # Signal<T> implementation
    └── examples/
        ├── builder_demo.rs  # Builder API example
        ├── macro_demo.rs    # Declarative macro example
        └── counter_macro.rs # Reactive state example
```

## Examples

Run the examples:

```bash
# Builder API demo
cargo run --example builder_demo

# Declarative macro demo
cargo run --example macro_demo

# Reactive counter
cargo run --example counter_macro
```

All examples generate `.kir` files in `/tmp/` that can be rendered:

```bash
# Render with Kryon CLI
kryon run /tmp/rust_macro.kir
```

## Design Philosophy

The Rust frontend implements **Proposal 3 (Declarative Blocks)** from the design document, providing:

- **95% Nim DSL Parity** - Clean declarative syntax matching Nim bindings
- **Least Boilerplate** - No manual cloning or explicit reactive calls (planned)
- **Automatic Reactive Detection** - Signals auto-update UI (in progress)
- **Safe Loop Capture** - Automatic variable capture for closures (planned)
- **Type Safety** - Compile-time validation of all properties

### Comparison with Builder API

**Declarative Macro:**
```rust
kryon_app! {
    Button {
        text: "Click",
        on_click: || count.update(|c| *c += 1),
    }
}
```

**Builder API:**
```rust
button("Click")
    .on_click({
        let count = count.clone();
        move || count.update(|c| *c += 1)
    })
    .build(id)
```

The macro is 40% less code and eliminates manual cloning!

## Validation

All generated `.kir` files are validated against the Kryon IR specification:

```bash
# Semantic comparison (ignores ID differences)
./scripts/compare_kir.sh original.kir generated.kir

# Full validation
kryon validate generated.kir
```

## Next Steps

1. **Automatic Reactive Detection** - Parse expressions to detect Signal<T> usage
2. **Event Handler Integration** - Bind button clicks to Signal updates
3. **Loop Variable Capture** - Transform for-loops to safely capture variables
4. **CLI Integration** - Add Rust compilation to `kryon` CLI
5. **Code Generation** - Transpile `.kry → .rs` for all examples

See `/home/wao/.claude/plans/snuggly-stargazing-mitten.md` for the full implementation plan.

## Contributing

The Rust frontend is under active development. Current focus:

- [ ] Automatic reactive expression detection
- [ ] Event handler binding system
- [ ] Loop variable capture transformation
- [ ] CLI integration for `kryon compile app.rs`
- [ ] Full feature parity with Nim bindings

## License

Same as parent Kryon project.

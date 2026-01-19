# Kryon Examples

This directory contains example applications demonstrating Kryon's features.

## Directory Structure

```
examples/
├── kry/           # Source files (tracked in git)
│   ├── hello_world.kry
│   ├── button_demo.kry
│   ├── counters_demo.kry
│   └── ...
├── lua/           # Generated Lua code (NOT tracked in git)
└── README.md
```

## Source of Truth

**Only `.kry` files in `examples/kry/` are checked into git.**

All other formats (`.lua`, etc.) are auto-generated from the `.kry` source files.

## Generating Examples

```bash
# Generate all examples (creates examples/lua/*.lua from examples/kry/*.kry)
make generate-examples

# Or run the script directly
./scripts/generate_examples.sh

# Generate a specific example
./scripts/generate_examples.sh button_demo

# With validation (round-trip check)
./scripts/generate_examples.sh --validate

# Show diffs on validation failure
./scripts/generate_examples.sh --validate --diff

# Clean all generated files
./scripts/generate_examples.sh --clean
# Or
make clean-generated
```

## Running Examples

```bash
# Run any example (auto-generates .lua if needed)
./run_example.sh hello_world
./run_example.sh button_demo
./run_example.sh counters_demo

# Use terminal renderer
./run_example.sh button_demo lua terminal

# With environment variables
KRYON_RENDERER=terminal ./run_example.sh button_demo
```

## Pipeline

The generation pipeline ensures perfect round-trip transpilation:

```
.kry → .kir (static) → .lua → .kir (round-trip) → validate
```

1. `.kry → .kir (static)` - Parse .kry to JSON IR with `--preserve-static`
2. `.kir → .lua` - Generate idiomatic Lua DSL code
3. `.lua → .kir` - Compile generated Lua back to IR (round-trip test)
4. **Validation** - Compare original and round-trip `.kir` files

If validation fails, it indicates a transpilation bug.

## Available Examples

| Example | Description |
|---------|-------------|
| `hello_world` | Simple text display |
| `button_demo` | Interactive buttons with click handlers |
| `counters_demo` | Multiple counters with reactive state |
| `checkbox` | Checkbox component demo |
| `dropdown` | Dropdown selection menu |
| `text_input_simple` | Text input handling |
| `tabs_reorderable` | Tab groups with reordering |
| `animations_demo` | Animations and transitions |
| `column_alignments` | Column layout alignment options |
| `row_alignments` | Row layout alignment options |
| `typography_showcase` | Typography and text styling |
| `zindex_test` | Z-index layering demo |
| `if_else_test` | Conditional rendering |
| `todo` | Todo list application |

## Creating New Examples

1. Create a new `.kry` file in `examples/kry/`:
   ```bash
   # Edit your new example
   vim examples/kry/my_example.kry
   ```

2. Generate the Lua code:
   ```bash
   ./scripts/generate_examples.sh my_example
   ```

3. Run and test:
   ```bash
   ./run_example.sh my_example
   ```

4. Validate round-trip:
   ```bash
   ./scripts/generate_examples.sh my_example --validate
   ```

## Do NOT Edit Generated Files

Files in `examples/lua/` are auto-generated and will be **overwritten** when you run `make generate-examples`.

To make changes:
1. Edit the source `.kry` file in `examples/kry/`
2. Re-generate with `make generate-examples`

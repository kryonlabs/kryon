# Kryon Examples

This directory contains example applications written in the Kryon language (.kry files).

## Available Examples

### Basic Examples
- **`hello-world.kry`** - Simple text display with container styling
- **`button.kry`** - Interactive button with Lua event handling
- **`text_input.kry`** - Text input field with validation
- **`dropdown.kry`** - Dropdown/select element

### Layout Examples  
- **`column_alignments.kry`** - Various column alignment options
- **`row_alignments.kry`** - Various row alignment options
- **`counters_demo.kry`** - Multiple counter elements
- **`z_index_test.kry`** - Layer ordering and depth testing

## Running Examples

### Quick Start
```bash
# From the project root directory:

# List all examples
../scripts/run_examples.sh list

# Run a specific example
../scripts/run_example.sh hello-world raylib
../scripts/run_example.sh button raylib

# Run with debug output
../scripts/run_example.sh --debug button raylib

# Run all examples
../scripts/run_examples.sh raylib
```

### Manual Execution
```bash
# Compile to binary format
../build/bin/kryon compile hello-world.kry -o hello-world.krb

# Run the binary
../build/bin/kryon run hello-world.krb --renderer raylib

# Or compile and run in one step  
../build/bin/kryon run hello-world.kry --renderer raylib
```

## File Format

Examples use the `.kry` format with the following syntax:

```kry
// Metadata (optional)
@metadata {
    title: "Example App"
    version: "1.0.0"
}

// Styling (optional)
@style "base" {
    backgroundColor: "#191919FF"
    color: "#FF9900FF"
}

// Script functions (optional)
@function "lua" handleClick() {
    print("Button clicked!")
}

// UI Elements
Container {
    padding: 20
    backgroundColor: "#191970FF"
    
    Text {
        text: "Hello World"
        color: "yellow"
    }
    
    Button {
        text: "Click Me"
        onClick: "handleClick"
    }
}
```

## Binary Files

The `.krb` files are compiled binary versions of the `.kry` source files. They are generated automatically and can be ignored in version control.
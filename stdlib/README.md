# Kryon Standard Library

The Kryon standard library provides reusable components, styles, and utilities for building Kryon applications.

## Directory Structure

```
stdlib/
├── styles/          # Color schemes and style utilities
│   └── theme.kry    # Dark and light theme definitions
├── components/      # Reusable UI components
│   └── button.kry   # Button component variants
└── utils/           # Helper functions
    └── helpers.kry  # String, number, array utilities
```

## Usage

### Including Standard Library Modules

Use angle bracket syntax to include standard library modules:

```kry
include <kryon/styles/theme.kry>
include <kryon/components/button.kry>
```

Or use relative paths:

```kry
include "./node_modules/kryon-stdlib/styles/theme.kry"
```

### Theme System

```kry
include <kryon/styles/theme.kry>

App {
    backgroundColor = theme.background

    Container {
        Text {
            text = "Hello World"
            color = theme.text
        }
    }
}
```

### Button Components

```kry
include <kryon/components/button.kry>

Container {
    PrimaryButton(text: "Submit", onClick: "handleSubmit")
    SecondaryButton(text: "Cancel", onClick: "handleCancel")
    OutlineButton(text: "More Info", onClick: "showInfo")
}
```

### Utility Functions

```kry
include <kryon/utils/helpers.kry>

var value = 150
var clamped = clamp(value, 0, 100)  // Returns 100
```

## Installation

### Standard Linux Build

Standard library is available at `/usr/lib/kryon/` or `/usr/local/lib/kryon/`:

```bash
# Set module path
export KRYON_MODULE_PATH=/usr/local/lib/kryon
```

### TaijiOS Build

Standard library is installed at `/lib/kryon/`:

```bash
# On TaijiOS, automatically searched
include <kryon/styles/theme.kry>
```

## Contributing

To add new standard library modules:

1. Create `.kry` file in appropriate directory
2. Document the module with comments
3. Add usage examples to this README
4. Test with both native and sh language functions

## Future Plans

- Form components (Input, Checkbox, Dropdown)
- Layout utilities (Grid, Flex helpers)
- Animation helpers
- TaijiOS-specific modules (console, process, namespace)
- Validation library
- HTTP client utilities

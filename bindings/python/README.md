# Kryon Python Bindings

Python bindings for the Kryon UI Framework - a declarative, cross-platform UI framework.

## Features

- **Python DSL**: Build UI trees using Pythonic dataclasses
- **Bidirectional KIR conversion**: Convert Python DSL ↔ KIR (JSON)
- **C FFI**: Direct access to libkryon C library via cffi
- **Code Generation**: Generate Python DSL code from KIR files
- **Round-trip Support**: Python → KIR → Python preserves semantic equivalence

## Installation

### From Source

```bash
cd kryon/bindings/python
pip install -e .
```

### Requirements

- Python 3.7+
- libkryon_ir (installed to `~/.local/lib` or `/usr/local/lib`)
- cffi

## Quick Start

```python
import kryon

# Build UI with Python DSL
app = kryon.Column(
    width="100%",
    height="100%",
    justify_content="center",
    background_color="#1a1a1a",
    children=[
        kryon.Text(text="Hello, World!", font_size=32, color="#ffffff"),
        kryon.Button(title="Click Me", on_click="handle_click")
    ]
)

# Save as KIR
kryon.save_kir(app, "app.kir")

# Load from KIR
loaded_app = kryon.load_kir("app.kir")

# Generate Python code from KIR
python_code = kryon.generate_python(loaded_app.to_kir())
print(python_code)
```

## Command-Line Tool

Generate Python code from KIR files:

```bash
kryon-python-gen input.kir -o output.py
```

## API Reference

### Core Components

All 67 component types are supported:

- **Layout**: Container, Row, Column, Center
- **Input**: Button, Input, Checkbox, Dropdown, Textarea
- **Display**: Text, Image, Canvas, Markdown
- **Navigation**: TabGroup, TabBar, Tab, TabContent, TabPanel
- **Tables**: Table, TableHead, TableBody, TableFoot, TableRow, TableCell, TableHeaderCell
- **Markdown**: Heading, Paragraph, Blockquote, CodeBlock, HorizontalRule, List, ListItem, Link
- **And more...**

### Style Properties

All style properties use snake_case:

```python
component = kryon.Container(
    width="100%",
    height="100%",
    background_color="#ff0000",
    border_width=2,
    border_color="#00ff00",
    padding=16,
    margin=8,
    font_size=14,
    font_family="Arial",
    color="#ffffff"
)
```

### Layout Properties

```python
component = kryon.Container(
    flex_direction="row",
    justify_content="center",
    align_items="center",
    gap=10,
    padding=20
)
```

## Testing

```bash
cd kryon/bindings/python
pytest
```

## Examples

See the `examples/` directory for complete examples:

- `hello_world.py` - Simple "Hello, World!" app
- `todo_app.py` - Todo list application
- `calendar_app.py` - Calendar UI

## License

MIT License - see LICENSE file for details.

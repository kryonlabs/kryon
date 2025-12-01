# Kryon JavaScript Bindings

Plain JavaScript API for building Kryon applications without TypeScript or JSX.

## Installation

The JavaScript bindings are included with Kryon. No additional installation needed!

## Requirements

- [Bun](https://bun.sh) runtime (v1.0.0 or later)

## Quick Start

```javascript
import { run, c } from 'kryon';

const app = c.app({
  windowTitle: 'Hello Kryon!',
  windowWidth: 800,
  windowHeight: 600,
  backgroundColor: '#1a1a2e'
}, [
  c.column({
    width: '100%',
    height: '100%',
    gap: 20,
    align: 'center',
    justify: 'center'
  }, [
    c.text({ content: 'Hello from JavaScript!', fontSize: 32, color: '#ffffff' }),
    c.button({ title: 'Click Me', backgroundColor: '#4a4a6a' })
  ])
]);

run(app, { title: 'Hello World', width: 800, height: 600 });
```

## Usage

### Component Factory API

The `c` object provides factory functions for all Kryon components:

```javascript
import { c } from 'kryon';

// Layout components
c.app(props, ...children)       // Root application
c.container(props, ...children) // Generic container
c.column(props, ...children)    // Vertical layout
c.row(props, ...children)       // Horizontal layout
c.center(props, ...children)    // Centered layout

// UI components
c.text(props)                   // Text display
c.button(props)                 // Button
c.input(props)                  // Text input
c.checkbox(props)               // Checkbox
c.dropdown(props)               // Dropdown menu
c.image(props)                  // Image
c.canvas(props)                 // Custom drawing
c.markdown(props)               // Markdown rendering
```

### Manual Element Creation

For more control, use `createElement` directly:

```javascript
import { createElement as h } from 'kryon';

const element = h('container', {
  width: '100%',
  padding: 20
}, [
  h('text', { content: 'Hello!' })
]);
```

### Running Your App

```javascript
import { run } from 'kryon';

run(app, {
  // Desktop options
  title: 'My App',
  width: 800,
  height: 600,
  resizable: true,
  vsync: true,

  // Web options (when target = 'web')
  target: 'web',
  outputDir: 'dist'
});
```

## Component Properties

### App Component

```javascript
c.app({
  windowTitle: 'My App',      // Window title
  windowWidth: 800,           // Window width (px)
  windowHeight: 600,          // Window height (px)
  backgroundColor: '#1a1a2e'  // Background color
}, [...children])
```

### Layout Components

```javascript
c.container({
  width: '100%',              // Width (px, %, auto)
  height: 400,                // Height (px, %, auto)
  padding: 20,                // Padding (px or 'top right bottom left')
  margin: '10 20',            // Margin
  backgroundColor: '#fff',    // Background color
  border: '1px solid #ccc',   // Border
  borderRadius: 8,            // Border radius
  layout: 'flex',             // Layout mode
  direction: 'row',           // Flex direction (row, column)
  align: 'center',            // Align items
  justify: 'space-between',   // Justify content
  gap: 20                     // Gap between children
}, [...children])

c.column({ gap: 20, align: 'center' }, [...children])
c.row({ gap: 20, justify: 'space-between' }, [...children])
c.center({}, [...children])
```

### Text Component

```javascript
c.text({
  content: 'Hello World!',    // Text content
  fontSize: 24,               // Font size (px)
  fontWeight: 'bold',         // Font weight
  color: '#000',              // Text color
  textAlignment: 'center',    // Text alignment
  lineHeight: 1.6,            // Line height
  margin: '10 0'              // Margin
})
```

### Button Component

```javascript
c.button({
  title: 'Click Me',          // Button text
  backgroundColor: '#007bff', // Background color
  color: '#fff',              // Text color
  padding: '10px 20px',       // Padding
  borderRadius: 6,            // Border radius
  fontSize: 16,               // Font size
  onClick: handleClick        // Click handler
})
```

### Input Component

```javascript
c.input({
  placeholder: 'Enter text...', // Placeholder text
  value: inputValue,            // Input value
  onChange: handleChange,       // Change handler
  width: 300,                   // Width
  padding: 10                   // Padding
})
```

## Building for Different Targets

### Desktop (SDL3)

```bash
bun run app.js
```

Or in code:
```javascript
run(app, {
  title: 'My Desktop App',
  width: 1024,
  height: 768
});
```

### Web (HTML/CSS/JS)

```javascript
run(app, {
  target: 'web',
  outputDir: 'dist'
});
```

Then build:
```bash
bun run app.js
# Generates: dist/index.html, dist/styles.css, dist/app.js
```

### Terminal (TUI)

Set the `KRYON_RENDERER` environment variable:

```bash
KRYON_RENDERER=terminal bun run app.js
```

## Examples

See the `examples/` directory for complete examples:

- `hello_world.js` - Basic example
- `website.js` - Full website example

## Differences from TypeScript

The JavaScript bindings provide the same functionality as TypeScript but:

- **No type checking** - Properties are not validated at compile time
- **No JSX syntax** - Use object-based API instead
- **Same runtime** - Uses Bun FFI, same as TypeScript
- **Same performance** - No overhead compared to TypeScript

## API Reference

For complete API documentation, see the [Kryon Documentation](https://docs.kryonlabs.com).

## Contributing

Contributions are welcome! Please see the main Kryon repository for guidelines.

## License

MIT License - see LICENSE file for details.

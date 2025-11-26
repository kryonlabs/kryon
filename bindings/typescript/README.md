# Kryon TypeScript Bindings

TypeScript/JSX bindings for the Kryon UI Framework using Bun's native FFI.

## Requirements

- [Bun](https://bun.sh/) >= 1.0.0
- Kryon C library built (`libkryon_desktop.so`)

## Installation

```bash
cd bindings/typescript
bun install
```

## Usage

### Hello World

```tsx
import { run } from 'kryon';

function App() {
  return (
    <column width="100%" height="100%" gap={20} align="center" justify="center">
      <text content="Hello World!" fontSize={24} />
      <button title="Click Me" />
    </column>
  );
}

run(<App />, {
  title: 'My App',
  width: 800,
  height: 600,
});
```

### Running Examples

```bash
# Make sure libkryon_desktop.so is built
cd ../..
make build

# Run hello world example
bun run examples/typescript/hello_world.tsx
```

## Components

### Layout Components
- `<row>` - Horizontal flex container
- `<column>` - Vertical flex container
- `<center>` - Center content
- `<container>` - Generic container

### Content Components
- `<text>` - Text display
- `<button>` - Clickable button
- `<input>` - Text input
- `<checkbox>` - Checkbox control
- `<dropdown>` - Dropdown selector

### Props

All components support these common props:
- `width`, `height` - Size (number for px, string like "50%" or "auto")
- `padding`, `margin` - Spacing (number or [top, right, bottom, left])
- `backgroundColor` - Background color (hex string like "#ff0000")
- `opacity` - Opacity (0.0 to 1.0)
- `zIndex` - Stack order

Layout components also support:
- `gap` - Space between children
- `justify` - Main axis alignment (start, center, end, space-between, space-around, space-evenly)
- `align` - Cross axis alignment (start, center, end)

## Environment Variables

- `KRYON_LIB_PATH` - Path to libkryon_desktop.so (default: build/libkryon_desktop.so)

# Getting Started with Kryon

## Installation

```bash
git clone https://github.com/yourusername/kryon.git
cd kryon
make
make -C cli install
```

## Your First App

Create `hello.tsx`:

```tsx
import { kryonApp, Column, Text, Button, useState } from '@kryon/react';

export default kryonApp({
  render: () => {
    const [count, setCount] = useState(0);

    return (
      <Column>
        <Text>Count: {count}</Text>
        <Button onClick={() => setCount(count + 1)}>
          <Text text="Increment" />
        </Button>
      </Column>
    );
  }
});
```

## Run It

```bash
# Parse to KIR
bun ir/parsers/tsx/tsx_to_kir.ts hello.tsx > hello.kir

# Run on desktop
kryon run hello.kir
```

A native window opens with your UI!

## CLI Commands

```bash
# Compile source to KIR
kryon compile input.tsx --target kir --output app.kir

# Generate code from KIR
kryon codegen tsx app.kir output.tsx
kryon codegen lua app.kir output.lua

# Run KIR on desktop
kryon run app.kir

# Inspect KIR
kryon inspect app.kir

# Compare KIR files
kryon diff app1.kir app2.kir
```

## Components

- **Layout**: Column, Row, Container
- **UI**: Text, Button, Input
- **Advanced**: Table, Image, Canvas, Modal

## Events

All standard DOM events:
- onClick, onChange, onSubmit
- onFocus, onBlur
- onKeyDown, onKeyUp
- onMouseEnter, onMouseLeave

## Hooks

- useState - State management
- useEffect - Side effects
- useCallback - Memoized callbacks
- useMemo - Computed values
- useReducer - Complex state

## Examples

### Counter

```tsx
<Column>
  <Text>Count: {count}</Text>
  <Row>
    <Button onClick={() => setCount(count + 1)}>+</Button>
    <Button onClick={() => setCount(count - 1)}>-</Button>
    <Button onClick={() => setCount(0)}>Reset</Button>
  </Row>
</Column>
```

### Input

```tsx
<Column>
  <Input
    value={text}
    onChange={(e) => setText(e.target.value)}
    placeholder="Enter text"
  />
  <Text>{text}</Text>
</Column>
```

## Workflow

1. Write UI in your preferred language (TSX, Lua, Kry, etc.)
2. Parse to KIR
3. Test on desktop with `kryon run`
4. Generate code for target platforms
5. Deploy

## Troubleshooting

**"Command not found: kryon"**

Add to PATH:
```bash
export PATH="$HOME/.local/bin:$PATH"
```

**Desktop window doesn't open**

Install SDL3:
```bash
# Ubuntu/Debian
sudo apt install libsdl3-dev libsdl3-ttf-dev

# macOS
brew install sdl3 sdl3_ttf
```

## Next Steps

- Read `ARCHITECTURE.md` for system design

---

**Build once, run everywhere.** 🚀

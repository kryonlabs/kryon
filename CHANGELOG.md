# Changelog

All notable changes to the Kryon UI Framework will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Breaking Changes

#### JSX Codegen Now Outputs JavaScript (Not TypeScript)

- **`kryon codegen --lang=jsx` now generates JavaScript without type annotations**
- **Previous behavior**: `--lang=jsx` produced TypeScript (same as `--lang=tsx`)
- **Migration**: If you relied on `--lang=jsx` for TypeScript output, use `--lang=tsx` instead
- **Reason**: JSX is JavaScript + JSX syntax, TSX is TypeScript + JSX syntax. The previous behavior was technically incorrect.
- **Files affected**:
  - `cli/codegen_jsx.nim` (new)
  - `cli/codegen_tsx.nim` (refactored)
  - `cli/codegen_react_common.nim` (new shared module)

### Added

#### Separate JSX and TSX Codegen

- **JSX**: JavaScript with JSX syntax (`.jsx` files) - no type annotations
  - Pure JavaScript React components
  - No interfaces or type annotations
  - Bun-compatible ESM imports
  - Output directory: `examples/jsx/`

- **TSX**: TypeScript with JSX syntax (`.tsx` files) - full type annotations
  - TypeScript React components with interfaces
  - Full type safety with props interfaces
  - Optional vs required prop types
  - Output directory: `examples/tsx/`

#### Shared Common Module

- New `cli/codegen_react_common.nim` module reduces code duplication
- Shared React/JSX logic for both TypeScript and JavaScript generation
- Mode-aware helper functions for type-specific behavior
- ~80% code reuse between TSX and JSX generators

#### Bun-Compatible Imports

- Both JSX and TSX use standard ESM imports from `@kryon/react`
- Optimized for Bun runtime (native TypeScript and ESM support)
- Same import statements work in both JS and TS environments

#### Default Generation Includes Both Formats

- Running `./scripts/generate_examples.sh` now generates 8 formats by default:
  - nim, c, tsx, **jsx**, lua, html, markdown
- 16 examples × 8 languages = 128 total files
- Both `examples/tsx/*.tsx` and `examples/jsx/*.jsx` generated automatically

### Example Output Differences

**TypeScript (TSX)** - `examples/tsx/button_demo.tsx`:
```typescript
interface CounterProps {
  initialValue?: number;
}

function Counter({ initialValue = 0 }: CounterProps) {
  const [count, setCount] = useState(initialValue);
  return <Button onClick={() => setCount(count + 1)} />;
}
```

**JavaScript (JSX)** - `examples/jsx/button_demo.jsx`:
```javascript
function Counter({ initialValue = 0 }) {
  const [count, setCount] = useState(initialValue);
  return <Button onClick={() => setCount(count + 1)} />;
}
```

**Key Differences**:
- ❌ No `interface CounterProps` in JSX
- ❌ No `: CounterProps` type annotation in JSX
- ❌ No `?: number` optional type markers in JSX
- ✅ Same structure, same logic, same imports
- ✅ Both work with Bun runtime

### Migration Guide

If you were using `--lang=jsx` expecting TypeScript output:

**Before** (incorrect):
```bash
kryon codegen app.kir --lang=jsx  # Produced TypeScript
```

**After** (correct):
```bash
kryon codegen app.kir --lang=tsx  # Use tsx for TypeScript
kryon codegen app.kir --lang=jsx  # Use jsx for JavaScript
```

If you have scripts or build processes that use `--lang=jsx`:
1. Change `--lang=jsx` to `--lang=tsx` if you need TypeScript
2. Keep `--lang=jsx` if you actually want JavaScript (the new correct behavior)

### Technical Details

#### Architecture Changes

```
cli/
  codegen_react_common.nim   [NEW] ~350 lines - Shared React/JSX logic
  codegen_tsx.nim            [MODIFIED] 530→170 lines - TypeScript wrapper
  codegen_jsx.nim            [NEW] ~170 lines - JavaScript wrapper
  main.nim                   [MODIFIED] - Separate tsx/jsx routing
```

#### Code Reduction

- **Before**: 530 lines of duplicated logic
- **After**:
  - Common module: 350 lines (shared)
  - TSX module: 170 lines (TS-specific)
  - JSX module: 170 lines (JS-specific)
- **Total**: 690 lines (vs 1060 if duplicated)
- **Savings**: ~35% reduction through shared module

#### Files Modified

| File | Action | Purpose |
|------|--------|---------|
| `cli/codegen_react_common.nim` | **CREATE** | Shared React/JSX logic |
| `cli/codegen_tsx.nim` | **REFACTOR** | TypeScript-specific wrapper |
| `cli/codegen_jsx.nim` | **CREATE** | JavaScript-specific wrapper |
| `cli/main.nim` | **MODIFY** | Route jsx to new function |
| `scripts/generate_examples.sh` | **MODIFY** | Handle jsx separately |


# Kryon

Cross-platform UI framework with multi-language code generation.

## Feature Matrix

### Expression Transpiler

Converts KRY expressions (`x => x * 2`, `obj.prop + 1`) to target language syntax.

| Target | Status | Notes |
|--------|--------|-------|
| JavaScript | ✅ | ES6 syntax |
| C | ✅ | Arrow functions via registry |
| Python | ❌ | |
| Kotlin | ❌ | |
| TypeScript | ❌ | |
| Swift | ❌ | |
| Go | ❌ | |

### Code Generators

Converts KIR (JSON IR) to complete source files.

| Target | Status | Location | Notes |
|--------|--------|----------|-------|
| C | ✅ | `codegens/c/` | |
| Kotlin | ✅ | `codegens/kotlin/` | |
| KRY | ✅ | `codegens/kry/` | Round-trip |
| Markdown | ✅ | `codegens/markdown/` | |
| HTML/CSS/JS | ✅ | `codegens/web/` | |
| Swift | ❌ | | |
| Go | ❌ | | |

### Runtime Bindings

FFI bindings for using Kryon from other languages.

| Language | Status | Location |
|----------|--------|----------|
| C | ✅ | `bindings/c/` |
| JavaScript | ✅ | `bindings/javascript/` |
| Kotlin | ✅ | `bindings/kotlin/` |
| Swift | ❌ | |
| Go | ❌ | |
| Rust | ❌ | |

### Expression Features

| Feature | JS | C |
|---------|----|----|
| Literals | ✅ | ✅ |
| Binary ops | ✅ | ✅ |
| Logical ops | ✅ | ✅ |
| Property access | ✅ | ✅ |
| Array access | ✅ | ✅ |
| Function calls | ✅ | ✅ |
| Array literals | ✅ | ✅ |
| Object literals | ✅ | ✅ | ✅ |
| Arrow functions | ✅ | ✅ |
| Ternary | ✅ | ✅ |
| Template strings | ❌ | ❌ |
| Spread operator | ❌ | ❌ |
| Destructuring | ❌ | ❌ |
| Optional chaining | ❌ | ❌ |

### Summary

| Category | Done | Missing |
|----------|------|---------|
| Expression Transpiler Targets | 2 | 5 |
| Code Generators | 5 | 2 |
| Runtime Bindings | 3 | 4 |

### Build Pipeline Status

| Pipeline | Status | Notes |
|----------|--------|-------|
| KRY -> KIR | ✅ | Native C parser |
| KIR -> Web (HTML/CSS/JS) | ✅ | Full support |
| KIR -> C source | ✅ | Via codegen |
| KRY -> Desktop (C/SDL3) | ❌ | `build_c_desktop()` not implemented |
| Hot reload (desktop) | ❌ | Not implemented |
| Hot reload (web) | ✅ | Dev server |

### Known Issues

- **Desktop build not implemented**: Missing `build_c_desktop()` for KRY frontend. KRY should use: KRY→KIR→C codegen→compile with SDL3.
- **Expression transpiler gaps**: Kotlin code generator exists but expression transpiler doesn't support it (uses raw expressions).

---

## Architecture

```
Source (.kry/.c)
    ↓
Parser (ir/parsers/)
    ↓
KIR (JSON Intermediate Representation)
    ↓
Code Generator (codegens/)
    ↓
Target Source Code
    ↓
Runtime (renderers/ or bindings/)
```

## Directory Structure

```
kryon/
├── ir/                     # Intermediate Representation
│   ├── src/                # Core IR implementation
│   ├── include/            # Public headers
│   └── parsers/            # Source language parsers
│       ├── kry/            # KRY DSL parser + expression transpiler
│       ├── html/           # HTML parser
│       └── c/              # C parser
├── codegens/               # Code generators
│   ├── c/
│   ├── kotlin/
│   ├── kry/
│   ├── markdown/
│   └── web/
├── bindings/               # Language bindings
│   ├── c/
│   ├── javascript/
│   └── kotlin/
├── renderers/              # Platform renderers
│   ├── sdl3/
│   ├── raylib/
│   └── terminal/
├── cli/                    # Command-line tools
├── examples/               # Example applications
└── build/                  # Build output
```

## Building

```bash
cd ir && make -j8
```

## License

0BSD
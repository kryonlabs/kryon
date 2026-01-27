# Kryon

Cross-platform UI framework with multi-language code generation.

## Feature Matrix

### Expression Transpiler

Converts KRY expressions (`x => x * 2`, `obj.prop + 1`) to target language syntax.

| Target | Status | Notes |
|--------|--------|-------|
| Lua | ✅ | 1-indexed arrays, and/or/not |
| JavaScript | ✅ | ES6 syntax |
| C | ✅ | Arrow functions via registry |
| Python | ❌ | |
| Kotlin | ❌ | |
| TypeScript | ❌ | |
| Swift | ❌ | |
| Go | ❌ | |
| [Hare](https://harelang.org/) | ⚠️ | Basic support (no templates) |

### Code Generators

Converts KIR (JSON IR) to complete source files.

| Target | Status | Location | Notes |
|--------|--------|----------|-------|
| Lua | ⚠️ | `codegens/lua/` | Skeleton only, missing events/bindings |
| TSX/React | ✅ | `codegens/tsx/` | |
| C | ✅ | `codegens/c/` | |
| Kotlin | ✅ | `codegens/kotlin/` | |
| Hare | ✅ | `codegens/hare/` | NEW - Beta |
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
| TypeScript | ✅ | `bindings/typescript/` |
| Lua | ✅ | `bindings/lua/` |
| Kotlin | ✅ | `bindings/kotlin/` |
| [Hare](https://harelang.org/) | ✅ | `bindings/hare/` |
| Swift | ❌ | |
| Go | ❌ | |
| Rust | ❌ | |

### Expression Features

| Feature | Lua | JS | C |
|---------|-----|----|----|
| Literals | ✅ | ✅ | ✅ |
| Binary ops | ✅ | ✅ | ✅ |
| Logical ops | ✅ | ✅ | ✅ |
| Property access | ✅ | ✅ | ✅ |
| Array access | ✅ | ✅ | ✅ |
| Function calls | ✅ | ✅ | ✅ |
| Array literals | ✅ | ✅ | ✅ |
| Object literals | ✅ | ✅ | ✅ |
| Arrow functions | ✅ | ✅ | ✅ |
| Ternary | ✅ | ✅ | ✅ |
| Template strings | ❌ | ❌ | ❌ |
| Spread operator | ❌ | ❌ | ❌ |
| Destructuring | ❌ | ❌ | ❌ |
| Optional chaining | ❌ | ❌ | ❌ |

### Summary

| Category | Done | Missing |
|----------|------|---------|
| Expression Transpiler Targets | 3 | 6 |
| Code Generators | 9 | 2 |
| Runtime Bindings | 6 | 4 |

### Build Pipeline Status

| Pipeline | Status | Notes |
|----------|--------|-------|
| KRY -> KIR | ✅ | Native C parser |
| Hare -> KIR | ✅ | NEW - Hare DSL parser |
| KIR -> Web (HTML/CSS/JS) | ✅ | Full support |
| KIR -> Lua source | ⚠️ | Skeleton only |
| KIR -> C source | ✅ | Via codegen |
| KIR -> Hare source | ✅ | Via codegen |
| KRY -> Desktop (C/SDL3) | ❌ | `build_c_desktop()` not implemented |
| Hare -> Desktop binary | ✅ | Via Hare compiler |
| Lua -> Desktop binary | ✅ | Via LuaJIT |
| Hot reload (desktop) | ✅ | Lua runtime only |
| Hot reload (web) | ✅ | Dev server |

### Known Issues

- **Desktop build hardcoded to Lua**: `cmd_build.c:321` calls `build_lua_desktop()` for ALL desktop builds. Missing `build_c_desktop()` for KRY frontend. KRY should use: KRY→KIR→C codegen→compile with SDL3.
- **Lua codegen incomplete**: Missing component definitions, event handlers, property bindings, ForEach expansion, logic blocks. Output is skeleton only.
- **Expression transpiler gaps**: Kotlin code generator exists but expression transpiler doesn't support it (uses raw expressions).

---

## Architecture

```
Source (.kry/.lua/.tsx/.c/.ha)
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
│       ├── lua/            # Lua parser
│       ├── tsx/            # TSX parser
│       ├── html/           # HTML parser
│       ├── c/              # C parser
│       └── hare/           # Hare DSL parser
├── codegens/               # Code generators
│   ├── lua/
│   ├── tsx/
│   ├── c/
│   ├── kotlin/
│   ├── hare/
│   ├── kry/
│   ├── markdown/
│   └── web/
├── bindings/               # Language bindings
│   ├── c/
│   ├── javascript/
│   ├── typescript/
│   ├── lua/
│   ├── kotlin/
│   └── hare/               # Hare FFI bindings + DSL
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
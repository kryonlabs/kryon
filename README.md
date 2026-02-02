# Kryon

**Multi-language UI framework** - Write declarative UI once, deploy to Web, Desktop, Mobile, and more.

## Quick Start

```bash
# Build
make && make install

# Run examples (transpile KRY to another language)
kryon run --target=limbo+tk@taiji examples/kry/hello_world.kry
kryon run --target=tcl+tk@desktop examples/kry/hello_world.kry
```

## Targets

Kryon transpiles **KRY source code** to other languages using **`language+toolkit@platform`** format:

| Target | Language | Toolkit | Platform | Status |
|--------|----------|---------|----------|--------|
| `tcl+tk@desktop` | Tcl | Tk | Desktop | 🟡 Limited (30%) |
| `javascript+dom@web` | JavaScript | DOM | Web | 🟡 Limited (60%) |
| `limbo+tk@taiji` | Limbo | Tk | TaijiOS | 🔴 Not Implemented |
| `c+sdl3@desktop` | C | SDL3 | Desktop | 🔴 In Progress |
| `c+raylib@desktop` | C | Raylib | Desktop | 🔴 In Progress |
| `kotlin+android@mobile` | Kotlin | Android | Mobile | 🔴 Not Implemented |

**KRY** is the source language - all `.kry` files are transpiled to target languages.

**Actually Working**:
- ✅ Terminal apps (all languages with terminal toolkit)
- 🟡 Tcl/Tk (loses script code on round-trip)
- 🟡 Web/DOM (loses some presentation details)

**Legend**:
- 🟢 **Production Ready** - Fully working for production use
- 🟡 **Limited** - Works but has limitations or bugs
- 🔴 **Not Implemented** - Planned but not yet built

### Platform Aliases

Shorter aliases are available for convenience:
- `taiji` → `taijios`
- `inferno` → `taijios`

### Auto-Resolution

If a language has only **one valid** platform+toolkit combination, you can omit them:

```bash
# JavaScript only works with DOM on web
kryon run --target=javascript main.kry  # Auto-resolves to javascript+dom@web
```

If a language has **multiple** valid combinations, you must specify explicitly:
```bash
# C works with multiple toolkits/platforms
kryon run --target=c+sdl3@desktop main.kry  # Must specify
```

### Examples

KRY source files (`.kry`) are transpiled to target languages:

```bash
# TaijiOS (using alias)
kryon run --target=limbo+tk@taiji main.kry

# Desktop
kryon run --target=c+sdl3@desktop main.kry

# Web (auto-resolves)
kryon run --target=javascript main.kry

# Mobile
kryon run --target=kotlin+android@mobile main.kry
```

## Commands

```bash
# List all valid language+toolkit@platform combinations
kryon targets

# Show languages, toolkits, or platforms
kryon lang                              # List all languages
kryon toolkit                           # List all toolkits
kryon platform                          # List all platforms

# Show capabilities
kryon capabilities                      # Show all combinations
kryon capabilities --lang=c            # Show toolkits for C
kryon capabilities --toolkit=sdl3      # Show languages for SDL3

# Build and run
kryon build --target=limbo+tk@taiji main.kry
kryon run --target=limbo+tk@taiji main.kry

# Dev server with hot reload (web only)
kryon dev main.kry
```

## Directory Structure

```
kryon/
├── ir/              # Intermediate Representation
├── codegens/        # Code generators
│   ├── languages/   # Language emitters
│   ├── toolkits/    # Toolkit profiles
│   └── platforms/   # Platform profiles
├── cli/             # Command-line interface
├── runtime/         # Runtime libraries
├── tests/           # Test suites
└── examples/        # Example code
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed architecture.

## Round-Trip Testing

Test that codegens preserve information through KRY → KIR → Target → KIR → KRY:

```bash
# Run test suite
bash tests/round_trip/test_roundtrip.sh

# Manual test
kryon parse hello_world.kry -o step1.kir
kryon codegen kry step1.kir step2_kry/
kryon parse step2_kry/main.kry -o step3.kir
kryon codegen kry step3.kir step4_kry/
diff hello_world.kry step4_kry/main.kry
```

**Results**:
- ✅ **KRY → KRY**: 95%+ preservation (production ready)
- 🟡 **Tcl+Tk**: ~30% preservation (scripts lost, expected)
- 🟡 **JavaScript+DOM**: ~60% preservation (presentation layer)
- 🔴 **C targets**: Parser in testing phase

**See [CODEGEN_STATUS.md](CODEGEN_STATUS.md) for detailed status and roadmap.**

## License

MIT

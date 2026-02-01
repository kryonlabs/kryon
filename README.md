# Kryon

**Multi-language UI framework** - Write declarative UI once, deploy to Web, Desktop, Mobile, and more.

## Quick Start

```bash
# Build
make && make install

# Run example
kryon run --target=limbo examples/kry/hello_world.kry
kryon run --target=tcl+tk examples/kry/hello_world.kry
```

## Targets

Kryon codegen targets are **explicit language+toolkit combinations**:

| Target Combination | Language | Toolkit | Alias | Status |
|-------------------|----------|---------|-------|--------|
| `kry` | KRY | KRY (self) | - | 🟢 Production |
| `limbo+draw` | Limbo | Draw | `limbo` | 🟡 One-Way? |
| `tcl+tk` | Tcl | Tk | `tcltk` | 🟡 Limited |
| `c+sdl3` | C | SDL3 | `c` | 🔴 Broken |
| `c+raylib` | C | Raylib | - | 🔴 Broken |
| `web` | JavaScript | DOM | `html` | 🟡 Limited |
| `markdown` | Markdown | - | - | 🔴 Docs Only |
| `android` | Java/Kotlin | Android | - | 🔴 Not Implemented |

**Legend**: 🟢 = Production Ready, 🟡 = Limited/Poor, 🔴 = Not Working

```bash
# Explicit syntax (recommended)
kryon run --target=limbo+draw main.kry
kryon run --target=tcl+tk main.kry
kryon run --target=c+sdl3 main.kry
kryon run --target=web main.kry

# Short aliases
kryon run --target=limbo main.kry    # → limbo+draw
kryon run --target=tcltk main.kry    # → tcl+tk
kryon run --target=c main.kry         # → c+sdl3
```

## Round-Trip Codegen Status

### What Works ✅

| Target | Round-Trip? | Preservation | Tests |
|--------|-----------|---------------|-------|
| **kry** | ✅ YES | 95%+ | 8/8 passing |
| **tcl+tk** | ✅ YES | ~30% | 8/8 passing |
| **web** | ✅ YES | ~40-60% | 8/8 passing |

**Total**: 24/24 tests passing ✅

### What Needs Work ⚠️

| Target | Issue | Priority |
|--------|-------|----------|
| **c+sdl3**, **c+raylib** | C parser needs compilation setup | 🔴 HIGH |
| **limbo+draw** | Only 20-30% preservation | 🟡 MEDIUM |
| **android** | No reverse parser | 🟢 LOW |

**See [CODEGEN_STATUS.md](CODEGEN_STATUS.md) for detailed status, action items, and test results.**

---

## Directory Structure

```
kryon/
├── ir/              # Intermediate Representation
├── codegens/        # Code generators
│   ├── languages/   # Language emitters
│   └── toolkits/    # Toolkit profiles
├── cli/             # Command-line interface
├── runtime/         # Runtime libraries
├── tests/           # Test suites
│   └── round_trip/  # Round-trip validation
└── examples/        # Examples
    └── kry/         # KRY source files
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed architecture.

## Building

```bash
make && make install
```

## Usage

```bash
# Build and run
kryon build --target=limbo+draw main.kry
kryon run --target=limbo+draw main.kry

# List all targets
kryon targets

# Dev server with hot reload
kryon dev main.kry
```

## Round-Tip Testing

Test that codegens preserve information correctly through KRY → KIR → Target → KIR → KRY conversions:

```bash
# Run test suite
bash tests/round_trip/test_roundtrip.sh

# Manual round-trip test
kryon parse hello_world.kry -o step1.kir
kryon codegen kry step1.kir step2_kry/
kryon parse step2_kry/main.kry -o step3.kir
kryon codegen kry step3.kir step4_kry/
diff hello_world.kry step4_kry/main.kry
```

**Results**:
- ✅ KRY self-round-trip: 95%+ preservation (production ready)
- ✅ Tcl+Tk round-trip: ~30% preservation (scripts lost, expected)
- ✅ Web round-trip: ~40-60% preservation (presentation layer)
- 🔴 C round-trip: **BROKEN** - needs compilation setup
- ⚠️ Limbo+Draw round-trip: ~20-30% preservation (documenting as one-way?)

**See [CODEGEN_STATUS.md](CODEGEN_STATUS.md) for:**
- Detailed target status
- Information preservation matrix
- Action items and roadmap
- Test results and discrepancies

## License

MIT

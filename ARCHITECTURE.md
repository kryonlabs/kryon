# Kryon Architecture

## Overview

Kryon is a multi-language UI framework that compiles declarative UI code to various target languages and toolkits.

## Core Components

```
kryon/
├── ir/              # Intermediate Representation
│   ├── parsers/     # KRY, TCL, HTML, C parsers
│   └── include/     # IR public API
│
├── codegens/        # Code generators (language + toolkit + platform)
│   ├── platforms/   # Platform profiles
│   │   └── common/  # Platform registry
│   ├── languages/   # Language emitters (syntax generation)
│   │   ├── common/  # Language registry
│   │   ├── tcl/
│   │   ├── limbo/
│   │   └── ...
│   └── toolkits/    # Toolkit profiles (widget mappings)
│       ├── common/  # Toolkit registry
│       ├── tk/      # Tcl/Tk
│       ├── draw/    # Limbo Draw
│       └── ...
│
├── cli/             # Command-line interface
│   └── src/
│       ├── commands/    # CLI commands
│       │   ├── cmd_capabilities.c  # Show capabilities
│       │   ├── cmd_lang.c          # List languages
│       │   ├── cmd_platform.c      # List platforms
│       │   └── cmd_toolkit.c       # List toolkits
│       └── config/
│
├── runtime/         # Runtime libraries
│   ├── desktop/     # C runtime (SDL3, Raylib)
│   └── ...
│
└── third_party/     # Dependencies
```

## Language + Toolkit + Platform Model

Kryon separates **language**, **UI toolkit**, and **deployment platform**:

```
Source (.kry) → IR → Language Emitter + Toolkit Profile → Platform Output
```

### Supported Languages

| Language | Extension | Description |
|----------|-----------|-------------|
| **KRY** | `.kry` | **Source language** (input) |
| Limbo    | `.b`      | Inferno Limbo |
| Tcl      | `.tcl`    | Tcl/Tk |
| C        | `.c`      | C (SDL3, Raylib) |
| Kotlin   | `.kt`     | Android/Kotlin |
| JS/TS    | `.js/ts`  | Web (HTML/CSS) |
| Lua      | `.lua`    | Lua (binding) |

> **Note**: KRY is the input/source language. All `.kry` files are transpiled to target languages.

### Supported Toolkits

| Toolkit | Language(s) | Description |
|---------|-------------|-------------|
| Tk      | tcl, limbo, c  | Tcl/Tk widgets |
| DOM     | js/ts       | HTML/CSS |
| SDL3    | c, lua      | SDL3 graphics |
| Raylib  | c           | Raylib graphics |
| Terminal | c, tcl, limbo, lua | Console I/O |
| Android | kotlin, java | Android Views |

### Supported Platforms

| Platform | Aliases | Description |
|----------|---------|-------------|
| desktop  | -       | Desktop applications |
| web      | -       | Web browsers |
| mobile   | -       | Mobile devices |
| taijios  | taiji, inferno | TaijiOS |
| terminal | -       | Terminal/console |

### Target Syntax

All targets use **`language+toolkit@platform`** format (all three required):

```bash
--target=limbo+tk@taiji           # Transpile KRY → Limbo+Tk on TaijiOS
--target=tcl+tk@desktop           # Transpile KRY → Tcl/Tk on desktop
--target=c+sdl3@desktop           # Transpile KRY → C+SDL3 on desktop
--target=javascript+dom@web       # Transpile KRY → JS+DOM on web
--target=kotlin+android@mobile    # Transpile KRY → Kotlin+Android on mobile
```

Platform aliases make it shorter:
```bash
--target=limbo+tk@taiji           # Same as @taijios
--target=limbo+tk@inferno         # Same as @taijios
```

## Codegen Pipeline

```
1. Parse  → KRY/HTML/TCL source → IR
2. Transform → IR optimizations
3. Emit   → IR → Language + Toolkit → Source code
4. Build  → Source code → Binary/Bytecode
5. Run    → Execute
```

## Key Design Decisions

### 1. Toolkit-Specific IRs

Each toolkit has its own IR abstraction:

- **TkIR** - Tk widget types (`button`, `label`, `frame`)
- **DrawIR** - Draw widget types (`Button`, `Textfield`, `Frame`)
- **DOMIR** - HTML elements (`button`, `span`, `div`)

This prevents "Tk bias" where non-Tk toolkits incorrectly use Tk conventions.

### 2. Explicit Combinations

Language, toolkit, and platform are **explicitly combined**:

```
limbo+tk@taiji      ✅ Valid (Limbo + Tk on TaijiOS)
tcl+tk@desktop      ✅ Valid (Tcl + Tk on desktop)
c+sdl3@desktop      ✅ Valid (C + SDL3 on desktop)
tcl+draw@desktop    ❌ Invalid (Tcl doesn't support Draw)
```

### 3. Validation Matrix

The combo registry validates all combinations at compile-time, catching errors early.

### 4. Platform Support

The **platform registry** adds platform awareness:

- **Platform profiles** define deployment targets (web, desktop, mobile, taijios, terminal)
- **Platform aliases** for convenience: `taiji` → `taijios`, `inferno` → `taijios`
- **3D validation**: language × toolkit × platform

```bash
# Query capabilities
kryon targets                    # Show all valid combinations
kryon capabilities              # Show technical compatibility
kryon lang                     # List languages
kryon platform                 # List platforms
kryon platform                 # List platforms (informational)
kryon toolkit                  # List toolkits
```

**Note**: Full platform-driven CLI syntax (`--platform=desktop`) is planned for future releases.

## Directory Details

### `/ir` - Intermediate Representation
- Generic UI representation
- Parsers for KRY, TCL, HTML, C
- Serialization and validation

### `/codegens` - Code Generation
- **languages/** - Syntax generation for each language
- **toolkits/** - Widget type mappings for each toolkit
- Registry system for validation

### `/cli` - Command-Line Interface
- `kryon run` - Run code
- `kryon build` - Build project
- `kryon parse` - Parse source to IR
- `kryon targets` - List valid targets

### `/runtime` - Runtime Libraries
- Desktop renderers (SDL3, Raylib)
- Reactive system
- Event handling

## Extending Kryon

### Adding a New Language

1. Create `/codegens/languages/<lang>/`
2. Implement language emitter
3. Register in `language_registry.c`
4. Add validation rules

### Adding a New Toolkit

1. Create `/codegens/toolkits/<toolkit>/`
2. Define toolkit IR
3. Implement widget mappings
4. Register in `toolkit_registry.c`
5. Add validation rules

## Contributing

See `README.md` for contribution guidelines.

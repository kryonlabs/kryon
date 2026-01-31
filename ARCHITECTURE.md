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
├── codegens/        # Code generators (language + toolkit)
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
│       ├── commands/
│       └── config/
│
├── runtime/         # Runtime libraries
│   ├── desktop/     # C runtime (SDL3, Raylib)
│   └── ...
│
└── third_party/     # Dependencies
```

## Language + Toolkit Model

Kryon separates **language** from **UI toolkit**:

```
Source (.kry) → IR → Language Emitter + Toolkit Profile → Output
```

### Supported Languages

| Language | Extension | Description |
|----------|-----------|-------------|
| Limbo    | `.b`      | Inferno Limbo |
| Tcl      | `.tcl`    | Tcl/Tk |
| C        | `.c`      | C (SDL3, Raylib) |
| Kotlin   | `.kt`     | Android/Kotlin |
| JS/TS    | `.js/ts`  | Web (HTML/CSS) |

### Supported Toolkits

| Toolkit | Language(s) | Description |
|---------|-------------|-------------|
| Tk      | tcl         | Tcl/Tk widgets |
| Draw    | limbo       | Inferno Draw API |
| DOM     | js/ts       | HTML/CSS |
| SDL3    | c           | SDL3 graphics |
| Raylib  | c           | Raylib graphics |
| stdio   | c, tcl      | Terminal/console |

### Target Syntax

```bash
# Explicit language+toolkit
--target=limbo+draw
--target=tcl+tk
--target=c+sdl3

# Short aliases (backward compatible)
--target=limbo     # → limbo+draw
--target=tcltk     # → tcl+tk
--target=sdl3      # → c+sdl3
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

Language and toolkit are **explicitly combined**:

```
limbo+draw  ✅ Valid (Limbo + Draw)
tcl+tk      ✅ Valid (Tcl + Tk)
tcl+draw    ❌ Invalid (Tcl doesn't support Draw)
```

### 3. Validation Matrix

The combo registry validates all combinations at compile-time, catching errors early.

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

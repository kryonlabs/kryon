# Kryon Overview

## What is Kryon?

Kryon is a cross-platform UI framework that compiles declarative UI definitions (KRY files) into efficient binary format (KRB files) for runtime execution across desktop, web, and mobile platforms.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    KRY Source Files                      │
│              (Declarative UI Definition)                 │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                   Kryon Compiler                         │
│         Lexer → Parser → Optimizer → Generator          │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                    KRB Binary Files                      │
│              (Optimized Binary Format)                   │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                    Kryon Runtime                         │
│   Loader → Element Tree → Layout Engine → Renderer      │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                  Platform Renderers                      │
│   Native (Raylib) │ Web (HTML) │ Terminal │ Mobile     │
└─────────────────────────────────────────────────────────┘
```

## Key Components

### 1. KRY Language
- Declarative syntax for UI definition
- Component-based architecture
- Built-in scripting support (Lua, JavaScript, Python, Wren)
- Reactive state management
- Style system with inheritance

### 2. Kryon Compiler
- Fast incremental compilation
- Optimization passes
- Multi-target code generation
- Source mapping for debugging
- Component bundling

### 3. KRB Binary Format
- Compact binary representation
- Platform-agnostic format
- Embedded resources and scripts
- Optional compression
- Fast loading and parsing

### 4. Kryon Runtime
- Element tree management
- Event handling system
- Layout engine (Taffy-based flexbox)
- State management
- Script execution environment

### 5. Platform Renderers
- **Native**: Raylib, SDL2, platform-specific
- **Web**: HTML/CSS/JS generation
- **Terminal**: Text-based UI
- **Mobile**: iOS/Android native

## Design Principles

1. **Write Once, Run Anywhere**: Single codebase for all platforms
2. **Performance First**: Compiled binary format for fast loading
3. **Developer Experience**: Hot reload, clear errors, great tooling
4. **Extensible**: Plugin system for custom elements and renderers
5. **Standards-Based**: Flexbox layout, CSS-like styling

## Use Cases

- Desktop applications
- Web applications
- Mobile apps
- Game UIs
- Embedded interfaces
- Terminal UIs

## Getting Started

```kry
App {
    windowTitle: "Hello Kryon"
    windowWidth: 800
    windowHeight: 600
    
    Container {
        display: "flex"
        alignItems: "center"
        justifyContent: "center"
        
        Text {
            text: "Hello, World!"
            fontSize: 24
            textColor: "#333333"
        }
    }
}
```

Compile and run:
```bash
kryon compile hello.kry -o hello.krb
kryon run hello.krb
```

## Documentation Structure

1. **KRY_LANGUAGE_SPEC.md** - Complete language reference
2. **KRB_BINARY_SPEC.md** - Binary format specification
3. **KRB_TO_HTML_MAPPING.md** - Web renderer mapping
4. **KRYON_IR_PIPELINE.md** - Compilation pipeline details

## Project Status

- ✅ Core language implementation
- ✅ Compiler with optimization
- ✅ Binary format specification
- ✅ Basic runtime system
- ✅ Raylib renderer
- 🚧 Web renderer
- 🚧 Mobile renderers
- 🚧 Advanced layout features
- 🚧 Animation system
- 🚧 Developer tools
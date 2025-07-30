# Kryon Documentation

Complete documentation for the Kryon UI description language and runtime system.

## 📖 Documentation Structure

### 🗣️ Language Specification
Core language documentation for KRY syntax and features.

- **[KRY Language Specification](language/KRY_LANGUAGE_SPECIFICATION.md)** - Complete syntax reference, control flow, expressions, and best practices

### ⚙️ Compiler & Pipeline
Technical documentation for the compilation system and intermediate representation.

- **[IR Pipeline](compiler/KRYON_IR_PIPELINE.md)** - KRY to KIR compilation pipeline, multi-language support, and optimization

### 🏗️ Architecture & Runtime
Technical documentation for the Kryon runtime architecture and platform abstraction.

- **[Runtime Architecture](architecture/KRYON_RUNTIME_ARCHITECTURE.md)** - Complete runtime system architecture, core components, and subsystems
- **[Platform Abstraction Layer](architecture/KRYON_PLATFORM_ABSTRACTION_LAYER.md)** - Cross-platform abstraction for rendering, input, hardware access, and system integration
- **[Compilation Pipeline](architecture/KRYON_COMPILATION_PIPELINE.md)** - Multi-target compilation pipeline from KRY to platform-specific output
- **[Script Engine Integration](architecture/KRYON_SCRIPT_ENGINE_INTEGRATION.md)** - Multi-language script engine integration (Lua, JavaScript, Python, Kryon Lisp)
- **[Runtime System](runtime/KRYON_RUNTIME_SYSTEM.md)** - Core runtime architecture, app lifecycle, and system components
- **[Script System](runtime/KRYON_SCRIPT_SYSTEM.md)** - Lua integration, DOM bridge, and canvas scripting
- **[Template Engine](runtime/KRYON_TEMPLATE_ENGINE.md)** - Reactive variables, expression evaluation, and data binding
- **[Layout Engine](runtime/KRYON_LAYOUT_ENGINE.md)** - Taffy integration, flexbox support, and performance optimizations
- **[Backend Renderers](runtime/KRYON_BACKEND_RENDERERS.md)** - Multi-backend rendering (WGPU, Raylib, HTML, Terminal, etc.)

### 📦 Binary Format
KRB binary format specification and tooling.

- **[KRB Binary Format](binary-format/KRB_BINARY_FORMAT_SPECIFICATION.md)** - Complete binary format specification
- **[KRB to HTML Mapping](binary-format/KRB_TO_HTML_MAPPING.md)** - Conversion reference for web deployment

### 📚 Reference & Implementation Specifications
Quick reference guides, comprehensive API documentation, and complete implementation specifications.

- **[Kryon Reference](reference/KRYON_REFERENCE.md)** - Complete element and property reference with inheritance
- **[Error Reference](reference/KRYON_ERROR_REFERENCE.md)** - All error codes, debugging techniques, and solutions
- **[Trait Specifications](reference/KRYON_TRAIT_SPECIFICATIONS.md)** - Complete trait definitions for implementation
- **[Performance Specifications](reference/KRYON_PERFORMANCE_SPECIFICATIONS.md)** - Performance requirements and benchmarks
- **[Thread Safety Specifications](reference/KRYON_THREAD_SAFETY_SPECIFICATIONS.md)** - Concurrency and async contracts
- **[Script Bridge API](reference/KRYON_SCRIPT_BRIDGE_API.md)** - Complete script integration API
- **[Optimization Strategies](reference/KRYON_OPTIMIZATION_STRATEGIES.md)** - Implementation details for all optimizations

## 🚀 Quick Start

1. **Learn the Language**: Start with [KRY Language Specification](language/KRY_LANGUAGE_SPECIFICATION.md)
2. **Understand Elements**: Check [Kryon Reference](reference/KRYON_REFERENCE.md) for all available elements and properties
3. **Explore Runtime**: Read [Runtime System](runtime/KRYON_RUNTIME_SYSTEM.md) to understand how Kryon works internally
4. **Choose a Renderer**: Review [Backend Renderers](runtime/KRYON_BACKEND_RENDERERS.md) for deployment options

## 🎯 Use Cases

### Desktop Applications
- Use **WGPU** or **Raylib** renderers for high-performance desktop apps
- See [Backend Renderers](runtime/KRYON_BACKEND_RENDERERS.md#wgpu-renderer-gpu) for setup

### Web Applications  
- Use **HTML** renderer to generate static web pages
- See [KRB to HTML Mapping](binary-format/KRB_TO_HTML_MAPPING.md) for web deployment

### Terminal Applications
- Use **Ratatui** renderer for command-line interfaces
- See [Backend Renderers](runtime/KRYON_BACKEND_RENDERERS.md#ratatui-renderer-terminal) for terminal UI

### Games and Interactive Media
- Use **Raylib** or **SDL2** renderers for games and multimedia
- Integrate with [Script System](runtime/KRYON_SCRIPT_SYSTEM.md) for game logic

## 🔧 Development Tools

### Renderers
```bash
# Desktop GPU rendering
kryon-renderer wgpu app.krb --width 1920 --height 1080

# Web generation
kryon-renderer html app.krb --output ./dist --responsive --accessibility

# Terminal UI
kryon-renderer ratatui app.krb

# Debug analysis
kryon-renderer debug app.krb --show-properties --show-layout
```

### Language Features
- **Reactive Variables**: Template variables with automatic UI updates
- **Lua Scripting**: Full Lua integration for dynamic behavior  
- **Multi-Backend**: Single codebase, multiple deployment targets
- **Component System**: Reusable UI components with inheritance
- **Responsive Layout**: Flexbox-based layout with Taffy engine

## 🐛 Debugging

When you encounter issues:

1. **Check Syntax**: Verify against [KRY Language Specification](language/KRY_LANGUAGE_SPECIFICATION.md)
2. **Find Error Code**: Look up error codes in [Error Reference](reference/KRYON_ERROR_REFERENCE.md)
3. **Debug Layout**: Use the debug renderer to analyze element hierarchy
4. **Inspect Binary**: Check the KRB format with debug tools

```bash
# Debug your KRB file
kryon-renderer debug app.krb --format detailed --show-properties --show-colors
```

## 📐 Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                       KRY Source                            │
│  Declarative UI description language with reactive vars    │
└─────────────────────┬───────────────────────────────────────┘
                      │ Compilation
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                    KRB Binary                               │
│  Optimized binary format with elements, styles, strings    │
└─────────────────────┬───────────────────────────────────────┘
                      │ Runtime Loading
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                  Kryon Runtime                              │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │  Template       │ │  Layout Engine  │ │  Script System  ││
│  │  Engine         │ │   (Taffy)       │ │   (Lua)         ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
└─────────────────────┬───────────────────────────────────────┘
                      │ Multi-Backend Rendering
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  WGPU  │  Raylib  │  HTML  │  Ratatui  │  SDL2  │  Debug   │
│ (GPU)  │ (Games)  │ (Web)  │ (Term)    │ (Native) │ (Dev)  │
└─────────────────────────────────────────────────────────────┘
```

## 🎨 Example Application

```kry
App {
    windowWidth: 1024
    windowHeight: 768
    windowTitle: "My Kryon App"
    
    @variables {
        counter: 0
        message: "Hello World"
        theme: "light"
    }
    
    @styles {
        "button_style" {
            backgroundColor: "#007ACC"
            borderRadius: 8
            padding: 12
            color: "#FFFFFF"
        }
    }
    
    Container {
        display: "flex"
        flexDirection: "column"
        alignItems: "center"
        justifyContent: "center"
        
        Text {
            text: $message
            fontSize: 24
            marginBottom: 20
        }
        
        Text {
            text: "Count: " + $counter
            fontSize: 18
            marginBottom: 20
        }
        
        Button {
            class: "button_style"
            text: "Increment"
            onClick: "incrementCounter()"
        }
    }
    
    @script {
        function incrementCounter() {
            local count = tonumber(kryon.getVariable("counter")) or 0
            kryon.setVariable("counter", tostring(count + 1))
        end
    }
}
```

This example demonstrates:
- Window configuration
- Reactive variables (`$counter`, `$message`)
- Style definitions
- Flexbox layout
- Lua scripting integration
- Template expressions

## 🤝 Contributing

To contribute to Kryon documentation:

1. Follow the existing structure and format
2. Add examples for new features
3. Update the appropriate reference files
4. Test examples with multiple renderers
5. Maintain backward compatibility

## 📄 License

This documentation is part of the Kryon project. Please refer to the main project license for terms and conditions.
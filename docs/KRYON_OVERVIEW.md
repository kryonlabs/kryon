# Kryon Overview v2.0
## Smart Hybrid System

## What is Kryon?

Kryon is a cross-platform UI framework that combines the familiar styling power of CSS with the structural benefits of widgets. It compiles declarative UI definitions (KRY files) into efficient binary format (KRB files) for runtime execution across desktop, web, and mobile platforms.

## Smart Hybrid Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    KRY Source Files                      │
│          (CSS-like Styles + Widget Layout)               │
│  style "button" { ... } + Column { Button { ... } }    │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                  Hybrid Compiler v2.0                    │
│    Lexer → Parser → Hybrid AST → Optimizer → Generator  │
│  + Style Resolution + Theme Variables + Widget Layout   │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                   KRB Hybrid Binary                      │
│     (Style Definitions + Theme Variables +               │
│      Widget Definitions + Widget Instances)              │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│                 Hybrid Runtime v2.0                      │
│  Loader → Style System → Theme System → Widget Layout   │
│       → Layout Engine → Renderer                        │
└─────────────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────┐
│              Advanced Platform Renderers                 │
│  Web (Style→CSS + Widget→HTML) │ Native │ Mobile        │
│    + CSS Custom Properties + Responsive Design          │
└─────────────────────────────────────────────────────────┘
```

## Key Components

### 1. Smart Hybrid Language v2.0
- **CSS-like styling** for familiar property-based appearance control
- **Widget composition** for powerful layout and structure
- **Enhanced theming** with organized variable groups
- **Style inheritance** with `extends` keyword
- **Responsive design** with breakpoint support
- **Theme switching** with light/dark mode support
- **Event handling** with script integration

### 2. Hybrid Compiler v2.0
- **Style resolution** and inheritance flattening
- **Theme variable** processing and optimization
- **Widget layout** analysis and optimization
- **CSS property** validation and optimization
- **Cross-platform** code generation
- **Style deduplication** and compression
- **Responsive value** optimization

### 3. KRB Hybrid Binary Format v2.0
- **Style definition table** with inheritance support
- **Theme variable table** with organized groups
- **Widget definition table** for layout widgets
- **Responsive value** support with breakpoints
- **Cross-platform** compatibility data
- **Efficient compression** with style awareness

### 4. Hybrid Runtime v2.0
- **Style system** with inheritance resolution
- **Theme system** with variable resolution and switching
- **Widget layout** engine with flexbox-like behavior
- **Responsive design** with breakpoint handling
- **Event handling** with script execution
- **Performance optimizations** with style caching

### 5. Platform Renderers
- **Web**: Style→CSS + Widget→HTML with CSS custom properties
- **Native**: Style→Platform styling + Widget→Native controls
- **Mobile**: Style→Platform themes + Widget→Native layout
- **Terminal**: Style→ANSI colors + Widget→Text layout

## Design Principles

1. **Best of Both Worlds**: Familiar CSS styling + Powerful widget layout
2. **Developer Friendly**: Intuitive syntax that doesn't require learning completely new concepts
3. **Performance First**: Compiled binary format with optimized style resolution
4. **Cross-Platform Excellence**: Write once, render natively everywhere
5. **Theme-Aware**: Built-in theming system with variable organization
6. **Responsive by Design**: Native responsive design with intelligent breakpoints
7. **Style Inheritance**: Reusable styles with inheritance for maintainability
8. **Future-Proof**: Extensible architecture for new features

## Use Cases

- **Modern Desktop Apps**: Native performance with familiar styling
- **Responsive Web Applications**: CSS-like styling with widget structure
- **Cross-Platform Mobile Apps**: Native feel with unified codebase
- **Design Systems**: Maintainable style libraries with inheritance
- **Rapid Prototyping**: Quick UI development with style reusability
- **Dashboard Applications**: Data-rich interfaces with responsive design
- **E-commerce Platforms**: Theme-aware shopping experiences
- **Content Management**: Style-consistent admin interfaces

## Getting Started with Smart Hybrid System

```kry
# Enhanced theme variables (organized variable groups)
@theme colors {
    primary: "#007AFF"
    secondary: "#34C759"
    background: "#ffffff"
    text: "#000000"
    border: "#e5e5e7"
}

@theme spacing {
    sm: 8
    md: 16
    lg: 24
}

# CSS-like styles with theme variables
style "card" {
    background: $colors.background
    borderRadius: 12
    padding: $spacing.lg
    border: "1px solid $colors.border"
    boxShadow: "0 2px 8px rgba(0,0,0,0.1)"
}

style "primaryButton" {
    background: $colors.primary
    color: $colors.background
    fontSize: 16
    fontWeight: 600
    borderRadius: 8
    padding: "$spacing.sm $spacing.md"
    border: "none"
    cursor: "pointer"
    transition: "all 0.2s ease"
}

# Widget layout for structure
App {
    windowWidth: 800
    windowHeight: 600
    background: $colors.background
    
    Center {
        child: Container {
            style: "card"
            width: 400
            
            Column {
                spacing: $spacing.md
                
                Text {
                    text: "Smart Hybrid Demo"
                    fontSize: 24
                    fontWeight: 700
                    color: $colors.text
                }
                
                Row {
                    spacing: $spacing.sm
                    mainAxis: "spaceBetween"
                    
                    Button {
                        text: "Cancel"
                        background: "transparent"
                        color: $colors.text
                        border: "1px solid $colors.border"
                    }
                    
                    Button {
                        text: "Save"
                        style: "primaryButton"
                        onClick: "handleSave"
                    }
                }
            }
        }
    }
}
```

Compile and run with hybrid features:
```bash
# Compile with style and theme optimizations
kryon compile app.kry -o app.krb --optimize --responsive

# Run with theme switching support
kryon run app.krb --theme light

# Development mode with hot reload
kryon dev app.kry --hot-reload --theme auto
```

## Documentation Structure

1. **KRY_LANGUAGE_SPEC.md** - Complete hybrid language reference v2.0
2. **KRB_BINARY_SPEC.md** - Hybrid binary format specification v2.0  
3. **KRB_TO_HTML_MAPPING.md** - Hybrid system to HTML/CSS mapping v2.0
4. **KRYON_IR_PIPELINE.md** - Hybrid compilation pipeline details v2.0
5. **KRYON_OVERVIEW.md** - This hybrid framework overview v2.0

## Project Status

### Core Hybrid System v2.0
- ✅ Smart hybrid language specification complete
- ✅ CSS-like styling with widget layout design
- ✅ Enhanced theming with variable groups
- ✅ Style inheritance system
- ✅ Binary format v2.0 with hybrid support
- ✅ Responsive design system
- ✅ Theme switching capabilities

### Implementation Status
- ✅ Hybrid language specification complete
- ✅ Hybrid binary format specification complete
- ✅ Hybrid HTML mapping complete
- ✅ Hybrid compilation pipeline design complete
- ✅ Framework overview updated
- 🚧 Hybrid compiler implementation
- 🚧 Hybrid runtime implementation
- 🚧 Style system implementation
- 🚧 Theme system implementation
- 🚧 Widget layout engine

### Platform Renderers
- 🚧 Web renderer with CSS generation
- 🚧 Native desktop renderers
- 🚧 Mobile platform renderers
- 🚧 Terminal renderer

### Developer Experience
- 🚧 Style development tools
- 🚧 Theme editor and preview
- 🚧 Widget inspector
- 🚧 Responsive design tools
- 🚧 Hot reload system

### Advanced Features
- 🔮 Visual style editor
- 🔮 Theme marketplace
- 🔮 Advanced accessibility features
- 🔮 Performance profiling tools
- 🔮 Cross-platform testing framework

Kryon's Smart Hybrid System gives you the familiar power of CSS styling combined with the structural benefits of widget composition, enhanced with a practical theming system that builds naturally on variable organization.
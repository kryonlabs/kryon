# Kryon Backend Renderers Documentation

Complete documentation of Kryon's multi-backend rendering system, supporting WGPU, Raylib, HTML, Ratatui, SDL2, and Debug backends.

> **🔗 Related Documentation**: 
> - [KRYON_RUNTIME_SYSTEM.md](KRYON_RUNTIME_SYSTEM.md) - Runtime system architecture
> - [KRYON_LAYOUT_ENGINE.md](KRYON_LAYOUT_ENGINE.md) - Layout system integration

## Table of Contents
- [Renderer Architecture Overview](#renderer-architecture-overview)
- [Command Renderer System](#command-renderer-system)
- [WGPU Renderer (GPU)](#wgpu-renderer-gpu)
- [Raylib Renderer (Graphics)](#raylib-renderer-graphics)
- [HTML Renderer (Web)](#html-renderer-web)
- [Ratatui Renderer (Terminal)](#ratatui-renderer-terminal)
- [SDL2 Renderer (Native)](#sdl2-renderer-native)
- [Debug Renderer (Analysis)](#debug-renderer-analysis)
- [Renderer Selection & CLI](#renderer-selection--cli)
- [Common Features](#common-features)
- [Performance Considerations](#performance-considerations)

## Renderer Architecture Overview

Kryon uses a modular, multi-backend rendering architecture that allows the same KRB content to be rendered across different platforms and environments:

```
┌─────────────────────────────────────────────────────────────┐
│                    KryonApp Runtime                        │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │   KRB Parser    │ │  Layout Engine  │ │ Template Engine ││
│  │   (.krb file)   │ │    (Taffy)      │ │  (Variables)    ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  CommandRenderer │
                    │   (Trait)       │
                    └─────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│    WGPU     │    │   Raylib    │    │    HTML     │
│  (Desktop/  │    │ (Graphics)  │    │   (Web)     │
│  Mobile)    │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
        │                     │                     │
        ▼                     ▼                     ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Ratatui   │    │    SDL2     │    │   Debug     │
│ (Terminal)  │    │  (Native)   │    │ (Analysis)  │
│             │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
```

### Core Renderer Trait

All renderers implement the common `Renderer` trait:

```rust
pub trait Renderer {
    type Surface;
    type Context;
    
    fn initialize(surface: Self::Surface) -> RenderResult<Self>;
    fn begin_frame(&mut self, clear_color: Vec4) -> RenderResult<Self::Context>;
    fn end_frame(&mut self, context: Self::Context) -> RenderResult<()>;
    fn render_element(&mut self, context: &mut Self::Context, element: &Element, 
                     layout: &LayoutResult, element_id: ElementId) -> RenderResult<()>;
    fn resize(&mut self, new_size: Vec2) -> RenderResult<()>;
}
```

## Command Renderer System

Kryon uses a command-based rendering system that separates rendering logic from platform-specific implementations:

### Render Commands

```rust
#[derive(Debug, Clone)]
pub enum RenderCommand {
    DrawRect {
        position: Vec2,
        size: Vec2,
        color: Vec4,
        border_radius: f32,
        border_width: f32,
        border_color: Vec4,
    },
    DrawText {
        text: String,
        position: Vec2,
        font_size: f32,
        color: Vec4,
        font_family: Option<String>,
        alignment: TextAlignment,
    },
    DrawImage {
        path: String,
        position: Vec2,
        size: Vec2,
        opacity: f32,
    },
    SetClipRegion {
        position: Vec2,
        size: Vec2,
    },
    ClearClipRegion,
    BeginTransform(Mat4),
    EndTransform,
}
```

### Command Processing

```rust
impl CommandRenderer for SpecificRenderer {
    fn execute_commands(&mut self, commands: &[RenderCommand]) -> RenderResult<()> {
        for command in commands {
            match command {
                RenderCommand::DrawRect { position, size, color, .. } => {
                    self.draw_rectangle(*position, *size, *color)?;
                }
                RenderCommand::DrawText { text, position, font_size, color, .. } => {
                    self.draw_text(text, *position, *font_size, *color)?;
                }
                // ... handle other commands
            }
        }
        Ok(())
    }
}
```

## WGPU Renderer (GPU)

High-performance GPU-accelerated renderer using WebGPU/WGPU for desktop and mobile platforms.

### Features

- **GPU Acceleration**: Hardware-accelerated rendering with compute shaders
- **Cross-Platform**: Works on Windows, macOS, Linux, and mobile devices  
- **Modern Graphics**: Uses modern graphics APIs (Vulkan, Metal, DirectX 12)
- **Text Rendering**: Hardware-accelerated text with font caching
- **Shader Support**: Custom WGSL shaders for effects

### Architecture

```rust
pub struct WgpuRenderer {
    surface: wgpu::Surface<'static>,
    device: wgpu::Device,
    queue: wgpu::Queue,
    config: wgpu::SurfaceConfiguration,
    size: Vec2,
    
    // Rendering pipelines
    rect_pipeline: wgpu::RenderPipeline,
    text_pipeline: wgpu::RenderPipeline,
    
    // Uniform buffers
    view_proj_buffer: wgpu::Buffer,
    view_proj_bind_group: wgpu::BindGroup,
    
    // Text rendering
    text_renderer: TextRenderer,
    
    // Resource management
    _resource_manager: ResourceManager,
    
    // Vertex buffers (reusable)
    vertex_buffer: wgpu::Buffer,
    index_buffer: wgpu::Buffer,
}
```

### Initialization

```bash
# WGPU renderer with window configuration
kryon-renderer wgpu app.krb --width 1920 --height 1080 --title "My App"

# With debug logging
kryon-renderer wgpu app.krb --debug

# Standalone mode (wraps non-App elements)
kryon-renderer wgpu container.krb --standalone
```

### Shader Pipeline

WGPU renderer uses custom WGSL shaders:

**Rectangle Shader** (`rect.wgsl`):
```wgsl
struct Vertex {
    @location(0) position: vec2<f32>,
    @location(1) tex_coords: vec2<f32>,
    @location(2) color: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) tex_coords: vec2<f32>,
    @location(1) color: vec4<f32>,
}

@vertex
fn vs_main(model: Vertex) -> VertexOutput {
    var out: VertexOutput;
    out.tex_coords = model.tex_coords;
    out.color = model.color;
    out.clip_position = view_proj * vec4<f32>(model.position, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return in.color;
}
```

### Performance Optimizations

- **Batch Rendering**: Groups similar draw calls
- **Vertex Buffer Reuse**: Reuses buffers across frames
- **GPU Memory Management**: Efficient resource allocation
- **Instanced Rendering**: Reduces draw calls for repeated elements

## Raylib Renderer (Graphics)

Simple, easy-to-use graphics renderer built on the Raylib library, perfect for prototyping and games.

### Features

- **Simple Graphics**: Easy-to-understand 2D rendering
- **Built-in Audio**: Sound effects and music support
- **Font Management**: Automatic font loading and rendering
- **Input Handling**: Mouse, keyboard, and gamepad support
- **Screenshot Capability**: Built-in screenshot functionality

### Initialization

```bash
# Raylib renderer with custom window settings
kryon-renderer raylib app.krb --width 800 --height 600 --title "Game"

# Take screenshot after 1 second and exit
kryon-renderer raylib app.krb --screenshot output.png --screenshot-delay 1000
```

### Font System

Raylib renderer supports automatic font detection and loading:

```rust
fn register_fonts_from_krb(renderer: &mut RaylibRenderer, krb_file: &KRBFile) {
    // Register fonts from KRB font mappings
    for (font_family, font_path) in &krb_file.fonts {
        renderer.register_font(font_family, font_path);
        if let Err(e) = renderer.load_font(font_family) {
            eprintln!("Warning: Failed to load font '{}': {}", font_family, e);
        }
    }
    
    // Fallback: scan strings for font patterns
    for (i, string) in krb_file.strings.iter().enumerate() {
        if string.ends_with(".ttf") || string.ends_with(".otf") {
            if i > 0 {
                let font_name = &krb_file.strings[i - 1];
                renderer.register_font(font_name, string);
            }
        }
    }
}
```

### Window Properties

Raylib renderer reads window configuration from KRB App elements:

```kry
App {
    windowWidth: 1024
    windowHeight: 768
    windowTitle: "My Raylib App"
    
    Container {
        # App content here
    }
}
```

## HTML Renderer (Web)

Generates static HTML/CSS/JavaScript from KRB files for web deployment.

### Features

- **Static Generation**: Creates deployable web pages
- **Responsive Design**: Automatic responsive styles
- **Accessibility**: ARIA attributes and semantic HTML
- **Dark Mode**: Automatic dark theme generation
- **CSS Variables**: Dynamic theming support
- **Development Server**: Live preview with hot reload

### Usage

```bash
# Generate HTML files
kryon-renderer html app.krb --output ./dist --filename index

# Generate with optimizations
kryon-renderer html app.krb --inline-css --minify --responsive --accessibility

# Start development server
kryon-renderer html app.krb --serve --port 3000 --hot-reload
```

### HTML Generation

```rust
pub struct HtmlRenderer {
    instance_id: String,
    viewport_size: Vec2,
    elements: Vec<HtmlElement>,
    css_rules: Vec<CssRule>,
    js_code: String,
    canvas_elements: Vec<CanvasElement>
}

impl HtmlRenderer {
    pub fn generate_html(&self, config: &HtmlConfig) -> String {
        let mut html = String::new();
        
        html.push_str("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
        html.push_str(&format!("    <title>{}</title>\n", config.title));
        html.push_str("    <meta charset=\"UTF-8\">\n");
        html.push_str("    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
        
        // Inline or external CSS
        if config.inline_css {
            html.push_str("    <style>\n");
            html.push_str(&self.generate_css(config));
            html.push_str("    </style>\n");
        } else {
            html.push_str(&format!("    <link rel=\"stylesheet\" href=\"kryon-{}.css\">\n", self.instance_id));
        }
        
        html.push_str("</head>\n<body>\n");
        
        // Generate HTML elements
        for element in &self.elements {
            html.push_str(&element.to_html());
        }
        
        // Inline or external JavaScript
        if config.inline_js {
            html.push_str("    <script>\n");
            html.push_str(&self.generate_js(config));
            html.push_str("    </script>\n");
        } else {
            html.push_str(&format!("    <script src=\"kryon-{}.js\"></script>\n", self.instance_id));
        }
        
        html.push_str("</body>\n</html>");
        html
    }
}
```

### CSS Generation

The HTML renderer generates modern CSS with:

- **Flexbox Layout**: Converts Taffy layout to CSS flexbox
- **CSS Grid**: For complex layouts
- **CSS Variables**: For dynamic theming
- **Responsive Breakpoints**: Mobile-first design
- **Accessibility Styles**: High contrast, focus indicators

### JavaScript Interactivity

```javascript
// Generated JavaScript for template variables
class KryonTemplateEngine {
    constructor() {
        this.variables = new Map();
        this.bindings = new Map();
    }
    
    setVariable(name, value) {
        if (this.variables.get(name) !== value) {
            this.variables.set(name, value);
            this.updateBindings(name);
        }
    }
    
    updateBindings(variableName) {
        const elements = this.bindings.get(variableName) || [];
        elements.forEach(({ elementId, property, expression }) => {
            const element = document.getElementById(elementId);
            if (element) {
                const value = this.evaluateExpression(expression);
                this.applyValue(element, property, value);
            }
        });
    }
}
```

## Ratatui Renderer (Terminal)

Terminal-based UI renderer for command-line applications using the Ratatui library.

### Features

- **Terminal UI**: Rich terminal interfaces
- **Cross-Platform**: Works on any terminal
- **Mouse Support**: Click interactions in terminal
- **Keyboard Navigation**: Full keyboard support
- **Unicode Support**: Proper text rendering
- **No Graphics Dependencies**: Pure terminal output

### Usage

```bash
# Terminal UI renderer
kryon-renderer ratatui app.krb

# Inspect KRB without rendering
kryon-renderer ratatui app.krb --inspect

# Standalone mode
kryon-renderer ratatui component.krb --standalone
```

### Terminal Controls

- **q** or **Escape**: Quit application
- **Mouse clicks**: Interact with buttons and elements
- **Resize**: Automatic terminal resize handling

### Implementation

```rust
pub struct RatatuiRenderer {
    terminal: Terminal<CrosstermBackend<io::Stdout>>,
    elements: Vec<TerminalElement>,
}

impl RatatuiRenderer {
    pub fn render_frame(&mut self, elements: &[TerminalElement]) -> Result<()> {
        self.terminal.draw(|f| {
            let chunks = Layout::default()
                .direction(Direction::Vertical)
                .constraints([Constraint::Min(0)].as_ref())
                .split(f.size());
            
            for element in elements {
                match element.element_type {
                    ElementType::Container => {
                        self.render_container(f, element, chunks[0]);
                    }
                    ElementType::Text => {
                        self.render_text(f, element, chunks[0]);
                    }
                    ElementType::Button => {
                        self.render_button(f, element, chunks[0]);
                    }
                    _ => {}
                }
            }
        })?;
        
        Ok(())
    }
}
```

## SDL2 Renderer (Native)

Native desktop renderer using SDL2 for direct hardware access and precise control.

### Features

- **Native Performance**: Direct hardware acceleration
- **Precise Control**: Low-level graphics control
- **Audio Support**: Built-in audio capabilities
- **Input Handling**: Comprehensive input system
- **Cross-Platform**: Windows, macOS, Linux support

### Usage

```bash
# SDL2 renderer
kryon-renderer sdl2 app.krb --width 1200 --height 800

# With custom title
kryon-renderer sdl2 app.krb --title "SDL2 Application"
```

### SDL2 Integration

```rust
pub struct Sdl2Renderer {
    sdl_context: sdl2::Sdl,
    video_subsystem: sdl2::VideoSubsystem,
    window: sdl2::video::Window,
    canvas: sdl2::render::Canvas<sdl2::video::Window>,
    event_pump: sdl2::EventPump,
    texture_creator: sdl2::render::TextureCreator<sdl2::video::WindowContext>,
}

impl Sdl2Renderer {
    pub fn draw_rect(&mut self, pos: Vec2, size: Vec2, color: Vec4) -> Result<()> {
        self.canvas.set_draw_color(Color::RGBA(
            (color.x * 255.0) as u8,
            (color.y * 255.0) as u8,
            (color.z * 255.0) as u8,
            (color.w * 255.0) as u8,
        ));
        
        let rect = Rect::new(
            pos.x as i32,
            pos.y as i32,
            size.x as u32,
            size.y as u32,
        );
        
        self.canvas.fill_rect(rect)?;
        Ok(())
    }
}
```

## Debug Renderer (Analysis)

Text-based renderer for analyzing KRB file structure and debugging layout issues.

### Features

- **Tree Visualization**: Hierarchical element display
- **Property Inspection**: Detailed property analysis
- **Layout Debugging**: Position and size information
- **Color Analysis**: Color value inspection
- **JSON Export**: Machine-readable output
- **File Export**: Save analysis to files

### Usage

```bash
# Tree view (default)
kryon-renderer debug app.krb

# Detailed analysis with all properties
kryon-renderer debug app.krb --format detailed --show-properties --show-layout --show-colors

# Export to JSON
kryon-renderer debug app.krb --format json --output analysis.json

# Save tree view to file
kryon-renderer debug app.krb --output debug.txt
```

### Output Formats

**Tree Format:**
```
=== KRB FILE OVERVIEW ===
Elements: 5, Styles: 2, Strings: 8
Root Element: Some(0)

=== ELEMENT TREE ===
App "My Application" pos:(0,0) size:(800,600)
├── Container pos:(0,0) size:(800,100) [bg:#333333FF]
│   └── Text "Header" pos:(20,20) size:(760,60) [color:#FFFFFFFF]
├── Container pos:(0,100) size:(800,400) [bg:#F0F0F0FF]
│   └── Button "Click Me" pos:(350,250) size:(100,30) [bg:#007ACCFF]
└── Container pos:(0,500) size:(800,100) [bg:#333333FF]
```

**Detailed Format:**
```
=== KRYON BINARY FILE ANALYSIS ===

HEADER:
  Version: 1.0
  Flags: 0x0000

STRING TABLE:
  [0]: "My Application"
  [1]: "Header"
  [2]: "Click Me"
  [3]: "#333333"
  [4]: "#FFFFFF"

ELEMENT TREE:
App "My Application"
    • Applied style: 'app_style' (id: 1)
    • COMPUTED FINAL VALUES:
    • text: "My Application"
    • background_color: #00000000
    • layout_flags: 0x01
```

## Renderer Selection & CLI

### Main Renderer CLI

The main `kryon-renderer` binary provides a unified interface:

```bash
# WGPU (GPU-accelerated desktop)
kryon-renderer wgpu app.krb [OPTIONS]

# Raylib (simple graphics)
kryon-renderer raylib app.krb [OPTIONS]

# HTML (web output)
kryon-renderer html app.krb [OPTIONS]

# Ratatui (terminal UI)
kryon-renderer ratatui app.krb [OPTIONS]

# SDL2 (native desktop)
kryon-renderer sdl2 app.krb [OPTIONS]

# Debug (analysis)
kryon-renderer debug app.krb [OPTIONS]
```

### Common Options

| Option | Description | Available In |
|--------|-------------|--------------|
| `--width` | Window width | WGPU, Raylib, SDL2 |
| `--height` | Window height | WGPU, Raylib, SDL2 |
| `--title` | Window title | WGPU, Raylib, SDL2 |
| `--debug` | Enable debug logging | All |
| `--standalone` | Auto-wrap non-App elements | WGPU, Raylib, Ratatui, SDL2 |

### Backend-Specific Options

**HTML Renderer:**
- `--output`: Output directory
- `--inline-css`: Inline CSS instead of external file
- `--inline-js`: Inline JavaScript instead of external file
- `--minify`: Minify generated code
- `--responsive`: Generate responsive styles
- `--accessibility`: Generate accessibility features
- `--serve`: Start development server
- `--hot-reload`: Enable hot reload

**Debug Renderer:**
- `--format`: Output format (tree, json, detailed)
- `--output`: Save to file
- `--show-properties`: Show element properties
- `--show-layout`: Show position/size info
- `--show-colors`: Show color values in hex

**Raylib Renderer:**
- `--screenshot`: Take screenshot and exit
- `--screenshot-delay`: Delay before screenshot (ms)

## Common Features

### Window Configuration

All GUI renderers support reading window properties from KRB App elements:

```kry
App {
    windowWidth: 1920
    windowHeight: 1080
    windowTitle: "My Application"
    
    # Application content
}
```

CLI arguments override KRB file settings:
```bash
kryon-renderer wgpu app.krb --width 1024 --height 768 --title "Override Title"
```

### Standalone Mode

Standalone mode automatically wraps non-App root elements:

```bash
# This Container will be wrapped in an auto-generated App
kryon-renderer wgpu container.krb --standalone
```

Generated wrapper:
```kry
App {
    id: "auto_generated_app"
    windowWidth: 800  # Default or CLI override
    windowHeight: 600 # Default or CLI override
    
    # Original content inserted here
    Container {
        # ... original element content
    }
}
```

### Input Event Handling

All interactive renderers support common input events:

```rust
pub enum InputEvent {
    MousePress { position: Vec2, button: MouseButton },
    MouseRelease { position: Vec2, button: MouseButton },
    MouseMove { position: Vec2 },
    KeyPress { key: KeyCode, modifiers: KeyModifiers },
    KeyRelease { key: KeyCode, modifiers: KeyModifiers },
    Resize { size: Vec2 },
    WindowClose,
}
```

### Error Handling

All renderers use consistent error handling:

```rust
pub enum RenderError {
    InitializationFailed(String),
    RenderFailed(String),
    ResourceLoadFailed(String),
    UnsupportedFeature(String),
    InvalidInput(String),
}

pub type RenderResult<T> = Result<T, RenderError>;
```

## Performance Considerations

### Renderer Performance Comparison

| Renderer | Performance | Use Case | Pros | Cons |
|----------|-------------|----------|------|------|
| **WGPU** | ⭐⭐⭐⭐⭐ | Desktop/Mobile Apps | GPU acceleration, modern graphics | Complex setup, larger binary |
| **Raylib** | ⭐⭐⭐⭐ | Games, Prototypes | Simple API, built-in features | Less optimized than WGPU |
| **SDL2** | ⭐⭐⭐⭐ | Native Desktop | Direct control, mature | Manual memory management |
| **HTML** | ⭐⭐⭐ | Web Deployment | Web standards, responsive | Static output only |
| **Ratatui** | ⭐⭐⭐ | CLI Tools | No dependencies, universal | Limited visual capabilities |
| **Debug** | ⭐⭐ | Development | Analysis tools | No visual output |

### Optimization Tips

**WGPU Renderer:**
- Use batch rendering for similar elements
- Minimize shader state changes
- Reuse vertex buffers
- Enable GPU debug layers only during development

**Raylib Renderer:**
- Preload fonts and textures
- Use sprite batching for repeated elements
- Optimize draw call order
- Enable VSync for smooth animation

**HTML Renderer:**
- Inline critical CSS for faster loading
- Minimize JavaScript bundle size
- Use responsive images
- Enable compression on the web server

**All Renderers:**
- Profile layout computation performance
- Use incremental layout updates
- Cache computed results
- Minimize template variable updates

### Memory Usage

| Renderer | Memory Usage | Notes |
|----------|--------------|-------|
| WGPU | High | GPU buffers, shader compilation |
| Raylib | Medium | Texture caching, font loading |
| SDL2 | Medium | Surface management, audio buffers |
| HTML | Low | Static generation, no runtime |
| Ratatui | Low | Terminal-only, minimal state |
| Debug | Low | Text output only |

This comprehensive renderer system allows Kryon applications to target multiple platforms and use cases while maintaining a consistent development experience and feature set across all backends.
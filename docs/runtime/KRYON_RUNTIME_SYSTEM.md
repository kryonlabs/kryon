# Kryon Runtime System Architecture

Complete documentation of Kryon's runtime system, covering the application lifecycle, component systems, and execution model.

> **🔗 Related Documentation**: 
> - [KRYON_ERROR_REFERENCE.md](KRYON_ERROR_REFERENCE.md) - Error codes and debugging
> - [KRB_BINARY_FORMAT_SPECIFICATION.md](KRB_BINARY_FORMAT_SPECIFICATION.md) - Binary format details

## Table of Contents
- [Architecture Overview](#architecture-overview)
- [KryonApp Core](#kryonapp-core)
- [System Components](#system-components)
- [Application Lifecycle](#application-lifecycle)
- [Event System](#event-system)
- [State Management](#state-management)
- [Memory Management](#memory-management)
- [Performance Characteristics](#performance-characteristics)

## Architecture Overview

Kryon Runtime follows a modular, system-based architecture designed for performance and flexibility across multiple backends.

### System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    KryonApp<R>                              │
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │   ScriptSystem  │ │ TemplateEngine  │ │  EventSystem    ││
│  │   (Lua/JS)      │ │ (Reactive Vars) │ │ (Input/Events)  ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
│  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐│
│  │  LayoutEngine   │ │ ElementRenderer │ │ StyleComputer   ││
│  │   (Taffy)       │ │   (Backend R)   │ │   (CSS-like)    ││
│  └─────────────────┘ └─────────────────┘ └─────────────────┘│
└─────────────────────────────────────────────────────────────┘
            │                    │                    │
    ┌───────▼───────┐    ┌───────▼───────┐    ┌───────▼───────┐
    │  WGPU Backend │    │ Raylib Backend│    │  HTML Backend │
    │  (Desktop/Web)│    │   (Games)     │    │    (Web)      │
    └───────────────┘    └───────────────┘    └───────────────┘
```

### Core Principles

1. **Backend Agnostic**: Single codebase runs on multiple renderers
2. **Reactive**: Template variables automatically update UI
3. **Script-Driven**: Lua/JavaScript for dynamic behavior
4. **Performance-First**: Optimized layout and rendering pipelines
5. **Memory Efficient**: Minimal allocations and smart caching

## KryonApp Core

The `KryonApp<R>` struct is the central coordinator of the entire runtime system.

### Core Structure

```rust
pub struct KryonApp<R: CommandRenderer> {
    // Core data
    krb_file: KRBFile,                      // Compiled UI description
    elements: HashMap<ElementId, Element>,   // Live element tree
    
    // Systems
    layout_engine: Box<dyn LayoutEngine>,   // Layout computation
    renderer: ElementRenderer<R>,           // Backend rendering
    event_system: EventSystem,              // Input handling
    script_system: ScriptSystem,            // Lua/JS execution
    template_engine: TemplateEngine,        // Reactive variables
    
    // State
    layout_result: LayoutResult,            // Computed positions/sizes
    viewport_size: Vec2,                    // Window dimensions
    needs_layout: bool,                     // Layout dirty flag
    needs_render: bool,                     // Render dirty flag
    
    // Focus management
    focused_element: Option<ElementId>,     // Currently focused element
    tab_order: Vec<ElementId>,              // Tab navigation order
    
    // Performance
    frame_count: u64,                       // Frame counter
    last_frame_time: Instant,               // For FPS calculation
}
```

### Initialization Process

```rust
impl<R: CommandRenderer> KryonApp<R> {
    pub fn new(krb_path: &str, renderer: R) -> anyhow::Result<Self> {
        // 1. Load and parse KRB file
        let krb_file = load_krb_file(krb_path)?;
        
        // 2. Initialize core systems
        let style_computer = StyleComputer::new(&elements, &krb_file.styles);
        let layout_engine = Box::new(TaffyLayoutEngine::new());
        let renderer = ElementRenderer::new(renderer, style_computer.clone());
        
        // 3. Initialize subsystems
        let event_system = EventSystem::new();
        let script_system = ScriptSystem::new()?;
        let template_engine = TemplateEngine::new(&krb_file);
        
        // 4. Setup script system with KRB data
        script_system.initialize(&krb_file, &elements)?;
        script_system.load_compiled_scripts(&krb_file.scripts)?;
        
        // 5. Initialize template variables
        script_system.initialize_template_variables(&template_variables)?;
        
        // 6. Execute initialization scripts
        script_system.execute_init_functions()?;
        
        // 7. Compute initial layout
        app.update_layout()?;
        
        Ok(app)
    }
}
```

## System Components

### Layout Engine

Handles all UI layout computation using the Taffy layout engine.

```rust
pub trait LayoutEngine {
    fn compute_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        root_id: ElementId,
        viewport_size: Vec2,
    ) -> LayoutResult;
}

pub struct TaffyLayoutEngine {
    taffy: taffy::Taffy,
    node_map: HashMap<ElementId, taffy::NodeId>,
}
```

**Key Features**:
- **Flexbox Support**: Full flexbox layout with grow/shrink/basis
- **Grid Layout**: CSS Grid-compatible grid system
- **Constraint Solving**: Automatic constraint resolution
- **Incremental Updates**: Only recomputes changed subtrees
- **Performance Optimized**: Uses Taffy's efficient algorithms

### Element Renderer

Abstracts rendering across different backends through the `CommandRenderer` trait.

```rust
pub trait CommandRenderer {
    fn viewport_size(&self) -> Vec2;
    fn resize(&mut self, size: Vec2) -> anyhow::Result<()>;
    fn begin_frame(&mut self, clear_color: Vec4) -> anyhow::Result<()>;
    fn end_frame(&mut self) -> anyhow::Result<()>;
    
    // Primitive rendering
    fn draw_rect(&mut self, cmd: DrawRectCommand) -> anyhow::Result<()>;
    fn draw_text(&mut self, cmd: DrawTextCommand) -> anyhow::Result<()>;
    fn draw_image(&mut self, cmd: DrawImageCommand) -> anyhow::Result<()>;
}

pub struct ElementRenderer<R: CommandRenderer> {
    backend: R,
    style_computer: StyleComputer,
    text_manager: TextManager,
}
```

**Rendering Pipeline**:
1. **Style Computation**: Resolve all element styles
2. **Command Generation**: Convert elements to render commands
3. **Backend Execution**: Execute commands on specific renderer
4. **Resource Management**: Handle textures, fonts, etc.

### Event System

Manages all user input and UI events with proper focus handling.

```rust
pub struct EventSystem {
    pending_events: Vec<InputEvent>,
    event_handlers: HashMap<ElementId, HashMap<EventType, String>>,
}

pub enum InputEvent {
    MouseMove { position: Vec2 },
    MousePress { position: Vec2, button: MouseButton },
    MouseRelease { position: Vec2, button: MouseButton },
    KeyPress { key: KeyCode, modifiers: KeyModifiers },
    TextInput { text: String },
    Resize { size: Vec2 },
}
```

**Event Flow**:
1. **Input Capture**: Collect events from platform
2. **Hit Testing**: Determine which element was targeted
3. **Focus Management**: Handle focus changes and tab navigation
4. **Event Dispatch**: Call appropriate handlers
5. **State Updates**: Apply resulting state changes

### Script System

Executes Lua/JavaScript code with full DOM API access.

```rust
pub struct ScriptSystem {
    registry: ScriptRegistry,                    // Engine management
    template_variables: HashMap<String, String>, // Reactive variables
    elements_data: HashMap<ElementId, Element>,  // DOM access
    bridge_data: Option<BridgeData>,            // API bridge
}
```

**Script Architecture**:
- **Multi-Language**: Supports Lua, JavaScript, Python, Wren
- **DOM Bridge**: Full element access and manipulation
- **Reactive Variables**: Automatic UI updates on variable changes
- **Event Handlers**: Direct integration with UI events
- **Bytecode Execution**: Pre-compiled scripts for performance

### Template Engine

Manages reactive template variables with automatic UI updates.

```rust
pub struct TemplateEngine {
    variables: HashMap<String, String>,          // Current values
    bindings: Vec<TemplateBinding>,             // Element bindings
    change_listeners: Vec<ChangeListener>,      // Update callbacks
}

pub struct TemplateBinding {
    variable_name: String,
    element_id: ElementId,
    property_name: String,
    binding_type: BindingType,
}
```

**Reactive Flow**:
1. **Variable Change**: Script or code updates template variable
2. **Binding Resolution**: Find all elements using this variable
3. **Property Update**: Update element properties with new values
4. **Layout Invalidation**: Mark layout as needing recalculation
5. **Render Update**: Trigger re-render with new values

## Application Lifecycle

### Frame Loop

The main application loop follows a consistent pattern:

```rust
impl<R: CommandRenderer> KryonApp<R> {
    pub fn run_frame(&mut self, delta_time: Duration) -> anyhow::Result<()> {
        // 1. Update systems
        self.update(delta_time)?;
        
        // 2. Process events
        self.handle_pending_events()?;
        
        // 3. Update layout if needed
        if self.needs_layout {
            self.update_layout()?;
            self.needs_layout = false;
            self.needs_render = true;
        }
        
        // 4. Render if needed
        if self.needs_render {
            self.render()?;
            self.needs_render = false;
        }
        
        // 5. Update performance metrics
        self.update_frame_metrics(delta_time);
        
        Ok(())
    }
}
```

### Update Phase

```rust
pub fn update(&mut self, delta_time: Duration) -> anyhow::Result<()> {
    // 1. Process script changes
    let pending_changes = self.script_system.get_pending_changes()?;
    
    // 2. Apply template variable changes
    if let Some(template_changes) = pending_changes.get("template_variables") {
        for (name, value) in &template_changes.data {
            self.set_template_variable(name, value)?;
        }
    }
    
    // 3. Apply DOM changes
    let changes_applied = self.script_system.apply_pending_dom_changes(
        &mut self.elements, 
        &pending_changes
    )?;
    
    // 4. Clear processed changes
    self.script_system.clear_pending_changes()?;
    
    // 5. Update event system
    self.event_system.update(&mut self.elements)?;
    
    // 6. Mark for re-render if changes occurred
    if changes_applied {
        self.needs_render = true;
    }
    
    Ok(())
}
```

### Render Phase

```rust
pub fn render(&mut self) -> anyhow::Result<()> {
    if !self.needs_render {
        return Ok(());
    }
    
    if let Some(root_id) = self.krb_file.root_element_id {
        let clear_color = Vec4::new(0.1, 0.1, 0.1, 1.0);
        
        // Render entire element tree
        self.renderer.render_frame(
            &self.elements,
            &self.layout_result,
            root_id,
            clear_color,
        )?;
    }
    
    self.needs_render = false;
    self.frame_count += 1;
    
    Ok(())
}
```

## Event System

### Input Event Processing

```rust
pub fn handle_input(&mut self, event: InputEvent) -> anyhow::Result<()> {
    match event {
        InputEvent::Resize { size } => {
            self.viewport_size = size;
            self.renderer.resize(size)?;
            self.needs_layout = true;
        }
        InputEvent::MouseMove { position } => {
            self.handle_mouse_move(position)?;
        }
        InputEvent::MousePress { position, button } => {
            self.handle_mouse_press(position, button)?;
        }
        InputEvent::MouseRelease { position, button } => {
            self.handle_mouse_release(position, button)?;
        }
        InputEvent::KeyPress { key, modifiers } => {
            self.handle_key_press(key, modifiers)?;
        }
        InputEvent::TextInput { text } => {
            self.handle_text_input(&text)?;
        }
    }
    
    Ok(())
}
```

### Hit Testing

```rust
fn find_element_at_position(&self, position: Vec2) -> Option<ElementId> {
    let mut found_elements = Vec::new();
    
    for (element_id, element) in &self.elements {
        if !element.visible {
            continue;
        }
        
        let element_pos = self.layout_result.computed_positions
            .get(element_id)
            .copied()
            .unwrap_or_default();
        let element_size = self.layout_result.computed_sizes
            .get(element_id)
            .copied()
            .unwrap_or_default();
        
        // Check if position is within element bounds
        if position.x >= element_pos.x
            && position.x <= element_pos.x + element_size.x
            && position.y >= element_pos.y
            && position.y <= element_pos.y + element_size.y
        {
            found_elements.push(*element_id);
        }
    }
    
    // Return topmost element (highest ID)
    found_elements.into_iter().max()
}
```

### Focus Management

```rust
fn set_focus(&mut self, element_id: ElementId) -> anyhow::Result<()> {
    // Clear previous focus
    if let Some(prev_focused) = self.focused_element {
        if let Some(prev_element) = self.elements.get_mut(&prev_focused) {
            prev_element.set_focus(false);
        }
    }
    
    // Set new focus
    if let Some(element) = self.elements.get_mut(&element_id) {
        if element.can_receive_focus() {
            element.set_focus(true);
            self.focused_element = Some(element_id);
            self.needs_render = true;
            
            // Initialize input state for input elements
            if element.element_type == ElementType::Input {
                element.initialize_input_state(InputType::Text);
            }
        }
    }
    
    Ok(())
}
```

## State Management

### Element State Tracking

Each element maintains multiple state aspects:

```rust
pub struct Element {
    // Identity
    pub id: String,
    pub element_type: ElementType,
    
    // Visual state
    pub visible: bool,
    pub current_state: InteractionState,
    pub cursor: CursorType,
    
    // Content
    pub text: String,
    pub custom_properties: HashMap<String, PropertyValue>,
    
    // Layout
    pub layout_position: LayoutPosition,
    pub layout_size: LayoutSize,
    
    // Input handling
    pub input_state: Option<InputState>,
    pub select_state: Option<SelectState>,
    
    // Events
    pub event_handlers: HashMap<EventType, String>,
}

pub enum InteractionState {
    Normal,
    Hover,
    Active,
    Checked,
    Disabled,
}
```

### Template Variable System

```rust
pub fn set_template_variable(&mut self, name: &str, value: &str) -> anyhow::Result<()> {
    // Update template engine
    self.template_engine.set_variable(name, value);
    
    // Find all bindings for this variable
    let bindings = self.template_engine.get_bindings_for_variable(name);
    
    if !bindings.is_empty() {
        // Update affected elements
        self.template_engine.update_elements(&mut self.elements, &self.krb_file);
        
        // Mark for layout and render update
        self.mark_needs_layout();
        self.mark_needs_render();
        
        tracing::info!("Template variable '{}' updated, affected {} elements", 
            name, bindings.len());
    }
    
    Ok(())
}
```

## Memory Management

### Resource Lifecycle

```rust
// Automatic cleanup through RAII
impl<R: CommandRenderer> Drop for KryonApp<R> {
    fn drop(&mut self) {
        // Script engines cleanup automatically
        // Renderer resources cleaned by backend
        // Layout engine cleanup is automatic
        tracing::info!("KryonApp cleanup completed");
    }
}
```

### Memory Optimization Strategies

1. **Element Pooling**: Reuse element instances
2. **Layout Caching**: Cache computed layouts when possible
3. **Texture Atlasing**: Combine small textures
4. **Script Optimization**: Minimize DOM queries
5. **Incremental Updates**: Only update changed elements

### Performance Monitoring

```rust
impl<R: CommandRenderer> KryonApp<R> {
    fn update_frame_metrics(&mut self, delta_time: Duration) {
        self.frame_count += 1;
        
        // Log FPS occasionally
        if self.frame_count % 60 == 0 {
            let fps = 1.0 / delta_time.as_secs_f32();
            tracing::debug!("FPS: {:.1}, Frame: {}", fps, self.frame_count);
        }
        
        // Memory usage tracking
        if self.frame_count % 300 == 0 { // Every 5 seconds at 60 FPS
            let element_count = self.elements.len();
            let script_memory = self.script_system.memory_usage();
            tracing::info!("Memory: {} elements, {} KB scripts", 
                element_count, script_memory / 1024);
        }
    }
}
```

## Performance Characteristics

### Benchmarks

**Layout Performance**:
- Simple layouts: <1ms for 100 elements
- Complex flexbox: 2-5ms for 1000 elements
- Grid layouts: 3-8ms for 1000 elements

**Rendering Performance**:
- WGPU backend: 60+ FPS with 10k+ elements
- Raylib backend: 60+ FPS with 5k+ elements  
- HTML backend: 30+ FPS with 1k+ elements

**Script Performance**:
- Lua function calls: <10μs average
- DOM property updates: <50μs average
- Template variable updates: <100μs average

### Optimization Guidelines

1. **Layout Optimization**:
```kry
# Good: Use absolute positioning for overlays
Container {
    position: "absolute"
    left: 10px
    top: 10px
}

# Avoid: Deep nesting
Container { Container { Container { ... } } }
```

2. **Script Optimization**:
```lua
-- Good: Cache element references
local button = kryon.getElementById("myButton")
for i = 1, 100 do
    button.text = "Count: " .. i
end

-- Bad: Query every time
for i = 1, 100 do
    kryon.getElementById("myButton").text = "Count: " .. i
end
```

3. **Memory Optimization**:
```rust
// Use object pooling for frequently created/destroyed elements
// Batch DOM updates
// Cache computed styles and layouts
```

This runtime system architecture provides the foundation for Kryon's cross-platform UI capabilities while maintaining high performance and developer productivity.
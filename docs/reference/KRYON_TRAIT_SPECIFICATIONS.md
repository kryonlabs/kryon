# Kryon Trait Specifications

This document provides complete trait specifications for implementing Kryon in Rust or other languages. These specifications define exact method signatures, error handling, and behavioral contracts.

## Table of Contents

- [Core Traits](#core-traits)
- [Layout Engine Trait](#layout-engine-trait)
- [Command Renderer Trait](#command-renderer-trait)
- [Script Bridge Trait](#script-bridge-trait)
- [Template Engine Trait](#template-engine-trait)
- [Event Handler Trait](#event-handler-trait)
- [Resource Manager Trait](#resource-manager-trait)

## Core Traits

### LayoutEngine Trait

```rust
use std::collections::HashMap;

pub trait LayoutEngine: Send + Sync {
    type LayoutResult: Clone + Send + Sync;
    type LayoutError: std::error::Error + Send + Sync;
    
    /// Computes layout for all elements in the tree
    /// 
    /// # Parameters
    /// - `elements`: Map of element IDs to their properties
    /// - `constraints`: Available space and sizing constraints
    /// - `viewport`: Current viewport dimensions
    /// 
    /// # Returns
    /// - `Ok(LayoutResult)`: Computed positions and sizes for all elements
    /// - `Err(LayoutError)`: Layout computation failed
    /// 
    /// # Performance Requirements
    /// - Must complete within 16ms for 60fps
    /// - Should handle up to 10,000 elements efficiently
    /// - Memory usage should not exceed 2x element count
    fn compute_layout(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        constraints: &LayoutConstraints,
        viewport: &Viewport,
    ) -> Result<Self::LayoutResult, Self::LayoutError>;
    
    /// Incremental layout update for changed elements
    /// 
    /// # Parameters
    /// - `changed_elements`: List of elements that changed since last layout
    /// - `previous_layout`: Previous layout result for diffing
    /// 
    /// # Performance Requirements
    /// - Must be faster than full recompute for < 100 changed elements
    /// - Should reuse cached computations where possible
    fn update_layout(
        &mut self,
        changed_elements: &[ElementId],
        previous_layout: &Self::LayoutResult,
    ) -> Result<Self::LayoutResult, Self::LayoutError>;
    
    /// Validates layout constraints and reports conflicts
    /// 
    /// # Returns
    /// - `Vec<LayoutWarning>`: List of constraint conflicts or warnings
    fn validate_constraints(
        &self,
        elements: &HashMap<ElementId, Element>,
    ) -> Vec<LayoutWarning>;
    
    /// Gets the computed position and size for a specific element
    fn get_element_rect(&self, element_id: ElementId) -> Option<Rect>;
    
    /// Clears all cached layout data
    fn clear_cache(&mut self);
}

#[derive(Debug, Clone)]
pub struct LayoutConstraints {
    pub available_width: f32,
    pub available_height: f32,
    pub min_width: f32,
    pub min_height: f32,
    pub max_width: f32,
    pub max_height: f32,
}

#[derive(Debug, Clone)]
pub struct Viewport {
    pub width: f32,
    pub height: f32,
    pub scaleFactor: f32,
}

#[derive(Debug, Clone)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

#[derive(Debug)]
pub enum LayoutWarning {
    CircularDependency(Vec<ElementId>),
    OverconstrainedElement(ElementId),
    InfiniteSize(ElementId),
    PerformanceWarning(String),
}
```

### CommandRenderer Trait

```rust
pub trait CommandRenderer: Send + Sync {
    type RenderError: std::error::Error + Send + Sync;
    type Surface: Send + Sync;
    type Texture: Send + Sync + Clone;
    
    /// Initializes the renderer with the given surface
    /// 
    /// # Parameters
    /// - `surface`: Platform-specific surface (Window, Canvas, etc.)
    /// - `config`: Renderer configuration options
    /// 
    /// # Thread Safety
    /// - Must be callable from any thread
    /// - Should initialize thread-local resources if needed
    fn initialize(
        &mut self,
        surface: Self::Surface,
        config: &RendererConfig,
    ) -> Result<(), Self::RenderError>;
    
    /// Begins a new render frame
    /// 
    /// # Parameters
    /// - `clear_color`: Background color for the frame
    /// 
    /// # Performance Requirements
    /// - Must complete within 1ms
    /// - Should clear previous frame buffers
    fn begin_frame(&mut self, clear_color: Color) -> Result<(), Self::RenderError>;
    
    /// Renders a single render command
    /// 
    /// # Parameters
    /// - `command`: The render command to execute
    /// - `transform`: Current transformation matrix
    /// 
    /// # Performance Requirements
    /// - Should batch similar commands when possible
    /// - Text rendering must support Unicode and complex scripts
    fn render_command(
        &mut self,
        command: &RenderCommand,
        transform: &Transform,
    ) -> Result<(), Self::RenderError>;
    
    /// Completes the current frame and presents to screen
    /// 
    /// # Performance Requirements
    /// - Must complete within 1ms
    /// - Should handle vsync appropriately
    fn end_frame(&mut self) -> Result<(), Self::RenderError>;
    
    /// Loads a texture from raw image data
    /// 
    /// # Parameters
    /// - `data`: Raw image bytes (PNG, JPEG, etc.)
    /// - `format`: Image format specification
    /// 
    /// # Memory Management
    /// - Should cache textures by content hash
    /// - Must handle texture atlas management
    fn load_texture(
        &mut self,
        data: &[u8],
        format: ImageFormat,
    ) -> Result<Self::Texture, Self::RenderError>;
    
    /// Creates a text texture from string and font
    /// 
    /// # Parameters
    /// - `text`: UTF-8 text to render
    /// - `font`: Font specification
    /// - `size`: Font size in logical pixels
    /// - `color`: Text color
    /// 
    /// # Text Rendering Requirements
    /// - Must support Unicode (including emojis)
    /// - Should support text shaping for complex scripts
    /// - Must handle word wrapping and line breaks
    fn create_text_texture(
        &mut self,
        text: &str,
        font: &FontSpec,
        size: f32,
        color: Color,
    ) -> Result<Self::Texture, Self::RenderError>;
    
    /// Gets the current renderer capabilities
    fn get_capabilities(&self) -> RendererCapabilities;
    
    /// Cleans up renderer resources
    fn cleanup(&mut self) -> Result<(), Self::RenderError>;
}

#[derive(Debug, Clone)]
pub struct RendererConfig {
    pub vsync: bool,
    pub max_texture_size: u32,
    pub multisample: u8,
    pub depth_buffer: bool,
}

#[derive(Debug, Clone)]
pub struct RendererCapabilities {
    pub max_texture_size: u32,
    pub supports_msaa: bool,
    pub supports_compute_shaders: bool,
    pub max_vertex_attributes: u32,
}
```

### Script Bridge Trait

```rust
pub trait ScriptBridge: Send + Sync {
    type ScriptError: std::error::Error + Send + Sync;
    type ScriptValue: Clone + Send + Sync;
    
    /// Initializes the script engine
    /// 
    /// # Parameters
    /// - `language`: Script language (Lua, JavaScript, Python, etc.)
    /// - `config`: Engine configuration
    /// 
    /// # Thread Safety
    /// - Must be thread-safe for multiple script contexts
    /// - Should isolate script execution environments
    fn initialize(
        &mut self,
        language: ScriptLanguage,
        config: &ScriptConfig,
    ) -> Result<(), Self::ScriptError>;
    
    /// Executes a script with given context variables
    /// 
    /// # Parameters
    /// - `script`: Script source code
    /// - `context`: Available variables and functions
    /// - `timeout`: Maximum execution time
    /// 
    /// # Security Requirements
    /// - Must sandbox script execution
    /// - Should prevent access to filesystem/network
    /// - Must limit memory usage
    fn execute_script(
        &mut self,
        script: &str,
        context: &ScriptContext,
        timeout: Duration,
    ) -> Result<Self::ScriptValue, Self::ScriptError>;
    
    /// Registers a native function callable from scripts
    /// 
    /// # Parameters
    /// - `name`: Function name in script environment
    /// - `function`: Native function implementation
    /// 
    /// # Type Conversion
    /// - Must handle automatic type conversions
    /// - Should provide clear error messages for type mismatches
    fn register_function<F>(
        &mut self,
        name: &str,
        function: F,
    ) -> Result<(), Self::ScriptError>
    where
        F: Fn(&[Self::ScriptValue]) -> Result<Self::ScriptValue, Self::ScriptError> + Send + Sync + 'static;
    
    /// Converts between script values and native types
    fn to_script_value<T>(&self, value: T) -> Result<Self::ScriptValue, Self::ScriptError>
    where
        T: Into<ScriptValueType>;
    
    fn from_script_value<T>(&self, value: &Self::ScriptValue) -> Result<T, Self::ScriptError>
    where
        T: TryFrom<ScriptValueType>;
    
    /// Gets current memory usage of script engine
    fn get_memory_usage(&self) -> usize;
    
    /// Garbage collects script engine memory
    fn collect_garbage(&mut self);
}

#[derive(Debug, Clone)]
pub enum ScriptLanguage {
    Lua,
    JavaScript,
    Python,
    Wren,
}

#[derive(Debug, Clone)]
pub struct ScriptConfig {
    pub max_memory: usize,
    pub max_execution_time: Duration,
    pub sandbox_enabled: bool,
}

#[derive(Debug, Clone)]
pub enum ScriptValueType {
    Null,
    Bool(bool),
    Number(f64),
    String(String),
    Array(Vec<ScriptValueType>),
    Object(HashMap<String, ScriptValueType>),
}
```

## Error Handling Specifications

All traits must implement comprehensive error handling:

```rust
/// Standard error categories for all Kryon operations
#[derive(Debug, Clone, PartialEq)]
pub enum KryonErrorCategory {
    /// Invalid input parameters
    InvalidInput,
    /// Resource not found (file, texture, font, etc.)
    ResourceNotFound,
    /// Out of memory or other resource exhaustion
    ResourceExhaustion,
    /// Platform-specific error
    PlatformError,
    /// Script execution error
    ScriptError,
    /// Layout computation error
    LayoutError,
    /// Rendering error
    RenderError,
}

/// All Kryon errors must implement this trait
pub trait KryonError: std::error::Error + Send + Sync {
    fn category(&self) -> KryonErrorCategory;
    fn error_code(&self) -> u32;
    fn is_recoverable(&self) -> bool;
}
```

## Performance Specifications

### Memory Usage Requirements

```rust
/// Memory usage guidelines for implementations
pub struct MemoryRequirements {
    /// Maximum memory per element (including layout cache)
    pub max_memory_per_element: usize, // 1KB per element
    
    /// Maximum texture memory usage
    pub max_texture_memory: usize, // 256MB default
    
    /// Maximum script heap size per context
    pub max_script_memory: usize, // 16MB default
}
```

### Timing Requirements

```rust
/// Performance timing requirements
pub struct TimingRequirements {
    /// Maximum layout computation time
    pub max_layout_time: Duration, // 16ms for 60fps
    
    /// Maximum render command processing time
    pub max_render_time: Duration, // 10ms for 60fps
    
    /// Maximum script execution time
    pub max_script_time: Duration, // 100ms default
}
```

## Thread Safety Specifications

### Thread Safety Guarantees

1. **Layout Engine**: Must be `Send + Sync` and support concurrent read access to computed layouts
2. **Renderer**: Must handle commands from multiple threads but render on single thread
3. **Script Bridge**: Must isolate script contexts and support concurrent execution
4. **All Traits**: Must be safe to clone and move between threads

### Async Operation Support

```rust
/// Async variants for potentially blocking operations
#[async_trait]
pub trait AsyncLayoutEngine: LayoutEngine {
    async fn compute_layout_async(
        &mut self,
        elements: &HashMap<ElementId, Element>,
        constraints: &LayoutConstraints,
        viewport: &Viewport,
    ) -> Result<Self::LayoutResult, Self::LayoutError>;
}
```

## Implementation Guidelines

### Required Optimizations

1. **Layout Caching**: Must cache layout results and use dirty tracking
2. **Render Batching**: Must batch similar render commands
3. **Texture Atlasing**: Should pack small textures into atlases
4. **Script Compilation**: Should compile and cache scripts
5. **Memory Pooling**: Should reuse allocated objects where possible

### Testing Requirements

Each trait implementation must provide:

1. **Unit Tests**: Cover all public methods and error conditions
2. **Performance Tests**: Verify timing and memory requirements
3. **Stress Tests**: Handle edge cases and resource exhaustion
4. **Integration Tests**: Work correctly with other trait implementations

## Related Documentation

- [KRYON_RUNTIME_SYSTEM.md](../runtime/KRYON_RUNTIME_SYSTEM.md) - Runtime architecture
- [KRYON_BACKEND_RENDERERS.md](../runtime/KRYON_BACKEND_RENDERERS.md) - Renderer implementations
- [KRYON_LAYOUT_ENGINE.md](../runtime/KRYON_LAYOUT_ENGINE.md) - Layout system details
- [KRYON_SCRIPT_SYSTEM.md](../runtime/KRYON_SCRIPT_SYSTEM.md) - Script integration
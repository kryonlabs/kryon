//! Embed View Provider System
//! 
//! This module implements the unified embed view architecture for Kryon,
//! allowing pluggable viewers for different types of external content.

mod providers;
pub use providers::*;

use glam::Vec2;
use std::collections::HashMap;
use kryon_core::PropertyValue;
use crate::RenderCommand;

/// Input events that can be passed to foreign views
#[derive(Debug, Clone)]
pub enum InputEvent {
    MouseMove { position: Vec2 },
    MouseDown { position: Vec2, button: MouseButton },
    MouseUp { position: Vec2, button: MouseButton },
    KeyDown { key: String },
    KeyUp { key: String },
    Focus,
    Blur,
}

#[derive(Debug, Clone)]
pub enum MouseButton {
    Left,
    Right,
    Middle,
}

/// Graphics frame abstraction for drawing operations
pub trait Frame {
    /// Execute a render command within this frame
    fn execute_command(&mut self, command: RenderCommand);
    
    /// Get the current viewport size
    fn viewport_size(&self) -> Vec2;
}

/// Rectangular bounds for view positioning
#[derive(Debug, Clone, Copy)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

impl Rect {
    pub fn new(x: f32, y: f32, width: f32, height: f32) -> Self {
        Self { x, y, width, height }
    }
    
    pub fn from_position_size(position: Vec2, size: Vec2) -> Self {
        Self::new(position.x, position.y, size.x, size.y)
    }
}

/// Defines the interface for a pluggable viewer
pub trait ViewProvider: Send + Sync {
    /// A unique string to identify this viewer (e.g., "webview", "wasm", "canvas")
    fn type_id(&self) -> &'static str;
    
    /// Creates a new, stateful instance of the view
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance>;
    
    /// Check if this provider can handle the given source URI
    fn can_handle(&self, source: Option<&str>) -> bool {
        source.is_some() // Default: can handle any source
    }
}

/// Represents a single, running instance of a view
pub trait ViewInstance: Send + Sync {
    /// Draw the view's content into the provided graphics frame and bounds
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect);
    
    /// Handle user input events (mouse, keyboard) that occur within the bounds
    fn handle_event(&mut self, event: &InputEvent) -> bool; // Returns true if event was handled
    
    /// Called when the view's configuration changes
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>);
    
    /// Called when the `source` property changes
    fn update_source(&mut self, new_source: Option<&str>);
    
    /// Called when the view is resized
    fn resize(&mut self, new_bounds: Rect);
    
    /// Called when the view is about to be destroyed
    fn destroy(&mut self) {}
}

/// Registry for managing view providers
pub struct ViewProviderRegistry {
    providers: HashMap<String, Box<dyn ViewProvider>>,
}

impl ViewProviderRegistry {
    /// Create a new empty registry
    pub fn new() -> Self {
        Self {
            providers: HashMap::new(),
        }
    }
    
    /// Create a new registry with default providers
    pub fn with_defaults() -> Self {
        let mut registry = Self::new();
        registry.register(Box::new(WasmViewProvider::new()));
        registry.register(Box::new(WebViewProvider::new()));
        registry.register(Box::new(NativeRendererProvider::new()));
        registry.register(Box::new(IFrameProvider::new()));
        registry
    }
    
    /// Register a new view provider
    pub fn register(&mut self, provider: Box<dyn ViewProvider>) {
        let type_id = provider.type_id().to_string();
        self.providers.insert(type_id, provider);
    }
    
    /// Get a provider by type ID
    pub fn get(&self, type_id: &str) -> Option<&dyn ViewProvider> {
        self.providers.get(type_id).map(|p| p.as_ref())
    }
    
    /// Create a view instance from a provider
    pub fn create_view(
        &self,
        view_type: &str,
        source: Option<&str>,
        config: &HashMap<String, PropertyValue>,
    ) -> Option<Box<dyn ViewInstance>> {
        self.get(view_type)
            .map(|provider| provider.create_view(source, config))
    }
    
    /// List all registered view types
    pub fn list_types(&self) -> Vec<&str> {
        self.providers.keys().map(|s| s.as_str()).collect()
    }
}

impl Default for ViewProviderRegistry {
    fn default() -> Self {
        Self::with_defaults()
    }
}

/// Builder pattern for configuring the view registry
pub struct ViewRegistryBuilder {
    registry: ViewProviderRegistry,
}

impl ViewRegistryBuilder {
    pub fn new() -> Self {
        Self {
            registry: ViewProviderRegistry::new(),
        }
    }
    
    /// Add a view provider to the registry
    pub fn with_provider(mut self, provider: Box<dyn ViewProvider>) -> Self {
        self.registry.register(provider);
        self
    }
    
    /// Build the final registry
    pub fn build(self) -> ViewProviderRegistry {
        self.registry
    }
}

impl Default for ViewRegistryBuilder {
    fn default() -> Self {
        Self::new()
    }
}
//! WASM ViewProvider Implementation
//! 
//! Provides a WASM view for executing WebAssembly modules.

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// WASM view provider for WebAssembly modules
pub struct WasmViewProvider;

impl WasmViewProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for WasmViewProvider {
    fn type_id(&self) -> &'static str {
        "wasm"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(WasmViewInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // WASM provider needs a source (the .wasm file)
        source.is_some() && (
            source.unwrap().ends_with(".wasm") || 
            source.unwrap().starts_with("file://") ||
            source.unwrap().starts_with("http://") ||
            source.unwrap().starts_with("https://")
        )
    }
}

/// WASM view instance that loads and executes WebAssembly modules
pub struct WasmViewInstance {
    source: Option<String>,
    bounds: Rect,
    initial_data: HashMap<String, PropertyValue>,
    loaded: bool,
}

impl WasmViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let source = source.map(|s| s.to_string());
        
        Self {
            source,
            bounds: Rect::new(0.0, 0.0, 100.0, 100.0),
            initial_data: config.clone(),
            loaded: false,
        }
    }
}

impl ViewInstance for WasmViewInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        if let Some(ref wasm_source) = self.source {
            // TODO: Load and execute WASM module
            eprintln!("[WASM] WasmView should load WASM module: '{}'", wasm_source);
            
            // For now, draw a placeholder to show WasmView is working
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(bounds.x + 10.0, bounds.y + 10.0),
                size: Vec2::new(bounds.width - 20.0, bounds.height - 20.0),
                color: Vec4::new(0.8, 0.2, 0.4, 0.3), // Light purple
                border_radius: 5.0,
                border_width: 2.0,
                border_color: Vec4::new(0.6, 0.0, 0.2, 1.0), // Darker purple
                transform: None,
                shadow: None,
                z_index: 0,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(bounds.x + bounds.width / 2.0 - 40.0, bounds.y + bounds.height / 2.0),
                text: "WASM Module".to_string(),
                font_size: 16.0,
                color: Vec4::new(1.0, 1.0, 1.0, 1.0), // White text
                alignment: TextAlignment::Center,
                max_width: Some(bounds.width),
                max_height: Some(bounds.height),
                transform: None,
                font_family: None,
                z_index: 1,
            });
        } else {
            // Error state - no WASM source specified
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(bounds.x, bounds.y),
                size: Vec2::new(bounds.width, bounds.height),
                color: Vec4::new(0.2, 0.1, 0.3, 1.0), // Dark purple background
                border_radius: 0.0,
                border_width: 1.0,
                border_color: Vec4::new(0.8, 0.4, 0.6, 1.0), // Pink border
                transform: None,
                shadow: None,
                z_index: 0,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(bounds.x + bounds.width / 2.0 - 60.0, bounds.y + bounds.height / 2.0),
                text: "No WASM Source".to_string(),
                font_size: 14.0,
                color: Vec4::new(0.8, 0.8, 0.8, 1.0), // Light gray text
                alignment: TextAlignment::Center,
                max_width: Some(bounds.width),
                max_height: Some(bounds.height),
                transform: None,
                font_family: None,
                z_index: 1,
            });
        }
    }
    
    fn handle_event(&mut self, event: &InputEvent) -> bool {
        match event {
            InputEvent::MouseMove { position } => {
                // TODO: Pass mouse events to WASM module
                eprintln!("[WASM] Mouse move at ({}, {})", position.x, position.y);
                true
            }
            InputEvent::MouseDown { position, button } => {
                // TODO: Pass click events to WASM module
                eprintln!("[WASM] Mouse down at ({}, {}) with button {:?}", position.x, position.y, button);
                true
            }
            InputEvent::KeyDown { key } => {
                // TODO: Pass key events to WASM module
                eprintln!("[WASM] Key down: {}", key);
                true
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        self.initial_data = config.clone();
        // TODO: Pass new configuration to WASM module
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.source = new_source.map(|s| s.to_string());
        self.loaded = false;
        // TODO: Reload WASM module with new source
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        // TODO: Notify WASM module of size change
    }
}

impl Default for WasmViewProvider {
    fn default() -> Self {
        Self::new()
    }
}
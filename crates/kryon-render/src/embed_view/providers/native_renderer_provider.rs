//! Native Renderer ViewProvider Implementation
//! 
//! Provides native rendering capabilities using system-specific backends.

use super::super::*;
use crate::{RenderCommand, TextAlignment};
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// Native renderer provider for system-specific rendering backends
pub struct NativeRendererProvider;

impl NativeRendererProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for NativeRendererProvider {
    fn type_id(&self) -> &'static str {
        "native_renderer"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(NativeRendererInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // Native renderer can handle script functions or backend specifications
        source.is_some() && (
            source.unwrap().ends_with(".lua") ||
            source.unwrap().ends_with(".js") ||
            source.unwrap().ends_with(".py") ||
            source.unwrap().contains("skia") ||
            source.unwrap().contains("opengl") ||
            source.unwrap().contains("vulkan")
        )
    }
}

/// Native renderer instance that executes native rendering code
pub struct NativeRendererInstance {
    script_source: Option<String>,
    backend: String,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
}

impl NativeRendererInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let script_source = source.map(|s| s.to_string());
        
        let backend = config.get("backend")
            .and_then(|v| if let PropertyValue::String(s) = v { Some(s.clone()) } else { None })
            .unwrap_or_else(|| "skia".to_string());
        
        Self {
            script_source,
            backend,
            bounds: Rect::new(0.0, 0.0, 100.0, 100.0),
            config: config.clone(),
        }
    }
}

impl ViewInstance for NativeRendererInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        if let Some(ref script) = self.script_source {
            // TODO: Execute native rendering script
            eprintln!("[NATIVE] Native renderer should execute script: '{}' with backend: '{}'", script, self.backend);
            
            // For now, draw a placeholder to show Native renderer is working
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(bounds.x + 5.0, bounds.y + 5.0),
                size: Vec2::new(bounds.width - 10.0, bounds.height - 10.0),
                color: Vec4::new(0.9, 0.7, 0.2, 0.4), // Gold/yellow background
                border_radius: 8.0,
                border_width: 2.0,
                border_color: Vec4::new(0.7, 0.5, 0.0, 1.0), // Darker gold border
                transform: None,
                shadow: None,
                z_index: 0,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(bounds.x + bounds.width / 2.0 - 50.0, bounds.y + bounds.height / 2.0 - 10.0),
                text: format!("Native Renderer\n({})", self.backend),
                font_size: 14.0,
                color: Vec4::new(0.2, 0.2, 0.2, 1.0), // Dark text
                alignment: TextAlignment::Center,
                max_width: Some(bounds.width),
                max_height: Some(bounds.height),
                transform: None,
                font_family: None,
                z_index: 1,
            });
        } else {
            // Error state - no script specified
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(bounds.x, bounds.y),
                size: Vec2::new(bounds.width, bounds.height),
                color: Vec4::new(0.4, 0.3, 0.1, 1.0), // Dark brown background
                border_radius: 0.0,
                border_width: 1.0,
                border_color: Vec4::new(0.8, 0.6, 0.2, 1.0), // Brown border
                transform: None,
                shadow: None,
                z_index: 0,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(bounds.x + bounds.width / 2.0 - 60.0, bounds.y + bounds.height / 2.0),
                text: "No Native Script".to_string(),
                font_size: 14.0,
                color: Vec4::new(0.9, 0.9, 0.9, 1.0), // Light text
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
                // TODO: Pass mouse events to native renderer
                eprintln!("[NATIVE] Mouse move at ({}, {})", position.x, position.y);
                true
            }
            InputEvent::MouseDown { position, button } => {
                // TODO: Pass click events to native renderer
                eprintln!("[NATIVE] Mouse down at ({}, {}) with button {:?}", position.x, position.y, button);
                true
            }
            InputEvent::KeyDown { key } => {
                // TODO: Pass key events to native renderer
                eprintln!("[NATIVE] Key down: {}", key);
                true
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        self.backend = config.get("backend")
            .and_then(|v| if let PropertyValue::String(s) = v { Some(s.clone()) } else { None })
            .unwrap_or_else(|| "skia".to_string());
        
        self.config = config.clone();
        // TODO: Apply new configuration to native renderer
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.script_source = new_source.map(|s| s.to_string());
        // TODO: Reload native renderer with new script
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        // TODO: Notify native renderer of size change
    }
}

impl Default for NativeRendererProvider {
    fn default() -> Self {
        Self::new()
    }
}
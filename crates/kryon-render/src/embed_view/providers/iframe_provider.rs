//! IFrame ViewProvider Implementation
//! 
//! Provides iframe functionality for embedding external websites and content.

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// IFrame provider for embedding external websites
pub struct IFrameProvider;

impl IFrameProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for IFrameProvider {
    fn type_id(&self) -> &'static str {
        "iframe"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(IFrameInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // IFrame can handle any URL or about: pages
        source.is_some() && (
            source.unwrap().starts_with("http://") ||
            source.unwrap().starts_with("https://") ||
            source.unwrap().starts_with("about:") ||
            source.unwrap().starts_with("data:")
        )
    }
}

/// IFrame instance that displays external content in an iframe
pub struct IFrameInstance {
    source_url: Option<String>,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
    sandbox_restrictions: Vec<String>,
}

impl IFrameInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let source_url = source.map(|s| s.to_string());
        
        let sandbox_restrictions = config.get("sandbox")
            .and_then(|v| if let PropertyValue::String(s) = v { 
                Some(s.split_whitespace().map(|s| s.to_string()).collect()) 
            } else { None })
            .unwrap_or_else(|| vec![
                "allow-scripts".to_string(),
                "allow-same-origin".to_string(),
            ]);
        
        Self {
            source_url,
            bounds: Rect::new(0.0, 0.0, 100.0, 100.0),
            config: config.clone(),
            sandbox_restrictions,
        }
    }
}

impl ViewInstance for IFrameInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        if let Some(ref url) = self.source_url {
            // TODO: Render iframe content here
            eprintln!("[IFRAME] IFrame should load URL: '{}' with sandbox: {:?}", url, self.sandbox_restrictions);
            
            // For now, draw a placeholder to show IFrame is working
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(bounds.x, bounds.y),
                size: Vec2::new(bounds.width, bounds.height),
                color: Vec4::new(0.95, 0.95, 0.95, 1.0), // Light gray background
                border_radius: 4.0,
                border_width: 2.0,
                border_color: Vec4::new(0.7, 0.7, 0.7, 1.0), // Gray border
                transform: None,
                shadow: None,
                z_index: 0,
            });
            
            // Draw an iframe-like header
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(bounds.x + 1.0, bounds.y + 1.0),
                size: Vec2::new(bounds.width - 2.0, 25.0),
                color: Vec4::new(0.85, 0.85, 0.85, 1.0), // Darker gray header
                border_radius: 0.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
                transform: None,
                shadow: None,
                z_index: 1,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(bounds.x + 8.0, bounds.y + 13.0),
                text: format!("🌐 {}", url),
                font_size: 11.0,
                color: Vec4::new(0.4, 0.4, 0.4, 1.0), // Dark gray text
                alignment: TextAlignment::Start,
                max_width: Some(bounds.width - 16.0),
                max_height: Some(20.0),
                transform: None,
                font_family: Some("monospace".to_string()),
                z_index: 2,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(bounds.x + bounds.width / 2.0 - 40.0, bounds.y + bounds.height / 2.0),
                text: "IFrame Content".to_string(),
                font_size: 16.0,
                color: Vec4::new(0.3, 0.3, 0.3, 1.0), // Dark text
                alignment: TextAlignment::Center,
                max_width: Some(bounds.width),
                max_height: Some(bounds.height),
                transform: None,
                font_family: None,
                z_index: 1,
            });
        } else {
            // Error state - no URL specified
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(bounds.x, bounds.y),
                size: Vec2::new(bounds.width, bounds.height),
                color: Vec4::new(0.4, 0.4, 0.4, 1.0), // Dark gray background
                border_radius: 0.0,
                border_width: 1.0,
                border_color: Vec4::new(0.7, 0.7, 0.7, 1.0), // Gray border
                transform: None,
                shadow: None,
                z_index: 0,
            });
            
            frame.execute_command(RenderCommand::DrawText {
                position: Vec2::new(bounds.x + bounds.width / 2.0 - 50.0, bounds.y + bounds.height / 2.0),
                text: "No URL Specified".to_string(),
                font_size: 14.0,
                color: Vec4::new(0.9, 0.9, 0.9, 1.0), // Light gray text
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
                // TODO: Pass mouse events to iframe
                eprintln!("[IFRAME] Mouse move at ({}, {})", position.x, position.y);
                true
            }
            InputEvent::MouseDown { position, button } => {
                // TODO: Pass click events to iframe
                eprintln!("[IFRAME] Mouse down at ({}, {}) with button {:?}", position.x, position.y, button);
                true
            }
            InputEvent::KeyDown { key } => {
                // TODO: Pass key events to iframe
                eprintln!("[IFRAME] Key down: {}", key);
                true
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        self.sandbox_restrictions = config.get("sandbox")
            .and_then(|v| if let PropertyValue::String(s) = v { 
                Some(s.split_whitespace().map(|s| s.to_string()).collect()) 
            } else { None })
            .unwrap_or_else(|| vec![
                "allow-scripts".to_string(),
                "allow-same-origin".to_string(),
            ]);
        
        self.config = config.clone();
        // TODO: Apply new configuration to iframe
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.source_url = new_source.map(|s| s.to_string());
        // TODO: Navigate iframe to new URL
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        // TODO: Resize iframe
    }
}

impl Default for IFrameProvider {
    fn default() -> Self {
        Self::new()
    }
}
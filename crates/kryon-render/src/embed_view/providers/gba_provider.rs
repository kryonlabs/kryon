//! Game Boy Advance ViewProvider Implementation
//! 
//! Provides a GBA emulator view for running Game Boy Advance ROM files.

use super::super::*;
use crate::{RenderCommand, TextAlignment};
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// GBA emulator provider for Game Boy Advance ROMs
pub struct GbaViewProvider;

impl GbaViewProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for GbaViewProvider {
    fn type_id(&self) -> &'static str {
        "gba"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(GbaViewInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // GBA provider handles .gba ROM files
        source.is_some() && (
            source.unwrap().ends_with(".gba") || 
            source.unwrap().ends_with(".bin") ||
            source.unwrap().ends_with(".rom")
        )
    }
}

/// GBA emulator instance that runs Game Boy Advance games
pub struct GbaViewInstance {
    rom_path: Option<String>,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
    emulator_state: EmulatorState,
    scale_factor: f32,
}

#[derive(Debug, Clone)]
enum EmulatorState {
    Loading,
    Running,
    Paused,
    Error(String),
}

impl GbaViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let rom_path = source.map(|s| s.to_string());
        
        let scale_factor = config.get("scale_factor")
            .and_then(|v| v.as_float())
            .unwrap_or(2.0) as f32;
        
        Self {
            rom_path,
            bounds: Rect::new(0.0, 0.0, 240.0, 160.0), // GBA native resolution
            config: config.clone(),
            emulator_state: if rom_path.is_some() { EmulatorState::Loading } else { EmulatorState::Error("No ROM specified".to_string()) },
            scale_factor,
        }
    }
}

impl ViewInstance for GbaViewInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        // Draw emulator screen background
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(bounds.x, bounds.y),
            size: Vec2::new(bounds.width, bounds.height),
            color: Vec4::new(0.1, 0.1, 0.1, 1.0), // Dark background
            border_radius: 4.0,
            border_width: 2.0,
            border_color: Vec4::new(0.3, 0.3, 0.3, 1.0), // Gray border
            transform: None,
            shadow: None,
            z_index: 0,
        });
        
        match &self.emulator_state {
            EmulatorState::Loading => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 30.0, bounds.y + bounds.height / 2.0),
                    text: "Loading ROM...".to_string(),
                    font_size: 14.0,
                    color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: None,
                    z_index: 1,
                });
            }
            EmulatorState::Running => {
                // Draw a mock GBA screen with pixel art style
                let screen_width = 240.0 * self.scale_factor;
                let screen_height = 160.0 * self.scale_factor;
                let screen_x = bounds.x + (bounds.width - screen_width) / 2.0;
                let screen_y = bounds.y + (bounds.height - screen_height) / 2.0;
                
                // Mock game screen (colorful blocks representing a game)
                frame.execute_command(RenderCommand::DrawRect {
                    position: Vec2::new(screen_x, screen_y),
                    size: Vec2::new(screen_width, screen_height),
                    color: Vec4::new(0.2, 0.4, 0.8, 1.0), // Sky blue background
                    border_radius: 2.0,
                    border_width: 1.0,
                    border_color: Vec4::new(0.5, 0.5, 0.5, 1.0),
                    transform: None,
                    shadow: None,
                    z_index: 1,
                });
                
                // Draw some mock game elements
                let block_size = 16.0 * self.scale_factor;
                for i in 0..5 {
                    for j in 0..3 {
                        let x = screen_x + i as f32 * block_size * 2.0;
                        let y = screen_y + j as f32 * block_size * 2.0;
                        let color = Vec4::new(
                            (i as f32 * 0.2) % 1.0,
                            (j as f32 * 0.3) % 1.0,
                            0.6,
                            1.0
                        );
                        
                        frame.execute_command(RenderCommand::DrawRect {
                            position: Vec2::new(x, y),
                            size: Vec2::new(block_size, block_size),
                            color,
                            border_radius: 2.0,
                            border_width: 0.0,
                            border_color: Vec4::ZERO,
                            transform: None,
                            shadow: None,
                            z_index: 2,
                        });
                    }
                }
                
                // Game title
                if let Some(ref rom_path) = self.rom_path {
                    let filename = rom_path.split('/').last().unwrap_or("Game");
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(bounds.x + 5.0, bounds.y + bounds.height - 20.0),
                        text: format!("🎮 {}", filename),
                        font_size: 10.0,
                        color: Vec4::new(0.9, 0.9, 0.9, 1.0),
                        alignment: TextAlignment::Start,
                        max_width: Some(bounds.width - 10.0),
                        max_height: Some(15.0),
                        transform: None,
                        font_family: Some("monospace".to_string()),
                        z_index: 3,
                    });
                }
            }
            EmulatorState::Paused => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 25.0, bounds.y + bounds.height / 2.0),
                    text: "⏸️ PAUSED".to_string(),
                    font_size: 16.0,
                    color: Vec4::new(1.0, 1.0, 0.0, 1.0), // Yellow
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: None,
                    z_index: 1,
                });
            }
            EmulatorState::Error(msg) => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 50.0, bounds.y + bounds.height / 2.0),
                    text: format!("❌ Error: {}", msg),
                    font_size: 12.0,
                    color: Vec4::new(1.0, 0.2, 0.2, 1.0), // Red
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: None,
                    z_index: 1,
                });
            }
        }
    }
    
    fn handle_event(&mut self, event: &InputEvent) -> bool {
        match event {
            InputEvent::MouseDown { position, button } => {
                // Toggle pause on click
                match self.emulator_state {
                    EmulatorState::Running => {
                        self.emulator_state = EmulatorState::Paused;
                        eprintln!("[GBA] Game paused");
                    }
                    EmulatorState::Paused => {
                        self.emulator_state = EmulatorState::Running;
                        eprintln!("[GBA] Game resumed");
                    }
                    _ => {}
                }
                true
            }
            InputEvent::KeyDown { key } => {
                // Handle GBA controls
                match key.as_str() {
                    "a" | "z" => eprintln!("[GBA] A button pressed"),
                    "s" | "x" => eprintln!("[GBA] B button pressed"),
                    "Return" => eprintln!("[GBA] Start button pressed"),
                    "space" => eprintln!("[GBA] Select button pressed"),
                    "Up" | "Down" | "Left" | "Right" => eprintln!("[GBA] D-pad {} pressed", key),
                    _ => return false,
                }
                true
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        self.scale_factor = config.get("scale_factor")
            .and_then(|v| v.as_float())
            .unwrap_or(2.0) as f32;
        
        self.config = config.clone();
        // TODO: Apply emulator configuration changes
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.rom_path = new_source.map(|s| s.to_string());
        self.emulator_state = if self.rom_path.is_some() { 
            EmulatorState::Loading 
        } else { 
            EmulatorState::Error("No ROM specified".to_string()) 
        };
        // TODO: Load new ROM file
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        // TODO: Adjust emulator screen scaling
    }
    
    fn destroy(&mut self) {
        // TODO: Clean up emulator resources
        eprintln!("[GBA] Emulator destroyed");
    }
}

impl Default for GbaViewProvider {
    fn default() -> Self {
        Self::new()
    }
}
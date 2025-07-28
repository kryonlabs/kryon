//! UXN Virtual Machine ViewProvider Implementation
//! 
//! Provides a UXN emulator view for running UXN ROMs (Varvara computer).

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// UXN emulator provider for Varvara/UXN ROM files
pub struct UxnViewProvider;

impl UxnViewProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for UxnViewProvider {
    fn type_id(&self) -> &'static str {
        "uxn"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(UxnViewInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // UXN provider handles .rom files and some other UXN formats
        source.is_some() && (
            source.unwrap().ends_with(".rom") || 
            source.unwrap().ends_with(".uxn") ||
            source.unwrap().ends_with(".tal") // Tal assembly files
        )
    }
}

/// UXN emulator instance that runs Varvara computer programs
pub struct UxnViewInstance {
    rom_path: Option<String>,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
    emulator_state: EmulatorState,
    scale_factor: f32,
    screen_width: u32,
    screen_height: u32,
    // Mock screen buffer for demonstration
    screen_buffer: Vec<u8>,
}

#[derive(Debug, Clone)]
enum EmulatorState {
    Loading,
    Running,
    Paused,
    Error(String),
}

impl UxnViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let rom_path = source.map(|s| s.to_string());
        
        let scale_factor = config.get("scale_factor")
            .and_then(|v| v.as_float())
            .unwrap_or(2.0) as f32;
            
        // UXN screen can be configured, but typical sizes are 64x40 or 320x240
        let screen_width = config.get("screen_width")
            .and_then(|v| v.as_int())
            .unwrap_or(320) as u32;
            
        let screen_height = config.get("screen_height")
            .and_then(|v| v.as_int())
            .unwrap_or(240) as u32;
        
        // Initialize mock screen buffer
        let buffer_size = (screen_width * screen_height * 4) as usize; // RGBA
        let screen_buffer = vec![0u8; buffer_size];
        
        Self {
            rom_path: rom_path.clone(),
            bounds: Rect::new(0.0, 0.0, screen_width as f32, screen_height as f32),
            config: config.clone(),
            emulator_state: if rom_path.is_some() { EmulatorState::Loading } else { EmulatorState::Error("No ROM specified".to_string()) },
            scale_factor,
            screen_width,
            screen_height,
            screen_buffer,
        }
    }
    
    fn simulate_uxn_screen(&mut self) {
        // Mock UXN screen with procedural pattern
        // In a real implementation, this would be the UXN virtual machine output
        let time = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis() as f32 / 1000.0;
            
        for y in 0..self.screen_height {
            for x in 0..self.screen_width {
                let idx = ((y * self.screen_width + x) * 4) as usize;
                
                // Create a UXN-style dithered pattern
                let wave_x = (x as f32 / 32.0 + time * 0.1).sin();
                let wave_y = (y as f32 / 24.0 + time * 0.15).cos();
                let pattern = (wave_x + wave_y) * 0.5 + 0.5;
                
                // Use UXN-like 4-color palette
                let color_index = (pattern * 3.0) as u8;
                let (r, g, b) = match color_index {
                    0 => (0x00, 0x00, 0x00), // Black
                    1 => (0x55, 0x55, 0x55), // Dark gray
                    2 => (0xAA, 0xAA, 0xAA), // Light gray
                    _ => (0xFF, 0xFF, 0xFF), // White
                };
                
                if idx + 3 < self.screen_buffer.len() {
                    self.screen_buffer[idx] = r;
                    self.screen_buffer[idx + 1] = g;
                    self.screen_buffer[idx + 2] = b;
                    self.screen_buffer[idx + 3] = 255; // Alpha
                }
            }
        }
    }
}

impl ViewInstance for UxnViewInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        // Draw emulator housing background
        frame.execute_command(RenderCommand::DrawRect {
            layout_style: None,
            position: Vec2::new(bounds.x, bounds.y),
            size: Vec2::new(bounds.width, bounds.height),
            color: Vec4::new(0.12, 0.12, 0.12, 1.0), // Dark Varvara-style background
            border_radius: 6.0,
            border_width: 2.0,
            border_color: Vec4::new(0.2, 0.2, 0.3, 1.0), // Subtle blue border
            transform: None,
            shadow: None,
            z_index: 0,
        });
        
        match &self.emulator_state {
            EmulatorState::Loading => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 40.0, bounds.y + bounds.height / 2.0),
                    text: "Loading UXN ROM...".to_string(),
                    font_size: 14.0,
                    color: Vec4::new(0.8, 0.8, 0.9, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 1,
                });
            }
            EmulatorState::Running => {
                // Update the mock screen
                self.simulate_uxn_screen();
                
                // Calculate screen positioning
                let screen_width = self.screen_width as f32 * self.scale_factor;
                let screen_height = self.screen_height as f32 * self.scale_factor;
                let screen_x = bounds.x + (bounds.width - screen_width) / 2.0;
                let screen_y = bounds.y + (bounds.height - screen_height) / 2.0;
                
                // Draw UXN screen background
                frame.execute_command(RenderCommand::DrawRect {
            layout_style: None,
                    position: Vec2::new(screen_x, screen_y),
                    size: Vec2::new(screen_width, screen_height),
                    color: Vec4::new(0.05, 0.05, 0.05, 1.0), // Very dark screen
                    border_radius: 2.0,
                    border_width: 1.0,
                    border_color: Vec4::new(0.3, 0.3, 0.4, 1.0),
                    transform: None,
                    shadow: None,
                    z_index: 1,
                });
                
                // Draw mock UXN graphics (pixelated pattern)
                let pixel_size = self.scale_factor.max(1.0);
                let pixels_x = (screen_width / pixel_size) as u32;
                let pixels_y = (screen_height / pixel_size) as u32;
                
                for py in 0..pixels_y.min(self.screen_height) {
                    for px in 0..pixels_x.min(self.screen_width) {
                        let buffer_idx = ((py * self.screen_width + px) * 4) as usize;
                        if buffer_idx + 3 < self.screen_buffer.len() {
                            let r = self.screen_buffer[buffer_idx] as f32 / 255.0;
                            let g = self.screen_buffer[buffer_idx + 1] as f32 / 255.0;
                            let b = self.screen_buffer[buffer_idx + 2] as f32 / 255.0;
                            
                            let pixel_x = screen_x + px as f32 * pixel_size;
                            let pixel_y = screen_y + py as f32 * pixel_size;
                            
                            frame.execute_command(RenderCommand::DrawRect {
            layout_style: None,
                                position: Vec2::new(pixel_x, pixel_y),
                                size: Vec2::new(pixel_size, pixel_size),
                                color: Vec4::new(r, g, b, 1.0),
                                border_radius: 0.0,
                                border_width: 0.0,
                                border_color: Vec4::ZERO,
                                transform: None,
                                shadow: None,
                                z_index: 2,
                            });
                        }
                    }
                }
                
                // ROM title and info
                if let Some(ref rom_path) = self.rom_path {
                    let filename = rom_path.split('/').last().unwrap_or("Program");
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(bounds.x + 5.0, bounds.y + bounds.height - 20.0),
                        text: format!("🖥️ UXN: {}", filename),
                        font_size: 10.0,
                        color: Vec4::new(0.7, 0.8, 0.9, 1.0),
                        alignment: TextAlignment::Start,
                        max_width: Some(bounds.width - 10.0),
                        max_height: Some(15.0),
                        transform: None,
                        font_family: Some("monospace".to_string()),
                        z_index: 3,
                    });
                    
                    // Show resolution
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(bounds.x + bounds.width - 80.0, bounds.y + bounds.height - 20.0),
                        text: format!("{}×{}", self.screen_width, self.screen_height),
                        font_size: 9.0,
                        color: Vec4::new(0.5, 0.6, 0.7, 1.0),
                        alignment: TextAlignment::End,
                        max_width: Some(75.0),
                        max_height: Some(15.0),
                        transform: None,
                        font_family: Some("monospace".to_string()),
                        z_index: 3,
                    });
                }
            }
            EmulatorState::Paused => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 30.0, bounds.y + bounds.height / 2.0),
                    text: "⏸️ UXN PAUSED".to_string(),
                    font_size: 16.0,
                    color: Vec4::new(1.0, 0.8, 0.0, 1.0), // Amber
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 1,
                });
            }
            EmulatorState::Error(msg) => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 60.0, bounds.y + bounds.height / 2.0),
                    text: format!("❌ UXN Error: {}", msg),
                    font_size: 12.0,
                    color: Vec4::new(1.0, 0.3, 0.3, 1.0), // Red
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 1,
                });
            }
        }
        
        // Always simulate loading after a brief delay
        if matches!(self.emulator_state, EmulatorState::Loading) {
            // In a real implementation, this would load the ROM
            self.emulator_state = EmulatorState::Running;
        }
    }
    
    fn handle_event(&mut self, event: &InputEvent) -> bool {
        match event {
            InputEvent::MouseDown { position: _, button: _ } => {
                // Toggle pause on click
                match self.emulator_state {
                    EmulatorState::Running => {
                        self.emulator_state = EmulatorState::Paused;
                        eprintln!("[UXN] Program paused");
                    }
                    EmulatorState::Paused => {
                        self.emulator_state = EmulatorState::Running;
                        eprintln!("[UXN] Program resumed");
                    }
                    _ => {}
                }
                true
            }
            InputEvent::KeyDown { key } => {
                // Handle UXN/Varvara controls
                match key.as_str() {
                    "a" | "z" => eprintln!("[UXN] Button A pressed"),
                    "s" | "x" => eprintln!("[UXN] Button B pressed"),
                    "Return" => eprintln!("[UXN] Start pressed"),
                    "space" => eprintln!("[UXN] Select pressed"),
                    "Up" | "Down" | "Left" | "Right" => eprintln!("[UXN] D-pad {} pressed", key),
                    "r" => {
                        // Reset/reload ROM
                        self.emulator_state = EmulatorState::Loading;
                        eprintln!("[UXN] ROM reset");
                    }
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
        
        // Update screen resolution if specified
        let new_width = config.get("screen_width")
            .and_then(|v| v.as_int())
            .unwrap_or(self.screen_width as i32) as u32;
            
        let new_height = config.get("screen_height")
            .and_then(|v| v.as_int())
            .unwrap_or(self.screen_height as i32) as u32;
        
        if new_width != self.screen_width || new_height != self.screen_height {
            self.screen_width = new_width;
            self.screen_height = new_height;
            
            // Recreate screen buffer
            let buffer_size = (self.screen_width * self.screen_height * 4) as usize;
            self.screen_buffer = vec![0u8; buffer_size];
            
            eprintln!("[UXN] Screen resolution changed to {}×{}", self.screen_width, self.screen_height);
        }
        
        self.config = config.clone();
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.rom_path = new_source.map(|s| s.to_string());
        self.emulator_state = if self.rom_path.is_some() { 
            EmulatorState::Loading 
        } else { 
            EmulatorState::Error("No ROM specified".to_string()) 
        };
        
        if let Some(path) = &self.rom_path {
            eprintln!("[UXN] Loading ROM: {}", path);
        }
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        // UXN screen adapts to container size through scale_factor
    }
    
    fn destroy(&mut self) {
        eprintln!("[UXN] Virtual machine destroyed");
        // In a real implementation: clean up UXN VM, close ROM file, etc.
    }
}

impl Default for UxnViewProvider {
    fn default() -> Self {
        Self::new()
    }
}
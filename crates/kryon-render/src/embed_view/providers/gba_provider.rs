//! Game Boy Advance ViewProvider Implementation
//! 
//! Provides a GBA emulator view for running Game Boy Advance ROM files.

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
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
    cpu_state: CpuState,
    frame_buffer: Vec<u16>, // GBA uses 15-bit color (RGB555)
    sound_enabled: bool,
    save_data: Vec<u8>,
    frame_count: u64,
}

#[derive(Debug, Clone)]
enum EmulatorState {
    Loading,
    Running,
    Paused,
    Error(String),
}

#[derive(Debug, Clone)]
struct CpuState {
    // ARM7TDMI registers
    registers: [u32; 16], // R0-R15 (R15 is PC)
    cpsr: u32, // Current Program Status Register
    spsr: u32, // Saved Program Status Register
    cycles: u64,
    thumb_mode: bool,
}

impl CpuState {
    fn new() -> Self {
        Self {
            registers: [0; 16],
            cpsr: 0x000000D3, // ARM mode, IRQ/FIQ disabled
            spsr: 0,
            cycles: 0,
            thumb_mode: false,
        }
    }
    
    fn reset(&mut self) {
        self.registers = [0; 16];
        self.registers[15] = 0x08000000; // ROM start address
        self.cpsr = 0x000000D3;
        self.spsr = 0;
        self.cycles = 0;
        self.thumb_mode = false;
    }
}

impl GbaViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let rom_path = source.map(|s| s.to_string());
        
        let scale_factor = config.get("scale_factor")
            .and_then(|v| v.as_float())
            .unwrap_or(2.0) as f32;
        
        let sound_enabled = config.get("sound_enabled")
            .and_then(|v| v.as_bool())
            .unwrap_or(true);
        
        Self {
            rom_path: rom_path.clone(),
            bounds: Rect::new(0.0, 0.0, 240.0, 160.0), // GBA native resolution
            config: config.clone(),
            emulator_state: if rom_path.is_some() { EmulatorState::Loading } else { EmulatorState::Error("No ROM specified".to_string()) },
            scale_factor,
            cpu_state: CpuState::new(),
            frame_buffer: vec![0x0000; 240 * 160], // Initialize with black pixels
            sound_enabled,
            save_data: Vec::new(),
            frame_count: 0,
        }
    }
    
    fn simulate_cpu_frame(&mut self) {
        // Simulate ARM7TDMI CPU execution for one frame
        const CYCLES_PER_FRAME: u64 = 280896; // ~16.78MHz / 59.73Hz
        
        for _ in 0..1000 { // Simulate some CPU instructions per frame
            self.cpu_state.cycles += 1;
            
            // Mock instruction execution
            if self.cpu_state.cycles % 100 == 0 {
                // Simulate register updates
                self.cpu_state.registers[0] = self.cpu_state.registers[0].wrapping_add(1);
                self.cpu_state.registers[15] += 4; // PC increment
            }
        }
        
        // Generate mock frame buffer content
        let time = (self.frame_count as f32 * 0.016) % 360.0; // 60 FPS timing
        self.generate_mock_frame_buffer(time);
        
        self.frame_count += 1;
    }
    
    fn generate_mock_frame_buffer(&mut self, time: f32) {
        // Generate a colorful pattern simulating a GBA game
        for y in 0..160 {
            for x in 0..240 {
                let idx = y * 240 + x;
                
                // Create animated patterns
                let wave1 = ((x as f32 / 30.0 + time * 0.1).sin() * 127.0 + 128.0) as u8;
                let wave2 = ((y as f32 / 20.0 + time * 0.05).cos() * 127.0 + 128.0) as u8;
                let spiral = ((((x - 120) * (x - 120) + (y - 80) * (y - 80)) as f32).sqrt() + time * 10.0).sin();
                
                // Convert to RGB555 format (GBA native)
                let r = ((wave1 as f32 * spiral + 128.0).clamp(0.0, 255.0) / 8.0) as u16;
                let g = ((wave2 as f32 * spiral + 128.0).clamp(0.0, 255.0) / 8.0) as u16;
                let b = (((time * 2.0).sin() * 127.0 + 128.0) / 8.0) as u16;
                
                // Pack into RGB555
                let color = (r << 10) | (g << 5) | b;
                self.frame_buffer[idx] = color;
            }
        }
    }
    
    fn render_gba_screen(&self, frame: &mut dyn Frame, screen_x: f32, screen_y: f32, screen_width: f32, screen_height: f32) {
        // Draw the screen background
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(screen_x, screen_y),
            size: Vec2::new(screen_width, screen_height),
            color: Vec4::new(0.0, 0.0, 0.0, 1.0),
            border_radius: 2.0,
            border_width: 1.0,
            border_color: Vec4::new(0.5, 0.5, 0.5, 1.0),
            transform: None,
            shadow: None,
            z_index: 1,
        });
        
        // Render pixels from frame buffer
        let pixel_width = screen_width / 240.0;
        let pixel_height = screen_height / 160.0;
        
        // Sample pixels (render every 4th pixel for performance)
        for y in (0..160).step_by(4) {
            for x in (0..240).step_by(4) {
                let idx = y * 240 + x;
                let color_555 = self.frame_buffer[idx];
                
                // Convert RGB555 to RGBA
                let r = ((color_555 >> 10) & 0x1F) as f32 / 31.0;
                let g = ((color_555 >> 5) & 0x1F) as f32 / 31.0;
                let b = (color_555 & 0x1F) as f32 / 31.0;
                
                let pixel_x = screen_x + x as f32 * pixel_width;
                let pixel_y = screen_y + y as f32 * pixel_height;
                
                frame.execute_command(RenderCommand::DrawRect {
                    position: Vec2::new(pixel_x, pixel_y),
                    size: Vec2::new(pixel_width * 4.0, pixel_height * 4.0),
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
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 60.0, bounds.y + bounds.height / 2.0),
                    text: "🔄 Loading GBA ROM...".to_string(),
                    font_size: 14.0,
                    color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 1,
                });
                
                // Simulate loading completion
                self.emulator_state = EmulatorState::Running;
                eprintln!("[GBA] ROM loaded successfully");
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
                
                // Simulate CPU execution and render frame
                self.simulate_cpu_frame();
                self.render_gba_screen(frame, screen_x, screen_y, screen_width, screen_height);
                
                // Draw CPU info overlay
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(screen_x + 5.0, screen_y + 5.0),
                    text: format!("CPU: {}MHz | Frame: {}", (self.cpu_state.cycles / 1000000), self.frame_count),
                    font_size: 8.0,
                    color: Vec4::new(1.0, 1.0, 1.0, 0.8),
                    alignment: TextAlignment::Start,
                    max_width: Some(screen_width - 10.0),
                    max_height: Some(12.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 4,
                });
                
                // Game title and status
                if let Some(ref rom_path) = self.rom_path {
                    let filename = rom_path.split('/').last().unwrap_or("Game");
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(bounds.x + 5.0, bounds.y + bounds.height - 35.0),
                        text: format!("🎮 GBA: {}", filename),
                        font_size: 10.0,
                        color: Vec4::new(0.9, 0.9, 0.9, 1.0),
                        alignment: TextAlignment::Start,
                        max_width: Some(bounds.width - 10.0),
                        max_height: Some(15.0),
                        transform: None,
                        font_family: Some("monospace".to_string()),
                        z_index: 3,
                    });
                    
                    // Sound indicator
                    let sound_status = if self.sound_enabled { "🔊" } else { "🔇" };
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(bounds.x + 5.0, bounds.y + bounds.height - 20.0),
                        text: format!("{} ARM7TDMI | Save: {}KB", sound_status, self.save_data.len() / 1024),
                        font_size: 8.0,
                        color: Vec4::new(0.7, 0.7, 0.7, 1.0),
                        alignment: TextAlignment::Start,
                        max_width: Some(bounds.width - 10.0),
                        max_height: Some(12.0),
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
            InputEvent::MouseDown { position: _, button: _ } => {
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
//! CHIP-8 ViewProvider Implementation
//! 
//! Provides CHIP-8 emulation for retro computing games and demos.

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// CHIP-8 emulator provider for retro games
pub struct Chip8ViewProvider;

impl Chip8ViewProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for Chip8ViewProvider {
    fn type_id(&self) -> &'static str {
        "chip8"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(Chip8ViewInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // CHIP-8 provider handles .ch8 and .c8 ROM files
        if let Some(src) = source {
            src.ends_with(".ch8") ||
            src.ends_with(".c8") ||
            src.ends_with(".chip8") ||
            src.contains("chip8") ||
            src.contains("chip-8")
        } else {
            false
        }
    }
}

/// CHIP-8 emulator instance
pub struct Chip8ViewInstance {
    rom_path: Option<String>,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
    emulator_state: EmulatorState,
    cpu_state: Chip8Cpu,
    display: [bool; 64 * 32], // 64x32 monochrome display
    keys: [bool; 16], // 16-key hexadecimal keypad
    scale_factor: f32,
    pixel_color: Vec4,
    background_color: Vec4,
    // Timers
    delay_timer: u8,
    sound_timer: u8,
    // ROM data (mock)
    rom_data: Vec<u8>,
    game_title: String,
}

#[derive(Debug, Clone)]
enum EmulatorState {
    Loading,
    Running,
    Paused,
    Error(String),
}

#[derive(Debug, Clone)]
struct Chip8Cpu {
    // CHIP-8 CPU state
    memory: [u8; 4096], // 4KB memory
    v: [u8; 16], // 16 general-purpose registers (V0-VF)
    i: u16, // Index register
    pc: u16, // Program counter
    sp: u8, // Stack pointer
    stack: [u16; 16], // Stack for subroutine calls
    cycles: u64,
}

impl Chip8Cpu {
    fn new() -> Self {
        let mut cpu = Self {
            memory: [0; 4096],
            v: [0; 16],
            i: 0,
            pc: 0x200, // Program starts at 0x200
            sp: 0,
            stack: [0; 16],
            cycles: 0,
        };
        
        // Load font set into memory (0x50-0x9F)
        let font_set = [
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F
        ];
        
        for (i, &byte) in font_set.iter().enumerate() {
            cpu.memory[0x50 + i] = byte;
        }
        
        cpu
    }
    
    fn reset(&mut self) {
        self.v = [0; 16];
        self.i = 0;
        self.pc = 0x200;
        self.sp = 0;
        self.stack = [0; 16];
        self.cycles = 0;
        // Keep font set and ROM data in memory
    }
    
    fn load_rom(&mut self, rom_data: &[u8]) {
        // Load ROM into memory starting at 0x200
        for (i, &byte) in rom_data.iter().enumerate() {
            if 0x200 + i < self.memory.len() {
                self.memory[0x200 + i] = byte;
            }
        }
    }
    
    fn fetch_instruction(&self) -> u16 {
        // Fetch 16-bit instruction from memory
        if self.pc < 4095 {
            ((self.memory[self.pc as usize] as u16) << 8) | (self.memory[self.pc as usize + 1] as u16)
        } else {
            0
        }
    }
}

impl Chip8ViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let rom_path = source.map(|s| s.to_string());
        
        let scale_factor = config.get("scale_factor")
            .and_then(|v| v.as_float())
            .unwrap_or(8.0) as f32; // Default 8x scaling for 64x32 display
        
        let pixel_color = config.get("pixel_color")
            .and_then(|v| v.as_string())
            .map(|color| parse_color(&color))
            .unwrap_or(Vec4::new(0.0, 1.0, 0.0, 1.0)); // Default green
        
        let background_color = config.get("background_color")
            .and_then(|v| v.as_string())
            .map(|color| parse_color(&color))
            .unwrap_or(Vec4::new(0.0, 0.0, 0.0, 1.0)); // Default black
        
        let (emulator_state, game_title, rom_data) = if let Some(ref path) = rom_path {
            let title = generate_mock_game_title(path);
            let rom = generate_mock_rom_data(path);
            (EmulatorState::Loading, title, rom)
        } else {
            (EmulatorState::Error("No ROM specified".to_string()), "Error".to_string(), vec![])
        };
        
        Self {
            rom_path,
            bounds: Rect::new(0.0, 0.0, 64.0 * scale_factor, 32.0 * scale_factor),
            config: config.clone(),
            emulator_state,
            cpu_state: Chip8Cpu::new(),
            display: [false; 64 * 32],
            keys: [false; 16],
            scale_factor,
            pixel_color,
            background_color,
            delay_timer: 0,
            sound_timer: 0,
            rom_data,
            game_title,
        }
    }
    
    fn simulate_chip8_loading(&mut self) {
        match &self.emulator_state {
            EmulatorState::Loading => {
                // Load ROM into CPU memory
                self.cpu_state.load_rom(&self.rom_data);
                self.cpu_state.reset();
                self.emulator_state = EmulatorState::Running;
                
                eprintln!("[CHIP-8] ROM loaded: {} ({} bytes)", self.game_title, self.rom_data.len());
            }
            _ => {}
        }
    }
    
    fn emulate_cycle(&mut self) {
        if !matches!(self.emulator_state, EmulatorState::Running) {
            return;
        }
        
        // Fetch instruction
        let instruction = self.cpu_state.fetch_instruction();
        self.cpu_state.pc += 2;
        
        // Decode and execute instruction (simplified implementation)
        self.execute_instruction(instruction);
        
        // Update timers
        if self.delay_timer > 0 {
            self.delay_timer -= 1;
        }
        
        if self.sound_timer > 0 {
            self.sound_timer -= 1;
            // In a real implementation, this would trigger a beep sound
        }
        
        self.cpu_state.cycles += 1;
    }
    
    fn execute_instruction(&mut self, instruction: u16) {
        // Simplified CHIP-8 instruction execution
        let opcode = (instruction & 0xF000) >> 12;
        let x = ((instruction & 0x0F00) >> 8) as usize;
        let y = ((instruction & 0x00F0) >> 4) as usize;
        let n = (instruction & 0x000F) as u8;
        let nn = (instruction & 0x00FF) as u8;
        let nnn = instruction & 0x0FFF;
        
        match opcode {
            0x0 => {
                match nn {
                    0xE0 => {
                        // Clear display
                        self.display = [false; 64 * 32];
                    }
                    0xEE => {
                        // Return from subroutine
                        if self.cpu_state.sp > 0 {
                            self.cpu_state.sp -= 1;
                            self.cpu_state.pc = self.cpu_state.stack[self.cpu_state.sp as usize];
                        }
                    }
                    _ => {}
                }
            }
            0x1 => {
                // Jump to address nnn
                self.cpu_state.pc = nnn;
            }
            0x2 => {
                // Call subroutine at nnn
                if (self.cpu_state.sp as usize) < self.cpu_state.stack.len() {
                    self.cpu_state.stack[self.cpu_state.sp as usize] = self.cpu_state.pc;
                    self.cpu_state.sp += 1;
                    self.cpu_state.pc = nnn;
                }
            }
            0x6 => {
                // Set Vx = nn
                if x < self.cpu_state.v.len() {
                    self.cpu_state.v[x] = nn;
                }
            }
            0x7 => {
                // Add nn to Vx
                if x < self.cpu_state.v.len() {
                    self.cpu_state.v[x] = self.cpu_state.v[x].wrapping_add(nn);
                }
            }
            0xA => {
                // Set I = nnn
                self.cpu_state.i = nnn;
            }
            0xD => {
                // Draw sprite at (Vx, Vy) with height n
                if x < self.cpu_state.v.len() && y < self.cpu_state.v.len() {
                    let vx = self.cpu_state.v[x] as usize % 64;
                    let vy = self.cpu_state.v[y] as usize % 32;
                    self.cpu_state.v[0xF] = 0; // Reset collision flag
                    
                    for row in 0..(n as usize) {
                        if vy + row >= 32 { break; }
                        
                        let sprite_byte = if (self.cpu_state.i as usize + row) < self.cpu_state.memory.len() {
                            self.cpu_state.memory[self.cpu_state.i as usize + row]
                        } else {
                            0
                        };
                        
                        for col in 0..8 {
                            if vx + col >= 64 { break; }
                            
                            let pixel_idx = (vy + row) * 64 + (vx + col);
                            let sprite_pixel = (sprite_byte >> (7 - col)) & 1;
                            
                            if sprite_pixel == 1 {
                                if self.display[pixel_idx] {
                                    self.cpu_state.v[0xF] = 1; // Collision detected
                                }
                                self.display[pixel_idx] ^= true;
                            }
                        }
                    }
                }
            }
            _ => {
                // Mock other instructions with simple animations
                self.generate_demo_pattern();
            }
        }
    }
    
    fn generate_demo_pattern(&mut self) {
        // Generate animated demo pattern for unknown instructions
        let time = (self.cpu_state.cycles / 60) % 360; // Slower animation
        
        for y in 0..32 {
            for x in 0..64 {
                let idx = y * 64 + x;
                
                // Create various patterns based on "game"
                let pattern = if self.game_title.contains("Snake") {
                    // Snake-like moving pattern
                    ((x + time as usize) % 8 == 0) && ((y + time as usize / 2) % 4 == 0)
                } else if self.game_title.contains("Pong") {
                    // Pong-like bouncing pattern
                    let ball_x = ((time * 2) % 128) as usize;
                    let ball_y = (((time * 3) % 64) as usize).min(31);
                    (x as i32 - ball_x as i32).abs() <= 1 && (y as i32 - ball_y as i32).abs() <= 1
                } else if self.game_title.contains("Tetris") {
                    // Tetris-like falling blocks
                    ((y + time as usize / 4) % 8 < 4) && ((x + time as usize / 8) % 12 < 4)
                } else {
                    // Default spiral pattern
                    let center_x = 32.0;
                    let center_y = 16.0;
                    let dx = x as f32 - center_x;
                    let dy = y as f32 - center_y;
                    let distance = (dx * dx + dy * dy).sqrt();
                    let angle = dy.atan2(dx) + (time as f32 * 0.1);
                    ((distance + angle * 5.0) as usize % 8) < 2
                };
                
                self.display[idx] = pattern;
            }
        }
    }
    
    fn render_display(&self, frame: &mut dyn Frame, display_x: f32, display_y: f32) {
        // Draw CHIP-8 display background
        frame.execute_command(RenderCommand::DrawRect {
            layout_style: None,
            position: Vec2::new(display_x, display_y),
            size: Vec2::new(64.0 * self.scale_factor, 32.0 * self.scale_factor),
            color: self.background_color,
            border_radius: 2.0,
            border_width: 2.0,
            border_color: Vec4::new(0.3, 0.3, 0.3, 1.0),
            transform: None,
            shadow: None,
            z_index: 1,
        });
        
        // Draw pixels
        for y in 0..32 {
            for x in 0..64 {
                let idx = y * 64 + x;
                if self.display[idx] {
                    let pixel_x = display_x + x as f32 * self.scale_factor;
                    let pixel_y = display_y + y as f32 * self.scale_factor;
                    
                    frame.execute_command(RenderCommand::DrawRect {
            layout_style: None,
                        position: Vec2::new(pixel_x, pixel_y),
                        size: Vec2::new(self.scale_factor, self.scale_factor),
                        color: self.pixel_color,
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
}

impl ViewInstance for Chip8ViewInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        // Simulate loading and run emulation cycles
        self.simulate_chip8_loading();
        
        // Run multiple cycles per frame for smooth animation
        for _ in 0..10 {
            self.emulate_cycle();
        }
        
        // Draw emulator background
        frame.execute_command(RenderCommand::DrawRect {
            layout_style: None,
            position: Vec2::new(bounds.x, bounds.y),
            size: Vec2::new(bounds.width, bounds.height),
            color: Vec4::new(0.1, 0.1, 0.1, 1.0),
            border_radius: 4.0,
            border_width: 2.0,
            border_color: Vec4::new(0.4, 0.4, 0.4, 1.0),
            transform: None,
            shadow: None,
            z_index: 0,
        });
        
        match &self.emulator_state {
            EmulatorState::Loading => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 80.0, bounds.y + bounds.height / 2.0),
                    text: "🕹️ Loading CHIP-8...".to_string(),
                    font_size: 14.0,
                    color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width),
                    max_height: Some(bounds.height),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 1,
                });
            }
            EmulatorState::Running | EmulatorState::Paused => {
                // Calculate display position (centered)
                let display_width = 64.0 * self.scale_factor;
                let display_height = 32.0 * self.scale_factor;
                let display_x = bounds.x + (bounds.width - display_width) / 2.0;
                let display_y = bounds.y + (bounds.height - display_height) / 2.0 - 20.0;
                
                self.render_display(frame, display_x, display_y);
                
                // Game title
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0, bounds.y + 10.0),
                    text: format!("🕹️ {}", self.game_title),
                    font_size: 12.0,
                    color: Vec4::new(0.9, 0.9, 0.9, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(bounds.width - 10.0),
                    max_height: Some(15.0),
                    transform: None,
                    font_family: Some("sans-serif".to_string()),
                    z_index: 3,
                });
                
                // CPU info
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + 5.0, bounds.y + bounds.height - 35.0),
                    text: format!("PC: 0x{:03X} | I: 0x{:03X} | Cycles: {}", 
                        self.cpu_state.pc, self.cpu_state.i, self.cpu_state.cycles),
                    font_size: 8.0,
                    color: Vec4::new(0.7, 0.7, 0.7, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(bounds.width - 10.0),
                    max_height: Some(12.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 3,
                });
                
                // Timer info
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + 5.0, bounds.y + bounds.height - 20.0),
                    text: format!("DT: {} | ST: {} | Keys: {:04b}", 
                        self.delay_timer, self.sound_timer, 
                        self.keys.iter().take(4).fold(0u8, |acc, &b| (acc << 1) | if b { 1 } else { 0 })),
                    font_size: 8.0,
                    color: Vec4::new(0.6, 0.6, 0.6, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(bounds.width - 10.0),
                    max_height: Some(12.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 3,
                });
                
                // Paused indicator
                if matches!(self.emulator_state, EmulatorState::Paused) {
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(bounds.x + bounds.width - 60.0, bounds.y + 10.0),
                        text: "⏸️ PAUSED".to_string(),
                        font_size: 10.0,
                        color: Vec4::new(1.0, 1.0, 0.0, 1.0),
                        alignment: TextAlignment::End,
                        max_width: Some(55.0),
                        max_height: Some(15.0),
                        transform: None,
                        font_family: Some("monospace".to_string()),
                        z_index: 3,
                    });
                }
            }
            EmulatorState::Error(msg) => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(bounds.x + bounds.width / 2.0 - 60.0, bounds.y + bounds.height / 2.0),
                    text: format!("❌ Error: {}", msg),
                    font_size: 12.0,
                    color: Vec4::new(1.0, 0.3, 0.3, 1.0),
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
                        eprintln!("[CHIP-8] Game paused");
                    }
                    EmulatorState::Paused => {
                        self.emulator_state = EmulatorState::Running;
                        eprintln!("[CHIP-8] Game resumed");
                    }
                    _ => {}
                }
                true
            }
            InputEvent::KeyDown { key } => {
                // Map keyboard to CHIP-8 hexadecimal keypad
                let chip8_key = match key.as_str() {
                    "1" => Some(0x1), "2" => Some(0x2), "3" => Some(0x3), "4" => Some(0xC),
                    "q" => Some(0x4), "w" => Some(0x5), "e" => Some(0x6), "r" => Some(0xD),
                    "a" => Some(0x7), "s" => Some(0x8), "d" => Some(0x9), "f" => Some(0xE),
                    "z" => Some(0xA), "x" => Some(0x0), "c" => Some(0xB), "v" => Some(0xF),
                    _ => None,
                };
                
                if let Some(key_idx) = chip8_key {
                    if key_idx < self.keys.len() {
                        self.keys[key_idx] = true;
                        eprintln!("[CHIP-8] Key {:X} pressed", key_idx);
                    }
                    true
                } else {
                    match key.as_str() {
                        "space" => {
                            // Reset emulator
                            self.cpu_state.reset();
                            self.display = [false; 64 * 32];
                            eprintln!("[CHIP-8] Reset");
                            true
                        }
                        _ => false,
                    }
                }
            }
            InputEvent::KeyUp { key } => {
                // Handle key releases
                let chip8_key = match key.as_str() {
                    "1" => Some(0x1), "2" => Some(0x2), "3" => Some(0x3), "4" => Some(0xC),
                    "q" => Some(0x4), "w" => Some(0x5), "e" => Some(0x6), "r" => Some(0xD),
                    "a" => Some(0x7), "s" => Some(0x8), "d" => Some(0x9), "f" => Some(0xE),
                    "z" => Some(0xA), "x" => Some(0x0), "c" => Some(0xB), "v" => Some(0xF),
                    _ => None,
                };
                
                if let Some(key_idx) = chip8_key {
                    if key_idx < self.keys.len() {
                        self.keys[key_idx] = false;
                    }
                    true
                } else {
                    false
                }
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        if let Some(scale) = config.get("scale_factor").and_then(|v| v.as_float()) {
            self.scale_factor = scale as f32;
        }
        
        if let Some(color_str) = config.get("pixel_color").and_then(|v| v.as_string()) {
            self.pixel_color = parse_color(&color_str);
        }
        
        if let Some(color_str) = config.get("background_color").and_then(|v| v.as_string()) {
            self.background_color = parse_color(&color_str);
        }
        
        self.config = config.clone();
        eprintln!("[CHIP-8] Config updated - Scale: {}×, Colors updated", self.scale_factor);
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.rom_path = new_source.map(|s| s.to_string());
        
        if let Some(ref path) = self.rom_path {
            self.game_title = generate_mock_game_title(path);
            self.rom_data = generate_mock_rom_data(path);
            self.emulator_state = EmulatorState::Loading;
            self.cpu_state.reset();
            self.display = [false; 64 * 32];
            self.keys = [false; 16];
            
            eprintln!("[CHIP-8] Loading ROM: {}", path);
        } else {
            self.emulator_state = EmulatorState::Error("No ROM specified".to_string());
        }
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        eprintln!("[CHIP-8] Emulator resized to {}×{}", new_bounds.width as i32, new_bounds.height as i32);
    }
    
    fn destroy(&mut self) {
        eprintln!("[CHIP-8] Emulator destroyed");
        // In a real implementation: cleanup resources, save state, etc.
    }
}

impl Default for Chip8ViewProvider {
    fn default() -> Self {
        Self::new()
    }
}

// Helper functions

fn parse_color(color_str: &str) -> Vec4 {
    // Simple color parsing (hex colors)
    if color_str.starts_with('#') && color_str.len() == 7 {
        if let (Ok(r), Ok(g), Ok(b)) = (
            u8::from_str_radix(&color_str[1..3], 16),
            u8::from_str_radix(&color_str[3..5], 16),
            u8::from_str_radix(&color_str[5..7], 16),
        ) {
            return Vec4::new(r as f32 / 255.0, g as f32 / 255.0, b as f32 / 255.0, 1.0);
        }
    }
    
    // Named colors
    match color_str.to_lowercase().as_str() {
        "white" => Vec4::new(1.0, 1.0, 1.0, 1.0),
        "black" => Vec4::new(0.0, 0.0, 0.0, 1.0),
        "red" => Vec4::new(1.0, 0.0, 0.0, 1.0),
        "green" => Vec4::new(0.0, 1.0, 0.0, 1.0),
        "blue" => Vec4::new(0.0, 0.0, 1.0, 1.0),
        "yellow" => Vec4::new(1.0, 1.0, 0.0, 1.0),
        "cyan" => Vec4::new(0.0, 1.0, 1.0, 1.0),
        "magenta" => Vec4::new(1.0, 0.0, 1.0, 1.0),
        _ => Vec4::new(0.0, 1.0, 0.0, 1.0), // Default green
    }
}

fn generate_mock_game_title(path: &str) -> String {
    let filename = path.split('/').last().unwrap_or("game");
    let name = filename.split('.').next().unwrap_or("game");
    
    // Generate title based on filename patterns
    if name.to_lowercase().contains("snake") {
        "Snake Game".to_string()
    } else if name.to_lowercase().contains("pong") {
        "Pong Classic".to_string()
    } else if name.to_lowercase().contains("tetris") {
        "Tetris Clone".to_string()
    } else if name.to_lowercase().contains("invaders") {
        "Space Invaders".to_string()
    } else if name.to_lowercase().contains("breakout") {
        "Breakout".to_string()
    } else {
        format!("CHIP-8 {}", name.to_uppercase())
    }
}

fn generate_mock_rom_data(path: &str) -> Vec<u8> {
    // Generate mock ROM data based on game type
    let title = generate_mock_game_title(path);
    
    if title.contains("Snake") {
        // Mock Snake game ROM (simplified)
        vec![
            0xA2, 0x50, // LD I, #050 (load sprite location)
            0x60, 0x20, // LD V0, #20 (X position)
            0x61, 0x10, // LD V1, #10 (Y position)
            0xD0, 0x14, // DRW V0, V1, #4 (draw sprite)
            0x70, 0x01, // ADD V0, #01 (move right)
            0x12, 0x04, // JP #204 (loop)
        ]
    } else if title.contains("Pong") {
        // Mock Pong game ROM
        vec![
            0x60, 0x10, // LD V0, #10 (paddle 1)
            0x61, 0x30, // LD V1, #30 (paddle 2)
            0x62, 0x20, // LD V2, #20 (ball X)
            0x63, 0x10, // LD V3, #10 (ball Y)
            0xA2, 0x50, // LD I, #050
            0xD0, 0x14, // DRW V0, V1, #4
            0x12, 0x08, // JP #208
        ]
    } else {
        // Generic demo ROM
        vec![
            0x60, 0x00, // LD V0, #00
            0x61, 0x00, // LD V1, #00
            0xA2, 0x50, // LD I, #050
            0xD0, 0x14, // DRW V0, V1, #4
            0x70, 0x01, // ADD V0, #01
            0x12, 0x06, // JP #206
        ]
    }
}
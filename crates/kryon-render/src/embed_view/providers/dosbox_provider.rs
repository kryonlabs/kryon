//! DOSBox ViewProvider Implementation
//! 
//! Provides DOSBox emulation for running retro PC games and applications.

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// DOSBox emulator provider for retro PC gaming
pub struct DosBoxViewProvider;

impl DosBoxViewProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for DosBoxViewProvider {
    fn type_id(&self) -> &'static str {
        "dosbox"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(DosBoxViewInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // DOSBox provider handles DOS executables and disk images
        if let Some(src) = source {
            src.ends_with(".exe") ||
            src.ends_with(".com") ||
            src.ends_with(".bat") ||
            src.ends_with(".img") ||
            src.ends_with(".iso") ||
            src.contains("dosbox")
        } else {
            false
        }
    }
}

/// DOSBox emulator instance
pub struct DosBoxViewInstance {
    executable_path: Option<String>,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
    dos_state: DosState,
    scale_factor: f32,
    sound_enabled: bool,
    cpu_speed: u32, // CPU cycles
    memory_size: u32, // MB
    video_mode: VideoMode,
    screen_buffer: Vec<u8>, // VGA screen buffer
    command_history: Vec<String>,
    current_directory: String,
}

#[derive(Debug, Clone)]
enum DosState {
    BootingDos,
    CommandPrompt,
    RunningProgram(String),
    Error(String),
}

#[derive(Debug, Clone)]
enum VideoMode {
    TextMode80x25,
    VGA320x200,
    VGA640x480,
    SVGA800x600,
}

impl DosBoxViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let executable_path = source.map(|s| s.to_string());
        
        let scale_factor = config.get("scale_factor")
            .and_then(|v| v.as_float())
            .unwrap_or(2.0) as f32;
            
        let sound_enabled = config.get("sound_enabled")
            .and_then(|v| v.as_bool())
            .unwrap_or(true);
            
        let cpu_speed = config.get("cpu_speed")
            .and_then(|v| v.as_int())
            .unwrap_or(3000) as u32; // Default to 3000 cycles
            
        let memory_size = config.get("memory_size")
            .and_then(|v| v.as_int())
            .unwrap_or(16) as u32; // Default to 16MB
        
        let video_mode = config.get("video_mode")
            .and_then(|v| v.as_string())
            .map(|mode| match mode {
                "text" => VideoMode::TextMode80x25,
                "vga320" => VideoMode::VGA320x200,
                "vga640" => VideoMode::VGA640x480,
                "svga800" => VideoMode::SVGA800x600,
                _ => VideoMode::VGA320x200,
            })
            .unwrap_or(VideoMode::VGA320x200);
        
        let (width, height) = match video_mode {
            VideoMode::TextMode80x25 => (640, 400), // 80x25 text mode
            VideoMode::VGA320x200 => (320, 200),
            VideoMode::VGA640x480 => (640, 480),
            VideoMode::SVGA800x600 => (800, 600),
        };
        
        Self {
            executable_path,
            bounds: Rect::new(0.0, 0.0, width as f32, height as f32),
            config: config.clone(),
            dos_state: DosState::BootingDos,
            scale_factor,
            sound_enabled,
            cpu_speed,
            memory_size,
            video_mode,
            screen_buffer: vec![0; width * height * 3], // RGB buffer
            command_history: Vec::new(),
            current_directory: "C:\\".to_string(),
        }
    }
    
    fn simulate_dos_boot(&mut self) {
        match &self.dos_state {
            DosState::BootingDos => {
                // Simulate boot sequence completion
                self.dos_state = DosState::CommandPrompt;
                self.generate_dos_screen();
                eprintln!("[DOSBox] DOS booted successfully");
            }
            _ => {}
        }
    }
    
    fn generate_dos_screen(&mut self) {
        // Generate mock DOS screen content based on video mode
        match self.video_mode {
            VideoMode::TextMode80x25 => self.generate_text_mode_screen(),
            _ => self.generate_graphics_mode_screen(),
        }
    }
    
    fn generate_text_mode_screen(&mut self) {
        // Fill with blue background (classic DOS)
        for pixel in self.screen_buffer.chunks_mut(3) {
            pixel[0] = 0;   // R
            pixel[1] = 0;   // G  
            pixel[2] = 170; // B (DOS blue)
        }
    }
    
    fn generate_graphics_mode_screen(&mut self) {
        // Generate a retro-style pattern
        let (width, height) = match self.video_mode {
            VideoMode::VGA320x200 => (320, 200),
            VideoMode::VGA640x480 => (640, 480),
            VideoMode::SVGA800x600 => (800, 600),
            _ => (320, 200),
        };
        
        for y in 0..height {
            for x in 0..width {
                let idx = (y * width + x) * 3;
                
                // Create a retro gradient pattern
                let pattern = ((x + y) / 8) % 16;
                let color = pattern * 16;
                
                if idx + 2 < self.screen_buffer.len() {
                    self.screen_buffer[idx] = color as u8;     // R
                    self.screen_buffer[idx + 1] = (color / 2) as u8; // G
                    self.screen_buffer[idx + 2] = (color / 4) as u8; // B
                }
            }
        }
    }
    
    fn execute_dos_command(&mut self, command: &str) {
        self.command_history.push(command.to_string());
        
        match command.to_lowercase().as_str() {
            "dir" => {
                eprintln!("[DOSBox] Listing directory: {}", self.current_directory);
            }
            "cls" => {
                eprintln!("[DOSBox] Screen cleared");
                self.generate_dos_screen();
            }
            cmd if cmd.starts_with("cd ") => {
                let new_dir = cmd.strip_prefix("cd ").unwrap_or("");
                self.current_directory = format!("C:\\{}", new_dir);
                eprintln!("[DOSBox] Changed directory to: {}", self.current_directory);
            }
            _ => {
                if let Some(ref exe_path) = self.executable_path {
                    if command.contains(&exe_path.split('/').last().unwrap_or("")) {
                        self.dos_state = DosState::RunningProgram(command.to_string());
                        eprintln!("[DOSBox] Running program: {}", command);
                    }
                }
            }
        }
    }
}

impl ViewInstance for DosBoxViewInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        // Simulate DOS boot/operation
        self.simulate_dos_boot();
        
        // Draw DOSBox window frame
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(bounds.x, bounds.y),
            size: Vec2::new(bounds.width, bounds.height),
            color: Vec4::new(0.1, 0.1, 0.1, 1.0), // Dark frame
            border_radius: 4.0,
            border_width: 2.0,
            border_color: Vec4::new(0.4, 0.4, 0.4, 1.0),
            transform: None,
            shadow: None,
            z_index: 0,
        });
        
        // Calculate screen area
        let screen_width = bounds.width - 20.0;
        let screen_height = bounds.height - 60.0;
        let screen_x = bounds.x + 10.0;
        let screen_y = bounds.y + 30.0;
        
        // Draw DOS screen
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(screen_x, screen_y),
            size: Vec2::new(screen_width, screen_height),
            color: Vec4::new(0.0, 0.0, 0.67, 1.0), // DOS blue background
            border_radius: 2.0,
            border_width: 1.0,
            border_color: Vec4::new(0.8, 0.8, 0.8, 1.0),
            transform: None,
            shadow: None,
            z_index: 1,
        });
        
        match &self.dos_state {
            DosState::BootingDos => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(screen_x + 10.0, screen_y + 10.0),
                    text: "MS-DOS starting...".to_string(),
                    font_size: 12.0,
                    color: Vec4::new(1.0, 1.0, 1.0, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(screen_width - 20.0),
                    max_height: Some(20.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 2,
                });
            }
            DosState::CommandPrompt => {
                // Draw DOS prompt
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(screen_x + 10.0, screen_y + 10.0),
                    text: format!("Microsoft MS-DOS [Version 6.22]"),
                    font_size: 10.0,
                    color: Vec4::new(1.0, 1.0, 1.0, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(screen_width - 20.0),
                    max_height: Some(15.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 2,
                });
                
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(screen_x + 10.0, screen_y + 30.0),
                    text: "(C) Copyright Microsoft Corp 1981-1994.".to_string(),
                    font_size: 10.0,
                    color: Vec4::new(1.0, 1.0, 1.0, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(screen_width - 20.0),
                    max_height: Some(15.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 2,
                });
                
                // Command prompt
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(screen_x + 10.0, screen_y + 60.0),
                    text: format!("{}> _", self.current_directory),
                    font_size: 12.0,
                    color: Vec4::new(1.0, 1.0, 1.0, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(screen_width - 20.0),
                    max_height: Some(15.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 2,
                });
                
                // Show last command if any
                if let Some(last_cmd) = self.command_history.last() {
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(screen_x + 10.0, screen_y + 80.0),
                        text: format!("Last: {}", last_cmd),
                        font_size: 10.0,
                        color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                        alignment: TextAlignment::Start,
                        max_width: Some(screen_width - 20.0),
                        max_height: Some(15.0),
                        transform: None,
                        font_family: Some("monospace".to_string()),
                        z_index: 2,
                    });
                }
            }
            DosState::RunningProgram(program) => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(screen_x + screen_width / 2.0 - 60.0, screen_y + screen_height / 2.0),
                    text: format!("🎮 Running: {}", program),
                    font_size: 14.0,
                    color: Vec4::new(1.0, 1.0, 0.0, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(screen_width),
                    max_height: Some(screen_height),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 2,
                });
                
                // Draw mock game graphics
                let block_size = 20.0;
                for i in 0..5 {
                    for j in 0..3 {
                        let x = screen_x + 50.0 + i as f32 * block_size * 1.5;
                        let y = screen_y + 120.0 + j as f32 * block_size * 1.5;
                        let color = Vec4::new(
                            (i as f32 * 0.2 + 0.2) % 1.0,
                            (j as f32 * 0.3 + 0.3) % 1.0,
                            0.8,
                            1.0
                        );
                        
                        frame.execute_command(RenderCommand::DrawRect {
                            position: Vec2::new(x, y),
                            size: Vec2::new(block_size, block_size),
                            color,
                            border_radius: 2.0,
                            border_width: 1.0,
                            border_color: Vec4::new(1.0, 1.0, 1.0, 0.5),
                            transform: None,
                            shadow: None,
                            z_index: 3,
                        });
                    }
                }
            }
            DosState::Error(msg) => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(screen_x + 10.0, screen_y + screen_height / 2.0),
                    text: format!("Bad command or file name: {}", msg),
                    font_size: 12.0,
                    color: Vec4::new(1.0, 0.2, 0.2, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(screen_width - 20.0),
                    max_height: Some(screen_height),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 2,
                });
            }
        }
        
        // Draw title bar
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(bounds.x + 5.0, bounds.y + 5.0),
            text: format!("💾 DOSBox - {} CPU | {}MB RAM", self.cpu_speed, self.memory_size),
            font_size: 10.0,
            color: Vec4::new(0.9, 0.9, 0.9, 1.0),
            alignment: TextAlignment::Start,
            max_width: Some(bounds.width - 10.0),
            max_height: Some(20.0),
            transform: None,
            font_family: Some("monospace".to_string()),
            z_index: 4,
        });
        
        // Video mode indicator
        let mode_text = match self.video_mode {
            VideoMode::TextMode80x25 => "Text 80x25",
            VideoMode::VGA320x200 => "VGA 320x200",
            VideoMode::VGA640x480 => "VGA 640x480", 
            VideoMode::SVGA800x600 => "SVGA 800x600",
        };
        
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(bounds.x + bounds.width - 80.0, bounds.y + bounds.height - 15.0),
            text: mode_text.to_string(),
            font_size: 8.0,
            color: Vec4::new(0.6, 0.6, 0.6, 1.0),
            alignment: TextAlignment::End,
            max_width: Some(75.0),
            max_height: Some(12.0),
            transform: None,
            font_family: Some("monospace".to_string()),
            z_index: 4,
        });
    }
    
    fn handle_event(&mut self, event: &InputEvent) -> bool {
        match event {
            InputEvent::MouseDown { position: _, button: _ } => {
                match &self.dos_state {
                    DosState::CommandPrompt => {
                        self.execute_dos_command("dir");
                        true
                    }
                    DosState::RunningProgram(_) => {
                        self.dos_state = DosState::CommandPrompt;
                        eprintln!("[DOSBox] Program exited to DOS prompt");
                        true
                    }
                    _ => false
                }
            }
            InputEvent::KeyDown { key } => {
                match key.as_str() {
                    "Return" => {
                        if let Some(exe_path) = self.executable_path.clone() {
                            let exe_name = exe_path.split('/').last().unwrap_or("program.exe");
                            self.execute_dos_command(exe_name);
                        }
                        true
                    }
                    "Escape" => {
                        if matches!(self.dos_state, DosState::RunningProgram(_)) {
                            self.dos_state = DosState::CommandPrompt;
                            eprintln!("[DOSBox] Exited to DOS prompt");
                        }
                        true
                    }
                    "F12" => {
                        // Toggle video mode
                        self.video_mode = match self.video_mode {
                            VideoMode::TextMode80x25 => VideoMode::VGA320x200,
                            VideoMode::VGA320x200 => VideoMode::VGA640x480,
                            VideoMode::VGA640x480 => VideoMode::SVGA800x600,
                            VideoMode::SVGA800x600 => VideoMode::TextMode80x25,
                        };
                        self.generate_dos_screen();
                        eprintln!("[DOSBox] Switched video mode");
                        true
                    }
                    _ => false,
                }
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        self.cpu_speed = config.get("cpu_speed")
            .and_then(|v| v.as_int())
            .unwrap_or(self.cpu_speed as i32) as u32;
        
        self.memory_size = config.get("memory_size")
            .and_then(|v| v.as_int())
            .unwrap_or(self.memory_size as i32) as u32;
        
        self.sound_enabled = config.get("sound_enabled")
            .and_then(|v| v.as_bool())
            .unwrap_or(self.sound_enabled);
        
        self.config = config.clone();
        eprintln!("[DOSBox] Config updated - CPU: {}, RAM: {}MB, Sound: {}", 
                 self.cpu_speed, self.memory_size, self.sound_enabled);
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.executable_path = new_source.map(|s| s.to_string());
        self.dos_state = DosState::BootingDos;
        self.command_history.clear();
        
        if let Some(ref path) = self.executable_path {
            eprintln!("[DOSBox] Loading executable: {}", path);
        }
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        eprintln!("[DOSBox] Resized to {}×{}", new_bounds.width as i32, new_bounds.height as i32);
    }
    
    fn destroy(&mut self) {
        eprintln!("[DOSBox] Emulator destroyed");
        // In a real implementation: cleanup DOSBox processes, save state, etc.
    }
}

impl Default for DosBoxViewProvider {
    fn default() -> Self {
        Self::new()
    }
}
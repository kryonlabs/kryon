//! Video ViewProvider Implementation
//! 
//! Provides video playback capabilities with media controls and format support.

use super::super::*;
use crate::RenderCommand;
use kryon_core::TextAlignment;
use glam::{Vec2, Vec4};
use std::collections::HashMap;
use kryon_core::PropertyValue;

/// Video player provider for media playback
pub struct VideoViewProvider;

impl VideoViewProvider {
    pub fn new() -> Self {
        Self
    }
}

impl ViewProvider for VideoViewProvider {
    fn type_id(&self) -> &'static str {
        "video"
    }
    
    fn create_view(&self, source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Box<dyn ViewInstance> {
        Box::new(VideoViewInstance::new(source, config))
    }
    
    fn can_handle(&self, source: Option<&str>) -> bool {
        // Video provider handles common video file formats
        if let Some(src) = source {
            src.ends_with(".mp4") ||
            src.ends_with(".mkv") ||
            src.ends_with(".avi") ||
            src.ends_with(".mov") ||
            src.ends_with(".webm") ||
            src.ends_with(".wmv") ||
            src.ends_with(".flv") ||
            src.ends_with(".m4v") ||
            src.starts_with("http") && (src.contains("youtube") || src.contains("vimeo")) ||
            src.contains("video/")
        } else {
            false
        }
    }
}

/// Video player instance
pub struct VideoViewInstance {
    video_path: Option<String>,
    bounds: Rect,
    config: HashMap<String, PropertyValue>,
    player_state: PlayerState,
    current_time: f32, // seconds
    duration: f32, // seconds
    volume: f32, // 0.0 to 1.0
    playback_speed: f32,
    is_muted: bool,
    is_fullscreen: bool,
    // Video metadata
    video_title: String,
    video_format: VideoFormat,
    resolution: (u32, u32),
    frame_rate: f32,
    bitrate: u32,
    // Mock video frame simulation
    frame_buffer: Vec<u8>, // RGB buffer for current frame
    _last_frame_time: f32,
}

#[derive(Debug, Clone)]
enum PlayerState {
    Loading,
    Playing,
    Paused,
    Stopped,
    Buffering,
    Error(String),
}

#[derive(Debug, Clone)]
enum VideoFormat {
    MP4,
    MKV,
    AVI,
    MOV,
    WebM,
    WMV,
    FLV,
    Stream,
    Unknown,
}

impl VideoViewInstance {
    pub fn new(source: Option<&str>, config: &HashMap<String, PropertyValue>) -> Self {
        let video_path = source.map(|s| s.to_string());
        
        let volume = config.get("volume")
            .and_then(|v| v.as_float())
            .unwrap_or(0.8) as f32;
            
        let playback_speed = config.get("playback_speed")
            .and_then(|v| v.as_float())
            .unwrap_or(1.0) as f32;
            
        let auto_play = config.get("auto_play")
            .and_then(|v| v.as_bool())
            .unwrap_or(false);
        
        let (player_state, video_title, video_format, resolution, duration) = if let Some(ref path) = video_path {
            let format = detect_video_format(path);
            let (title, res, dur) = generate_mock_video_metadata(path);
            let state = if auto_play { PlayerState::Loading } else { PlayerState::Stopped };
            (state, title, format, res, dur)
        } else {
            (PlayerState::Error("No video file specified".to_string()), "Error".to_string(), VideoFormat::Unknown, (640, 480), 0.0)
        };
        
        let frame_buffer_size = (resolution.0 * resolution.1 * 3) as usize; // RGB
        
        Self {
            video_path,
            bounds: Rect::new(0.0, 0.0, 800.0, 600.0),
            config: config.clone(),
            player_state,
            current_time: 0.0,
            duration,
            volume,
            playback_speed,
            is_muted: false,
            is_fullscreen: false,
            video_title,
            video_format,
            resolution,
            frame_rate: 30.0,
            bitrate: 2000, // kbps
            frame_buffer: vec![0; frame_buffer_size],
            _last_frame_time: 0.0,
        }
    }
    
    fn simulate_video_loading(&mut self) {
        match &self.player_state {
            PlayerState::Loading => {
                // Simulate buffering state first
                self.player_state = PlayerState::Buffering;
                eprintln!("[Video] Buffering video...");
            }
            PlayerState::Buffering => {
                // Simulate loading completion
                self.player_state = PlayerState::Playing;
                if let Some(ref path) = self.video_path {
                    eprintln!("[Video] Video loaded: {}", path);
                    eprintln!("[Video] Resolution: {}x{}, Duration: {:.1}s", 
                             self.resolution.0, self.resolution.1, self.duration);
                }
            }
            _ => {}
        }
    }
    
    fn update_playback(&mut self, delta_time: f32) {
        if matches!(self.player_state, PlayerState::Playing) {
            self.current_time += delta_time * self.playback_speed;
            
            if self.current_time >= self.duration {
                self.current_time = self.duration;
                self.player_state = PlayerState::Stopped;
                eprintln!("[Video] Playback finished");
            } else {
                // Generate new frame
                self.generate_mock_video_frame();
            }
        }
    }
    
    fn generate_mock_video_frame(&mut self) {
        let time_progress = self.current_time / self.duration.max(1.0);
        let width = self.resolution.0 as usize;
        let height = self.resolution.1 as usize;
        
        // Generate animated video content based on time
        for y in 0..height {
            for x in 0..width {
                let idx = (y * width + x) * 3;
                
                if idx + 2 < self.frame_buffer.len() {
                    // Create moving patterns based on video format
                    let (r, g, b) = match self.video_format {
                        VideoFormat::MP4 | VideoFormat::MKV => {
                            // Blue/purple gradient with moving wave
                            let wave = ((x as f32 / 50.0 + time_progress * 10.0).sin() * 127.0 + 128.0) as u8;
                            let r = ((y as f32 / height as f32) * 255.0) as u8;
                            let g = wave / 2;
                            let b = wave;
                            (r, g, b)
                        }
                        VideoFormat::AVI | VideoFormat::WMV => {
                            // Green/yellow pattern
                            let pattern = ((x + y) as f32 / 20.0 + time_progress * 5.0).sin();
                            let intensity = (pattern * 127.0 + 128.0) as u8;
                            (intensity / 2, intensity, intensity / 3)
                        }
                        VideoFormat::WebM | VideoFormat::MOV => {
                            // Rotating color wheel
                            let center_x = width as f32 / 2.0;
                            let center_y = height as f32 / 2.0;
                            let dx = x as f32 - center_x;
                            let dy = y as f32 - center_y;
                            let angle = (dx.atan2(dy) + time_progress * 2.0 * std::f32::consts::PI) % (2.0 * std::f32::consts::PI);
                            let distance = (dx * dx + dy * dy).sqrt() / (width.min(height) as f32 / 2.0);
                            
                            let r = ((angle / (2.0 * std::f32::consts::PI)) * 255.0) as u8;
                            let g = ((distance * 255.0).min(255.0)) as u8;
                            let b = ((1.0 - distance) * 255.0).max(0.0) as u8;
                            (r, g, b)
                        }
                        _ => {
                            // Default colorful noise
                            let noise = ((x + y + (time_progress * 1000.0) as usize) % 256) as u8;
                            (noise, noise / 2, noise / 3)
                        }
                    };
                    
                    self.frame_buffer[idx] = r;
                    self.frame_buffer[idx + 1] = g;
                    self.frame_buffer[idx + 2] = b;
                }
            }
        }
    }
    
    fn render_video_frame(&self, frame: &mut dyn Frame, video_x: f32, video_y: f32, video_width: f32, video_height: f32) {
        // Draw video background
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(video_x, video_y),
            size: Vec2::new(video_width, video_height),
            color: Vec4::new(0.0, 0.0, 0.0, 1.0),
            border_radius: 4.0,
            border_width: 1.0,
            border_color: Vec4::new(0.3, 0.3, 0.3, 1.0),
            transform: None,
            shadow: None,
            z_index: 1,
        });
        
        // Render video pixels (sample for performance)
        let width = self.resolution.0 as usize;
        let height = self.resolution.1 as usize;
        let pixel_width = video_width / width as f32;
        let pixel_height = video_height / height as f32;
        
        // Sample every 4th pixel for performance
        for y in (0..height).step_by(4) {
            for x in (0..width).step_by(4) {
                let idx = (y * width + x) * 3;
                
                if idx + 2 < self.frame_buffer.len() {
                    let r = self.frame_buffer[idx] as f32 / 255.0;
                    let g = self.frame_buffer[idx + 1] as f32 / 255.0;
                    let b = self.frame_buffer[idx + 2] as f32 / 255.0;
                    
                    let pixel_x = video_x + x as f32 * pixel_width;
                    let pixel_y = video_y + y as f32 * pixel_height;
                    
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
    
    fn render_media_controls(&self, frame: &mut dyn Frame, controls_x: f32, controls_y: f32, controls_width: f32, controls_height: f32) {
        // Draw controls background
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(controls_x, controls_y),
            size: Vec2::new(controls_width, controls_height),
            color: Vec4::new(0.1, 0.1, 0.1, 0.9),
            border_radius: 4.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
            transform: None,
            shadow: None,
            z_index: 3,
        });
        
        // Play/pause button
        let play_button_text = match self.player_state {
            PlayerState::Playing => "⏸️",
            PlayerState::Paused | PlayerState::Stopped => "▶️",
            PlayerState::Loading | PlayerState::Buffering => "⏳",
            PlayerState::Error(_) => "❌",
        };
        
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(controls_x + 10.0, controls_y + 8.0),
            text: play_button_text.to_string(),
            font_size: 20.0,
            color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            alignment: TextAlignment::Start,
            max_width: Some(30.0),
            max_height: Some(controls_height - 4.0),
            transform: None,
            font_family: None,
            z_index: 4,
        });
        
        // Progress bar
        let progress_x = controls_x + 50.0;
        let progress_width = controls_width - 150.0;
        let progress_y = controls_y + controls_height / 2.0 - 2.0;
        let progress_height = 4.0;
        
        // Background track
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(progress_x, progress_y),
            size: Vec2::new(progress_width, progress_height),
            color: Vec4::new(0.3, 0.3, 0.3, 1.0),
            border_radius: 2.0,
            border_width: 0.0,
            border_color: Vec4::ZERO,
            transform: None,
            shadow: None,
            z_index: 4,
        });
        
        // Progress fill
        if self.duration > 0.0 {
            let progress_ratio = self.current_time / self.duration;
            let fill_width = progress_width * progress_ratio;
            
            frame.execute_command(RenderCommand::DrawRect {
                position: Vec2::new(progress_x, progress_y),
                size: Vec2::new(fill_width, progress_height),
                color: Vec4::new(1.0, 0.3, 0.3, 1.0), // Red progress
                border_radius: 2.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
                transform: None,
                shadow: None,
                z_index: 5,
            });
        }
        
        // Time display
        let time_text = format!("{:02}:{:02} / {:02}:{:02}", 
            (self.current_time / 60.0) as i32, (self.current_time % 60.0) as i32,
            (self.duration / 60.0) as i32, (self.duration % 60.0) as i32
        );
        
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(controls_x + controls_width - 90.0, controls_y + 8.0),
            text: time_text,
            font_size: 12.0,
            color: Vec4::new(0.9, 0.9, 0.9, 1.0),
            alignment: TextAlignment::Start,
            max_width: Some(85.0),
            max_height: Some(controls_height - 4.0),
            transform: None,
            font_family: Some("monospace".to_string()),
            z_index: 4,
        });
        
        // Volume control
        let volume_icon = if self.is_muted { "🔇" } else if self.volume > 0.5 { "🔊" } else { "🔉" };
        frame.execute_command(RenderCommand::DrawText {
            position: Vec2::new(controls_x + controls_width - 25.0, controls_y + 8.0),
            text: volume_icon.to_string(),
            font_size: 16.0,
            color: Vec4::new(1.0, 1.0, 1.0, 1.0),
            alignment: TextAlignment::Start,
            max_width: Some(20.0),
            max_height: Some(controls_height - 4.0),
            transform: None,
            font_family: None,
            z_index: 4,
        });
    }
}

impl ViewInstance for VideoViewInstance {
    fn draw(&mut self, frame: &mut dyn Frame, bounds: Rect) {
        self.bounds = bounds;
        
        // Simulate video loading and update playback
        self.simulate_video_loading();
        self.update_playback(0.016); // Assume 60 FPS update
        
        // Draw player background
        frame.execute_command(RenderCommand::DrawRect {
            position: Vec2::new(bounds.x, bounds.y),
            size: Vec2::new(bounds.width, bounds.height),
            color: Vec4::new(0.05, 0.05, 0.05, 1.0),
            border_radius: 4.0,
            border_width: 2.0,
            border_color: Vec4::new(0.3, 0.3, 0.3, 1.0),
            transform: None,
            shadow: None,
            z_index: 0,
        });
        
        // Calculate video area (leave space for controls)
        let controls_height = 50.0;
        let margin = 10.0;
        let video_area = Rect::new(
            bounds.x + margin,
            bounds.y + margin,
            bounds.width - margin * 2.0,
            bounds.height - margin * 2.0 - controls_height,
        );
        
        match &self.player_state {
            PlayerState::Loading => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(video_area.x + video_area.width / 2.0 - 80.0, video_area.y + video_area.height / 2.0),
                    text: "🎬 Loading video...".to_string(),
                    font_size: 16.0,
                    color: Vec4::new(0.8, 0.8, 0.8, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(video_area.width),
                    max_height: Some(video_area.height),
                    transform: None,
                    font_family: Some("sans-serif".to_string()),
                    z_index: 1,
                });
            }
            PlayerState::Playing | PlayerState::Paused | PlayerState::Stopped => {
                self.render_video_frame(frame, video_area.x, video_area.y, video_area.width, video_area.height);
                
                // Video info overlay
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(video_area.x + 5.0, video_area.y + 5.0),
                    text: format!("🎬 {}", self.video_title),
                    font_size: 12.0,
                    color: Vec4::new(1.0, 1.0, 1.0, 0.9),
                    alignment: TextAlignment::Start,
                    max_width: Some(video_area.width - 10.0),
                    max_height: Some(15.0),
                    transform: None,
                    font_family: Some("sans-serif".to_string()),
                    z_index: 3,
                });
                
                // Format and quality info
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(video_area.x + 5.0, video_area.y + 20.0),
                    text: format!("{:?} {}x{} @{:.1}fps {}kbps", 
                        self.video_format, self.resolution.0, self.resolution.1, 
                        self.frame_rate, self.bitrate),
                    font_size: 8.0,
                    color: Vec4::new(0.8, 0.8, 0.8, 0.8),
                    alignment: TextAlignment::Start,
                    max_width: Some(video_area.width - 10.0),
                    max_height: Some(12.0),
                    transform: None,
                    font_family: Some("monospace".to_string()),
                    z_index: 3,
                });
                
                // Playback speed indicator
                if self.playback_speed != 1.0 {
                    frame.execute_command(RenderCommand::DrawText {
                        position: Vec2::new(video_area.x + video_area.width - 60.0, video_area.y + 5.0),
                        text: format!("{}×", self.playback_speed),
                        font_size: 12.0,
                        color: Vec4::new(1.0, 1.0, 0.0, 0.9),
                        alignment: TextAlignment::End,
                        max_width: Some(55.0),
                        max_height: Some(15.0),
                        transform: None,
                        font_family: Some("sans-serif".to_string()),
                        z_index: 3,
                    });
                }
            }
            PlayerState::Buffering => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(video_area.x + video_area.width / 2.0 - 40.0, video_area.y + video_area.height / 2.0),
                    text: "⏳ Buffering...".to_string(),
                    font_size: 16.0,
                    color: Vec4::new(1.0, 1.0, 0.0, 1.0),
                    alignment: TextAlignment::Center,
                    max_width: Some(video_area.width),
                    max_height: Some(video_area.height),
                    transform: None,
                    font_family: None,
                    z_index: 1,
                });
            }
            PlayerState::Error(msg) => {
                frame.execute_command(RenderCommand::DrawText {
                    position: Vec2::new(video_area.x + 20.0, video_area.y + video_area.height / 2.0),
                    text: format!("❌ Error: {}", msg),
                    font_size: 14.0,
                    color: Vec4::new(1.0, 0.3, 0.3, 1.0),
                    alignment: TextAlignment::Start,
                    max_width: Some(video_area.width - 40.0),
                    max_height: Some(video_area.height),
                    transform: None,
                    font_family: None,
                    z_index: 1,
                });
            }
        }
        
        // Draw media controls
        let controls_y = bounds.y + bounds.height - controls_height;
        self.render_media_controls(frame, bounds.x, controls_y, bounds.width, controls_height);
    }
    
    fn handle_event(&mut self, event: &InputEvent) -> bool {
        match event {
            InputEvent::MouseDown { position: _, button: _ } => {
                // Toggle play/pause on click
                match self.player_state {
                    PlayerState::Playing => {
                        self.player_state = PlayerState::Paused;
                        eprintln!("[Video] Paused at {:.1}s", self.current_time);
                    }
                    PlayerState::Paused | PlayerState::Stopped => {
                        self.player_state = PlayerState::Playing;
                        eprintln!("[Video] Playing from {:.1}s", self.current_time);
                    }
                    _ => {}
                }
                true
            }
            InputEvent::KeyDown { key } => {
                match key.as_str() {
                    "space" => {
                        // Toggle play/pause
                        match self.player_state {
                            PlayerState::Playing => self.player_state = PlayerState::Paused,
                            PlayerState::Paused | PlayerState::Stopped => self.player_state = PlayerState::Playing,
                            _ => {}
                        }
                        true
                    }
                    "Right" => {
                        // Seek forward 10 seconds
                        self.current_time = (self.current_time + 10.0).min(self.duration);
                        eprintln!("[Video] Seek forward: {:.1}s", self.current_time);
                        true
                    }
                    "Left" => {
                        // Seek backward 10 seconds
                        self.current_time = (self.current_time - 10.0).max(0.0);
                        eprintln!("[Video] Seek backward: {:.1}s", self.current_time);
                        true
                    }
                    "Up" => {
                        // Volume up
                        self.volume = (self.volume + 0.1).min(1.0);
                        self.is_muted = false;
                        eprintln!("[Video] Volume: {:.0}%", self.volume * 100.0);
                        true
                    }
                    "Down" => {
                        // Volume down
                        self.volume = (self.volume - 0.1).max(0.0);
                        eprintln!("[Video] Volume: {:.0}%", self.volume * 100.0);
                        true
                    }
                    "m" => {
                        // Toggle mute
                        self.is_muted = !self.is_muted;
                        eprintln!("[Video] {}", if self.is_muted { "Muted" } else { "Unmuted" });
                        true
                    }
                    "f" => {
                        // Toggle fullscreen (mock)
                        self.is_fullscreen = !self.is_fullscreen;
                        eprintln!("[Video] Fullscreen: {}", self.is_fullscreen);
                        true
                    }
                    "Equal" | "Plus" => {
                        // Increase playback speed
                        self.playback_speed = (self.playback_speed + 0.25).min(4.0);
                        eprintln!("[Video] Playback speed: {}×", self.playback_speed);
                        true
                    }
                    "Minus" => {
                        // Decrease playback speed
                        self.playback_speed = (self.playback_speed - 0.25).max(0.25);
                        eprintln!("[Video] Playback speed: {}×", self.playback_speed);
                        true
                    }
                    "Home" => {
                        // Seek to beginning
                        self.current_time = 0.0;
                        eprintln!("[Video] Seek to beginning");
                        true
                    }
                    "End" => {
                        // Seek to end
                        self.current_time = self.duration;
                        self.player_state = PlayerState::Stopped;
                        eprintln!("[Video] Seek to end");
                        true
                    }
                    _ => false,
                }
            }
            _ => false,
        }
    }
    
    fn update_config(&mut self, config: &HashMap<String, PropertyValue>) {
        if let Some(volume) = config.get("volume").and_then(|v| v.as_float()) {
            self.volume = volume as f32;
        }
        
        if let Some(speed) = config.get("playback_speed").and_then(|v| v.as_float()) {
            self.playback_speed = speed as f32;
        }
        
        self.config = config.clone();
        eprintln!("[Video] Config updated - Volume: {:.0}%, Speed: {}×", 
                 self.volume * 100.0, self.playback_speed);
    }
    
    fn update_source(&mut self, new_source: Option<&str>) {
        self.video_path = new_source.map(|s| s.to_string());
        
        if let Some(ref path) = self.video_path {
            self.video_format = detect_video_format(path);
            let (title, resolution, duration) = generate_mock_video_metadata(path);
            self.video_title = title;
            self.resolution = resolution;
            self.duration = duration;
            self.current_time = 0.0;
            self.player_state = PlayerState::Loading;
            
            // Resize frame buffer
            let frame_buffer_size = (self.resolution.0 * self.resolution.1 * 3) as usize;
            self.frame_buffer = vec![0; frame_buffer_size];
            
            eprintln!("[Video] Loading: {}", path);
        } else {
            self.player_state = PlayerState::Error("No video file specified".to_string());
        }
    }
    
    fn resize(&mut self, new_bounds: Rect) {
        self.bounds = new_bounds;
        eprintln!("[Video] Player resized to {}×{}", new_bounds.width as i32, new_bounds.height as i32);
    }
    
    fn destroy(&mut self) {
        eprintln!("[Video] Video player destroyed");
        // In a real implementation: cleanup media decoder, close files, etc.
    }
}

impl Default for VideoViewProvider {
    fn default() -> Self {
        Self::new()
    }
}

// Helper functions

fn detect_video_format(path: &str) -> VideoFormat {
    if path.ends_with(".mp4") || path.ends_with(".m4v") {
        VideoFormat::MP4
    } else if path.ends_with(".mkv") {
        VideoFormat::MKV
    } else if path.ends_with(".avi") {
        VideoFormat::AVI
    } else if path.ends_with(".mov") {
        VideoFormat::MOV
    } else if path.ends_with(".webm") {
        VideoFormat::WebM
    } else if path.ends_with(".wmv") {
        VideoFormat::WMV
    } else if path.ends_with(".flv") {
        VideoFormat::FLV
    } else if path.starts_with("http") {
        VideoFormat::Stream
    } else {
        VideoFormat::Unknown
    }
}

fn generate_mock_video_metadata(path: &str) -> (String, (u32, u32), f32) {
    let filename = path.split('/').last().unwrap_or("video");
    let title = filename.split('.').next().unwrap_or("video").to_string();
    
    // Mock metadata based on file extension or URL
    let (resolution, duration) = if path.contains("4k") || path.contains("2160p") {
        ((3840, 2160), 180.0)
    } else if path.contains("1080p") || path.contains("fullhd") {
        ((1920, 1080), 120.0)
    } else if path.contains("720p") || path.contains("hd") {
        ((1280, 720), 90.0)
    } else if path.contains("youtube") || path.contains("vimeo") {
        ((1280, 720), 300.0)
    } else {
        ((854, 480), 60.0)
    };
    
    (title, resolution, duration)
}
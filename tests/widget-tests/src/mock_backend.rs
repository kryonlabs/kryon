//! Mock backend implementation for widget testing
//! 
//! Provides a simulated rendering backend that captures commands and events
//! for verification in tests without requiring actual graphics rendering.

use anyhow::Result;
use glam::{Vec2, Vec4};
use kryon_render::RenderCommand;
use std::collections::VecDeque;
use std::sync::{Arc, Mutex};

/// Mock rendering backend for testing
#[derive(Clone)]
pub struct MockRenderBackend {
    commands: Arc<Mutex<Vec<RenderCommand>>>,
    events: Arc<Mutex<VecDeque<MockEvent>>>,
    canvas_size: Vec2,
    frame_count: usize,
}

impl MockRenderBackend {
    /// Create a new mock backend
    pub fn new(width: f32, height: f32) -> Self {
        Self {
            commands: Arc::new(Mutex::new(Vec::new())),
            events: Arc::new(Mutex::new(VecDeque::new())),
            canvas_size: Vec2::new(width, height),
            frame_count: 0,
        }
    }

    /// Submit render commands
    pub fn submit_commands(&mut self, commands: Vec<RenderCommand>) {
        let mut stored_commands = self.commands.lock().unwrap();
        stored_commands.extend(commands);
        self.frame_count += 1;
    }

    /// Get all submitted commands
    pub fn get_commands(&self) -> Vec<RenderCommand> {
        self.commands.lock().unwrap().clone()
    }

    /// Clear all commands
    pub fn clear_commands(&mut self) {
        self.commands.lock().unwrap().clear();
    }

    /// Get commands for a specific frame
    pub fn get_frame_commands(&self, frame: usize) -> Vec<RenderCommand> {
        // In a real implementation, this would track commands per frame
        self.get_commands()
    }

    /// Add a mock event to the queue
    pub fn push_event(&mut self, event: MockEvent) {
        self.events.lock().unwrap().push_back(event);
    }

    /// Pop next event from queue
    pub fn pop_event(&mut self) -> Option<MockEvent> {
        self.events.lock().unwrap().pop_front()
    }

    /// Get canvas size
    pub fn canvas_size(&self) -> Vec2 {
        self.canvas_size
    }

    /// Set canvas size
    pub fn set_canvas_size(&mut self, size: Vec2) {
        self.canvas_size = size;
    }

    /// Get frame count
    pub fn frame_count(&self) -> usize {
        self.frame_count
    }

    /// Simulate a frame render
    pub fn render_frame(&mut self) -> MockFrameResult {
        let commands = self.get_commands();
        let result = MockFrameResult {
            frame_number: self.frame_count,
            command_count: commands.len(),
            draw_calls: self.count_draw_calls(&commands),
            state_changes: self.count_state_changes(&commands),
            total_pixels: self.calculate_pixel_coverage(&commands),
        };
        
        self.clear_commands();
        result
    }

    fn count_draw_calls(&self, commands: &[RenderCommand]) -> usize {
        commands.iter().filter(|cmd| {
            matches!(cmd,
                RenderCommand::DrawRect { .. } |
                RenderCommand::DrawText { .. } |
                RenderCommand::DrawLine { .. } |
                RenderCommand::DrawTriangle { .. } |
                RenderCommand::DrawCircle { .. } |
                RenderCommand::DrawImage { .. }
            )
        }).count()
    }

    fn count_state_changes(&self, commands: &[RenderCommand]) -> usize {
        commands.iter().filter(|cmd| {
            matches!(cmd,
                RenderCommand::SetClip { .. } |
                RenderCommand::ClearClip |
                RenderCommand::SetCanvasSize(_)
            )
        }).count()
    }

    fn calculate_pixel_coverage(&self, commands: &[RenderCommand]) -> f32 {
        commands.iter().map(|cmd| {
            match cmd {
                RenderCommand::DrawRect { size, .. } => size.x * size.y,
                RenderCommand::DrawCircle { radius, .. } => std::f32::consts::PI * radius * radius,
                _ => 0.0,
            }
        }).sum()
    }
}

/// Mock event for testing
#[derive(Debug, Clone)]
pub enum MockEvent {
    MouseMove { x: f32, y: f32 },
    MouseDown { x: f32, y: f32, button: u8 },
    MouseUp { x: f32, y: f32, button: u8 },
    KeyDown { key: String, modifiers: u8 },
    KeyUp { key: String, modifiers: u8 },
    TextInput { text: String },
    Resize { width: f32, height: f32 },
    Custom(String),
}

/// Result of rendering a frame
#[derive(Debug, Clone)]
pub struct MockFrameResult {
    pub frame_number: usize,
    pub command_count: usize,
    pub draw_calls: usize,
    pub state_changes: usize,
    pub total_pixels: f32,
}

/// Mock text measurement for testing
pub struct MockTextMeasurer;

impl MockTextMeasurer {
    /// Measure text dimensions
    pub fn measure_text(text: &str, font_size: f32) -> Vec2 {
        // Simple approximation for testing
        let char_width = font_size * 0.6;
        let char_height = font_size * 1.2;
        Vec2::new(
            text.len() as f32 * char_width,
            char_height
        )
    }

    /// Measure wrapped text
    pub fn measure_wrapped_text(text: &str, font_size: f32, max_width: f32) -> Vec2 {
        let full_size = Self::measure_text(text, font_size);
        if full_size.x <= max_width {
            full_size
        } else {
            let lines = (full_size.x / max_width).ceil();
            Vec2::new(max_width, full_size.y * lines)
        }
    }
}

/// Mock asset loader for testing
pub struct MockAssetLoader {
    loaded_assets: Arc<Mutex<Vec<String>>>,
}

impl MockAssetLoader {
    pub fn new() -> Self {
        Self {
            loaded_assets: Arc::new(Mutex::new(Vec::new())),
        }
    }

    /// Simulate loading an image
    pub fn load_image(&self, path: &str) -> Result<MockImage> {
        self.loaded_assets.lock().unwrap().push(path.to_string());
        Ok(MockImage {
            path: path.to_string(),
            width: 256,
            height: 256,
        })
    }

    /// Simulate loading a font
    pub fn load_font(&self, path: &str) -> Result<MockFont> {
        self.loaded_assets.lock().unwrap().push(path.to_string());
        Ok(MockFont {
            path: path.to_string(),
            family: "MockFont".to_string(),
        })
    }

    /// Get list of loaded assets
    pub fn get_loaded_assets(&self) -> Vec<String> {
        self.loaded_assets.lock().unwrap().clone()
    }
}

#[derive(Debug, Clone)]
pub struct MockImage {
    pub path: String,
    pub width: u32,
    pub height: u32,
}

#[derive(Debug, Clone)]
pub struct MockFont {
    pub path: String,
    pub family: String,
}

/// Mock animation system for testing
pub struct MockAnimationSystem {
    time: f32,
    animations: Vec<MockAnimation>,
}

impl MockAnimationSystem {
    pub fn new() -> Self {
        Self {
            time: 0.0,
            animations: Vec::new(),
        }
    }

    /// Advance time
    pub fn tick(&mut self, delta: f32) {
        self.time += delta;
        
        // Update animations
        for anim in &mut self.animations {
            anim.current_time = (anim.current_time + delta).min(anim.duration);
        }
        
        // Remove completed animations
        self.animations.retain(|a| a.current_time < a.duration);
    }

    /// Add an animation
    pub fn add_animation(&mut self, animation: MockAnimation) {
        self.animations.push(animation);
    }

    /// Get current animations
    pub fn active_animations(&self) -> &[MockAnimation] {
        &self.animations
    }

    /// Get current time
    pub fn current_time(&self) -> f32 {
        self.time
    }
}

#[derive(Debug, Clone)]
pub struct MockAnimation {
    pub id: String,
    pub duration: f32,
    pub current_time: f32,
    pub property: String,
    pub from: f32,
    pub to: f32,
}

/// Mock performance profiler
pub struct MockPerformanceProfiler {
    measurements: Vec<PerformanceMeasurement>,
}

impl MockPerformanceProfiler {
    pub fn new() -> Self {
        Self {
            measurements: Vec::new(),
        }
    }

    /// Record a performance measurement
    pub fn record(&mut self, name: &str, duration_ms: f32) {
        self.measurements.push(PerformanceMeasurement {
            name: name.to_string(),
            duration_ms,
            timestamp: std::time::SystemTime::now(),
        });
    }

    /// Get average duration for a measurement
    pub fn average(&self, name: &str) -> Option<f32> {
        let measurements: Vec<_> = self.measurements
            .iter()
            .filter(|m| m.name == name)
            .collect();
        
        if measurements.is_empty() {
            None
        } else {
            let sum: f32 = measurements.iter().map(|m| m.duration_ms).sum();
            Some(sum / measurements.len() as f32)
        }
    }

    /// Get all measurements
    pub fn get_measurements(&self) -> &[PerformanceMeasurement] {
        &self.measurements
    }

    /// Clear measurements
    pub fn clear(&mut self) {
        self.measurements.clear();
    }
}

#[derive(Debug, Clone)]
pub struct PerformanceMeasurement {
    pub name: String,
    pub duration_ms: f32,
    pub timestamp: std::time::SystemTime,
}

/// Mock event recorder for interaction testing
pub struct MockEventRecorder {
    events: Vec<RecordedEvent>,
    recording: bool,
}

impl MockEventRecorder {
    pub fn new() -> Self {
        Self {
            events: Vec::new(),
            recording: false,
        }
    }

    /// Start recording events
    pub fn start_recording(&mut self) {
        self.recording = true;
        self.events.clear();
    }

    /// Stop recording
    pub fn stop_recording(&mut self) {
        self.recording = false;
    }

    /// Record an event
    pub fn record(&mut self, event: MockEvent) {
        if self.recording {
            self.events.push(RecordedEvent {
                event,
                timestamp: std::time::SystemTime::now(),
            });
        }
    }

    /// Play back recorded events
    pub fn playback(&self) -> impl Iterator<Item = &RecordedEvent> {
        self.events.iter()
    }

    /// Get recorded event count
    pub fn event_count(&self) -> usize {
        self.events.len()
    }

    /// Save recording to string (for snapshot testing)
    pub fn to_string(&self) -> String {
        self.events
            .iter()
            .map(|e| format!("{:?}", e.event))
            .collect::<Vec<_>>()
            .join("\n")
    }
}

#[derive(Debug, Clone)]
pub struct RecordedEvent {
    pub event: MockEvent,
    pub timestamp: std::time::SystemTime,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mock_backend_command_tracking() {
        let mut backend = MockRenderBackend::new(800.0, 600.0);
        
        let commands = vec![
            RenderCommand::DrawRect {
                position: Vec2::new(0.0, 0.0),
                size: Vec2::new(100.0, 100.0),
                color: Vec4::new(1.0, 0.0, 0.0, 1.0),
                border_radius: 0.0,
                border_width: 0.0,
                border_color: Vec4::ZERO,
                transform: None,
                shadow: None,
                layout_style: None,
                z_index: 0,
            },
        ];
        
        backend.submit_commands(commands);
        
        assert_eq!(backend.get_commands().len(), 1);
        assert_eq!(backend.frame_count(), 1);
        
        let frame_result = backend.render_frame();
        assert_eq!(frame_result.draw_calls, 1);
        assert_eq!(frame_result.total_pixels, 10000.0);
    }

    #[test]
    fn test_mock_text_measurer() {
        let size = MockTextMeasurer::measure_text("Hello", 16.0);
        assert_eq!(size.x, 5.0 * 16.0 * 0.6);
        assert_eq!(size.y, 16.0 * 1.2);
        
        let wrapped = MockTextMeasurer::measure_wrapped_text("Very long text", 16.0, 50.0);
        assert!(wrapped.y > size.y);
    }

    #[test]
    fn test_mock_animation_system() {
        let mut anim_system = MockAnimationSystem::new();
        
        anim_system.add_animation(MockAnimation {
            id: "test".to_string(),
            duration: 1.0,
            current_time: 0.0,
            property: "opacity".to_string(),
            from: 0.0,
            to: 1.0,
        });
        
        assert_eq!(anim_system.active_animations().len(), 1);
        
        anim_system.tick(0.5);
        assert_eq!(anim_system.active_animations()[0].current_time, 0.5);
        
        anim_system.tick(0.6);
        assert_eq!(anim_system.active_animations().len(), 0);
    }

    #[test]
    fn test_mock_performance_profiler() {
        let mut profiler = MockPerformanceProfiler::new();
        
        profiler.record("render", 16.5);
        profiler.record("render", 17.5);
        profiler.record("update", 5.0);
        
        assert_eq!(profiler.average("render"), Some(17.0));
        assert_eq!(profiler.average("update"), Some(5.0));
        assert_eq!(profiler.average("unknown"), None);
    }
}
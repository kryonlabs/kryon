//! Test utilities for widget testing

use anyhow::Result;
use glam::{Vec2, Vec4};
use kryon_render::RenderCommand;
use std::collections::HashMap;
use std::time::{Duration, Instant};

/// Performance measurement utilities
pub struct PerformanceProfiler {
    start_time: Instant,
    measurements: HashMap<String, Duration>,
}

impl PerformanceProfiler {
    pub fn new() -> Self {
        Self {
            start_time: Instant::now(),
            measurements: HashMap::new(),
        }
    }

    /// Start measuring a named operation
    pub fn start_measurement(&mut self, name: &str) {
        self.measurements.insert(name.to_string(), Instant::now().duration_since(self.start_time));
    }

    /// End measurement and return duration
    pub fn end_measurement(&mut self, name: &str) -> Option<Duration> {
        let start = self.measurements.get(name)?;
        let end = Instant::now().duration_since(self.start_time);
        Some(end - *start)
    }

    /// Get all measurements
    pub fn get_measurements(&self) -> &HashMap<String, Duration> {
        &self.measurements
    }
}

/// Color comparison utilities
pub struct ColorMatcher;

impl ColorMatcher {
    /// Check if two colors are approximately equal within tolerance
    pub fn colors_match(a: Vec4, b: Vec4, tolerance: f32) -> bool {
        (a.x - b.x).abs() <= tolerance
            && (a.y - b.y).abs() <= tolerance
            && (a.z - b.z).abs() <= tolerance
            && (a.w - b.w).abs() <= tolerance
    }

    /// Convert hex color to Vec4
    pub fn hex_to_vec4(hex: &str) -> Result<Vec4> {
        let hex = hex.trim_start_matches('#');
        if hex.len() != 6 && hex.len() != 8 {
            return Err(anyhow::anyhow!("Invalid hex color format"));
        }

        let r = u8::from_str_radix(&hex[0..2], 16)? as f32 / 255.0;
        let g = u8::from_str_radix(&hex[2..4], 16)? as f32 / 255.0;
        let b = u8::from_str_radix(&hex[4..6], 16)? as f32 / 255.0;
        let a = if hex.len() == 8 {
            u8::from_str_radix(&hex[6..8], 16)? as f32 / 255.0
        } else {
            1.0
        };

        Ok(Vec4::new(r, g, b, a))
    }
}

/// Render command analysis utilities
pub struct RenderCommandAnalyzer;

impl RenderCommandAnalyzer {
    /// Count render commands by type
    pub fn count_by_type(commands: &[RenderCommand]) -> HashMap<String, usize> {
        let mut counts = HashMap::new();
        
        for command in commands {
            let type_name = match command {
                RenderCommand::DrawRect { .. } => "DrawRect",
                RenderCommand::DrawText { .. } => "DrawText",
                RenderCommand::DrawLine { .. } => "DrawLine",
                RenderCommand::DrawTriangle { .. } => "DrawTriangle",
                RenderCommand::DrawCircle { .. } => "DrawCircle",
                RenderCommand::DrawTextInput { .. } => "DrawTextInput",
                RenderCommand::SetClip { .. } => "SetClip",
                RenderCommand::ClearClip => "ClearClip",
                _ => "Other",
            };
            
            *counts.entry(type_name.to_string()).or_insert(0) += 1;
        }
        
        counts
    }

    /// Find all DrawRect commands
    pub fn find_rect_commands(commands: &[RenderCommand]) -> Vec<&RenderCommand> {
        commands
            .iter()
            .filter(|cmd| matches!(cmd, RenderCommand::DrawRect { .. }))
            .collect()
    }

    /// Find all DrawText commands
    pub fn find_text_commands(commands: &[RenderCommand]) -> Vec<&RenderCommand> {
        commands
            .iter()
            .filter(|cmd| matches!(cmd, RenderCommand::DrawText { .. }))
            .collect()
    }

    /// Calculate total area covered by rect commands
    pub fn calculate_total_rect_area(commands: &[RenderCommand]) -> f32 {
        commands
            .iter()
            .filter_map(|cmd| {
                if let RenderCommand::DrawRect { size, .. } = cmd {
                    Some(size.x * size.y)
                } else {
                    None
                }
            })
            .sum()
    }

    /// Check if commands are properly ordered by z-index
    pub fn verify_z_index_ordering(commands: &[RenderCommand]) -> bool {
        let mut last_z_index = i32::MIN;
        
        for command in commands {
            let z_index = match command {
                RenderCommand::DrawRect { z_index, .. } => *z_index,
                RenderCommand::DrawText { z_index, .. } => *z_index,
                RenderCommand::DrawSvg { z_index, .. } => *z_index,
                _ => continue,
            };
            
            if z_index < last_z_index {
                return false;
            }
            last_z_index = z_index;
        }
        
        true
    }
}

/// Layout testing utilities
pub struct LayoutTestUtils;

impl LayoutTestUtils {
    /// Check if a point is within a rectangle
    pub fn point_in_rect(point: Vec2, rect_pos: Vec2, rect_size: Vec2) -> bool {
        point.x >= rect_pos.x
            && point.x <= rect_pos.x + rect_size.x
            && point.y >= rect_pos.y
            && point.y <= rect_pos.y + rect_size.y
    }

    /// Check if two rectangles overlap
    pub fn rects_overlap(pos1: Vec2, size1: Vec2, pos2: Vec2, size2: Vec2) -> bool {
        !(pos1.x + size1.x < pos2.x
            || pos2.x + size2.x < pos1.x
            || pos1.y + size1.y < pos2.y
            || pos2.y + size2.y < pos1.y)
    }

    /// Calculate the distance between two points
    pub fn distance(a: Vec2, b: Vec2) -> f32 {
        ((a.x - b.x).powi(2) + (a.y - b.y).powi(2)).sqrt()
    }

    /// Check if a size is within reasonable bounds
    pub fn size_is_reasonable(size: Vec2, max_size: Vec2) -> bool {
        size.x >= 0.0 && size.y >= 0.0 && size.x <= max_size.x && size.y <= max_size.y
    }
}

/// Event simulation utilities
pub struct EventSimulator;

impl EventSimulator {
    /// Simulate a click and drag operation
    pub fn click_and_drag(start: Vec2, end: Vec2, steps: usize) -> Vec<crate::InputEvent> {
        let mut events = Vec::new();
        
        // Mouse down at start
        events.push(crate::InputEvent::MouseDown {
            position: start,
            button: 0,
        });
        
        // Intermediate drag events
        for i in 1..steps {
            let t = i as f32 / steps as f32;
            let pos = start + (end - start) * t;
            events.push(crate::InputEvent::MouseMove { position: pos });
        }
        
        // Mouse up at end
        events.push(crate::InputEvent::MouseUp {
            position: end,
            button: 0,
        });
        
        events
    }

    /// Simulate typing text
    pub fn type_text(text: &str) -> Vec<crate::InputEvent> {
        text.chars()
            .map(|c| crate::InputEvent::TextInput {
                text: c.to_string(),
            })
            .collect()
    }

    /// Simulate key sequence
    pub fn key_sequence(keys: &[&str]) -> Vec<crate::InputEvent> {
        keys.iter()
            .flat_map(|&key| {
                vec![
                    crate::InputEvent::KeyDown {
                        key: key.to_string(),
                        modifiers: 0,
                    },
                    crate::InputEvent::KeyUp {
                        key: key.to_string(),
                        modifiers: 0,
                    },
                ]
            })
            .collect()
    }
}

/// Snapshot testing utilities
pub struct SnapshotTester;

impl SnapshotTester {
    /// Convert render commands to a comparable string representation
    pub fn commands_to_snapshot(commands: &[RenderCommand]) -> String {
        commands
            .iter()
            .map(|cmd| Self::command_to_string(cmd))
            .collect::<Vec<_>>()
            .join("\n")
    }

    fn command_to_string(command: &RenderCommand) -> String {
        match command {
            RenderCommand::DrawRect {
                position,
                size,
                color,
                border_radius,
                z_index,
                ..
            } => {
                format!(
                    "DrawRect(pos: {:.1},{:.1}, size: {:.1},{:.1}, color: {:.2},{:.2},{:.2},{:.2}, radius: {:.1}, z: {})",
                    position.x, position.y,
                    size.x, size.y,
                    color.x, color.y, color.z, color.w,
                    border_radius, z_index
                )
            }
            RenderCommand::DrawText {
                position,
                text,
                font_size,
                color,
                z_index,
                ..
            } => {
                format!(
                    "DrawText(pos: {:.1},{:.1}, text: \"{}\", size: {:.1}, color: {:.2},{:.2},{:.2},{:.2}, z: {})",
                    position.x, position.y,
                    text,
                    font_size,
                    color.x, color.y, color.z, color.w,
                    z_index
                )
            }
            RenderCommand::DrawLine {
                start,
                end,
                color,
                width,
            } => {
                format!(
                    "DrawLine(start: {:.1},{:.1}, end: {:.1},{:.1}, color: {:.2},{:.2},{:.2},{:.2}, width: {:.1})",
                    start.x, start.y,
                    end.x, end.y,
                    color.x, color.y, color.z, color.w,
                    width
                )
            }
            RenderCommand::DrawTriangle { points, color } => {
                format!(
                    "DrawTriangle(points: [{:.1},{:.1}, {:.1},{:.1}, {:.1},{:.1}], color: {:.2},{:.2},{:.2},{:.2})",
                    points[0].x, points[0].y,
                    points[1].x, points[1].y,
                    points[2].x, points[2].y,
                    color.x, color.y, color.z, color.w
                )
            }
            _ => format!("{:?}", command),
        }
    }

    /// Save snapshot to file
    pub fn save_snapshot(name: &str, content: &str) -> Result<()> {
        use std::fs;
        use std::path::Path;

        let snapshot_dir = Path::new("tests/snapshots");
        fs::create_dir_all(snapshot_dir)?;

        let file_path = snapshot_dir.join(format!("{}.snap", name));
        fs::write(file_path, content)?;

        Ok(())
    }

    /// Load snapshot from file
    pub fn load_snapshot(name: &str) -> Result<String> {
        use std::fs;
        use std::path::Path;

        let file_path = Path::new("tests/snapshots").join(format!("{}.snap", name));
        Ok(fs::read_to_string(file_path)?)
    }

    /// Compare current output with saved snapshot
    pub fn compare_with_snapshot(name: &str, current: &str) -> Result<bool> {
        match Self::load_snapshot(name) {
            Ok(saved) => Ok(saved.trim() == current.trim()),
            Err(_) => {
                // If snapshot doesn't exist, save current as baseline
                Self::save_snapshot(name, current)?;
                Ok(true)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_color_matcher() {
        let color1 = Vec4::new(1.0, 0.5, 0.0, 1.0);
        let color2 = Vec4::new(1.01, 0.49, 0.01, 1.0);
        
        assert!(ColorMatcher::colors_match(color1, color2, 0.02));
        assert!(!ColorMatcher::colors_match(color1, color2, 0.005));
    }

    #[test]
    fn test_hex_to_vec4() -> Result<()> {
        let color = ColorMatcher::hex_to_vec4("#FF8000")?;
        assert!(ColorMatcher::colors_match(
            color,
            Vec4::new(1.0, 0.502, 0.0, 1.0),
            0.01
        ));
        Ok(())
    }

    #[test]
    fn test_layout_utils() {
        assert!(LayoutTestUtils::point_in_rect(
            Vec2::new(5.0, 5.0),
            Vec2::new(0.0, 0.0),
            Vec2::new(10.0, 10.0)
        ));

        assert!(!LayoutTestUtils::point_in_rect(
            Vec2::new(15.0, 5.0),
            Vec2::new(0.0, 0.0),
            Vec2::new(10.0, 10.0)
        ));

        assert!(LayoutTestUtils::rects_overlap(
            Vec2::new(0.0, 0.0),
            Vec2::new(10.0, 10.0),
            Vec2::new(5.0, 5.0),
            Vec2::new(10.0, 10.0)
        ));
    }

    #[test]
    fn test_event_simulator() {
        let events = EventSimulator::type_text("hello");
        assert_eq!(events.len(), 5);

        let drag_events = EventSimulator::click_and_drag(
            Vec2::new(0.0, 0.0),
            Vec2::new(10.0, 10.0),
            5
        );
        assert_eq!(drag_events.len(), 6); // down + 4 moves + up
    }
}
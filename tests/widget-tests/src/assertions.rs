//! Custom assertions for widget testing
//! 
//! Provides specialized assertion macros and functions for verifying
//! widget behavior, state changes, and rendering output.

use anyhow::{bail, Result};
use glam::{Vec2, Vec4};
use kryon_render::RenderCommand;
use std::fmt::Debug;

/// Assert that a widget state matches expected conditions
#[macro_export]
macro_rules! assert_widget_state {
    ($widget:expr, $field:ident == $expected:expr) => {
        assert_eq!(
            $widget.state().$field,
            $expected,
            "Widget state field '{}' does not match expected value.\nActual: {:?}\nExpected: {:?}",
            stringify!($field),
            $widget.state().$field,
            $expected
        );
    };
    ($widget:expr, $field:ident != $expected:expr) => {
        assert_ne!(
            $widget.state().$field,
            $expected,
            "Widget state field '{}' should not equal the given value.\nValue: {:?}",
            stringify!($field),
            $expected
        );
    };
}

/// Assert that render commands contain specific elements
#[macro_export]
macro_rules! assert_render_commands {
    ($commands:expr, contains DrawRect) => {
        assert!(
            $commands.iter().any(|cmd| matches!(cmd, RenderCommand::DrawRect { .. })),
            "Expected render commands to contain DrawRect"
        );
    };
    ($commands:expr, contains DrawText with $text:expr) => {
        assert!(
            $commands.iter().any(|cmd| {
                if let RenderCommand::DrawText { text, .. } = cmd {
                    text == $text
                } else {
                    false
                }
            }),
            "Expected render commands to contain DrawText with text: {}",
            $text
        );
    };
    ($commands:expr, count == $expected:expr) => {
        assert_eq!(
            $commands.len(),
            $expected,
            "Expected {} render commands, got {}",
            $expected,
            $commands.len()
        );
    };
}

/// Assert that an event was handled correctly
#[macro_export]
macro_rules! assert_event_handled {
    ($result:expr) => {
        match $result {
            Ok(EventResult::Handled) | Ok(EventResult::StateChanged) => {},
            Ok(EventResult::NotHandled) => panic!("Event was not handled"),
            Ok(other) => panic!("Unexpected event result: {:?}", other),
            Err(e) => panic!("Event handling failed: {}", e),
        }
    };
    ($result:expr, StateChanged) => {
        match $result {
            Ok(EventResult::StateChanged) => {},
            Ok(other) => panic!("Expected StateChanged, got {:?}", other),
            Err(e) => panic!("Event handling failed: {}", e),
        }
    };
}

/// Assertion helpers for render commands
pub struct RenderCommandAssertions;

impl RenderCommandAssertions {
    /// Assert that a rectangle is rendered at the expected position
    pub fn assert_rect_at_position(
        commands: &[RenderCommand],
        position: Vec2,
        tolerance: f32,
    ) -> Result<()> {
        for cmd in commands {
            if let RenderCommand::DrawRect { position: pos, .. } = cmd {
                if (pos.x - position.x).abs() <= tolerance
                    && (pos.y - position.y).abs() <= tolerance
                {
                    return Ok(());
                }
            }
        }
        bail!("No DrawRect command found at position {:?} (tolerance: {})", position, tolerance)
    }

    /// Assert that text is rendered with specific properties
    pub fn assert_text_rendered(
        commands: &[RenderCommand],
        expected_text: &str,
        font_size: Option<f32>,
        color: Option<Vec4>,
    ) -> Result<()> {
        for cmd in commands {
            if let RenderCommand::DrawText { text, font_size: fs, color: c, .. } = cmd {
                if text == expected_text {
                    if let Some(expected_size) = font_size {
                        if (*fs - expected_size).abs() > 0.1 {
                            bail!("Text '{}' has wrong font size. Expected: {}, Got: {}", 
                                text, expected_size, fs);
                        }
                    }
                    if let Some(expected_color) = color {
                        if !colors_equal(*c, expected_color, 0.01) {
                            bail!("Text '{}' has wrong color. Expected: {:?}, Got: {:?}", 
                                text, expected_color, c);
                        }
                    }
                    return Ok(());
                }
            }
        }
        bail!("Text '{}' not found in render commands", expected_text)
    }

    /// Assert z-index ordering
    pub fn assert_z_index_order(commands: &[RenderCommand]) -> Result<()> {
        let mut last_z = i32::MIN;
        
        for cmd in commands {
            let z_index = match cmd {
                RenderCommand::DrawRect { z_index, .. } => *z_index,
                RenderCommand::DrawText { z_index, .. } => *z_index,
                RenderCommand::DrawSvg { z_index, .. } => *z_index,
                RenderCommand::DrawScrollbar { z_index, .. } => *z_index,
                _ => continue,
            };
            
            if z_index < last_z {
                bail!("Z-index ordering violation: {} comes after {}", z_index, last_z);
            }
            last_z = z_index;
        }
        
        Ok(())
    }

    /// Assert clipping is properly set
    pub fn assert_clipping_active(commands: &[RenderCommand]) -> Result<()> {
        let mut clip_active = false;
        
        for cmd in commands {
            match cmd {
                RenderCommand::SetClip { .. } => clip_active = true,
                RenderCommand::ClearClip => clip_active = false,
                _ => {
                    if !clip_active && Self::is_draw_command(cmd) {
                        bail!("Draw command found without active clipping");
                    }
                }
            }
        }
        
        Ok(())
    }

    fn is_draw_command(cmd: &RenderCommand) -> bool {
        matches!(cmd,
            RenderCommand::DrawRect { .. } |
            RenderCommand::DrawText { .. } |
            RenderCommand::DrawLine { .. } |
            RenderCommand::DrawCircle { .. } |
            RenderCommand::DrawImage { .. }
        )
    }
}

/// Assertions for widget state
pub struct StateAssertions;

impl StateAssertions {
    /// Assert that a value is within range
    pub fn assert_in_range<T: PartialOrd + Debug>(
        value: T,
        min: T,
        max: T,
        name: &str,
    ) -> Result<()> {
        if value < min || value > max {
            bail!("{} out of range. Value: {:?}, Expected range: [{:?}, {:?}]", 
                name, value, min, max);
        }
        Ok(())
    }

    /// Assert that a collection has expected size
    pub fn assert_collection_size<T>(
        collection: &[T],
        expected: usize,
        name: &str,
    ) -> Result<()> {
        if collection.len() != expected {
            bail!("{} has wrong size. Expected: {}, Got: {}", 
                name, expected, collection.len());
        }
        Ok(())
    }

    /// Assert that an option is Some with a specific value
    pub fn assert_some_eq<T: PartialEq + Debug>(
        option: &Option<T>,
        expected: &T,
        name: &str,
    ) -> Result<()> {
        match option {
            Some(value) if value == expected => Ok(()),
            Some(value) => bail!("{} has wrong value. Expected: {:?}, Got: {:?}", 
                name, expected, value),
            None => bail!("{} is None, expected Some({:?})", name, expected),
        }
    }
}

/// Assertions for layout properties
pub struct LayoutAssertions;

impl LayoutAssertions {
    /// Assert that an element is within bounds
    pub fn assert_within_bounds(
        position: Vec2,
        size: Vec2,
        bounds: Vec2,
    ) -> Result<()> {
        if position.x < 0.0 || position.y < 0.0 {
            bail!("Element position {:?} is negative", position);
        }
        
        if position.x + size.x > bounds.x || position.y + size.y > bounds.y {
            bail!("Element exceeds bounds. Position: {:?}, Size: {:?}, Bounds: {:?}", 
                position, size, bounds);
        }
        
        Ok(())
    }

    /// Assert that elements don't overlap
    pub fn assert_no_overlap(
        elements: &[(Vec2, Vec2)], // (position, size) pairs
    ) -> Result<()> {
        for i in 0..elements.len() {
            for j in (i + 1)..elements.len() {
                let (pos1, size1) = elements[i];
                let (pos2, size2) = elements[j];
                
                if rects_overlap(pos1, size1, pos2, size2) {
                    bail!("Elements {} and {} overlap", i, j);
                }
            }
        }
        Ok(())
    }

    /// Assert alignment
    pub fn assert_aligned(
        positions: &[Vec2],
        axis: Axis,
        tolerance: f32,
    ) -> Result<()> {
        if positions.is_empty() {
            return Ok(());
        }

        let first = positions[0];
        for (i, pos) in positions.iter().enumerate().skip(1) {
            let diff = match axis {
                Axis::Horizontal => (pos.x - first.x).abs(),
                Axis::Vertical => (pos.y - first.y).abs(),
            };
            
            if diff > tolerance {
                bail!("Element {} is not aligned on {:?} axis. Difference: {}", 
                    i, axis, diff);
            }
        }
        
        Ok(())
    }
}

#[derive(Debug, Clone, Copy)]
pub enum Axis {
    Horizontal,
    Vertical,
}

/// Performance assertions
pub struct PerformanceAssertions;

impl PerformanceAssertions {
    /// Assert that an operation completes within time limit
    pub fn assert_duration_under(
        duration: std::time::Duration,
        limit_ms: u64,
        operation: &str,
    ) -> Result<()> {
        let actual_ms = duration.as_millis() as u64;
        if actual_ms > limit_ms {
            bail!("{} took too long. Expected: <{}ms, Actual: {}ms", 
                operation, limit_ms, actual_ms);
        }
        Ok(())
    }

    /// Assert memory usage is within limits
    pub fn assert_memory_usage_under(
        bytes: usize,
        limit_mb: f64,
        context: &str,
    ) -> Result<()> {
        let actual_mb = bytes as f64 / (1024.0 * 1024.0);
        if actual_mb > limit_mb {
            bail!("{} uses too much memory. Expected: <{}MB, Actual: {:.2}MB", 
                context, limit_mb, actual_mb);
        }
        Ok(())
    }
}

// Helper functions

fn colors_equal(a: Vec4, b: Vec4, tolerance: f32) -> bool {
    (a.x - b.x).abs() <= tolerance
        && (a.y - b.y).abs() <= tolerance
        && (a.z - b.z).abs() <= tolerance
        && (a.w - b.w).abs() <= tolerance
}

fn rects_overlap(pos1: Vec2, size1: Vec2, pos2: Vec2, size2: Vec2) -> bool {
    !(pos1.x + size1.x < pos2.x
        || pos2.x + size2.x < pos1.x
        || pos1.y + size1.y < pos2.y
        || pos2.y + size2.y < pos1.y)
}

/// Fluent assertion builder
pub struct AssertionBuilder<T> {
    value: T,
    name: String,
}

impl<T> AssertionBuilder<T> {
    pub fn new(value: T, name: impl Into<String>) -> Self {
        Self {
            value,
            name: name.into(),
        }
    }
}

impl<T: PartialEq + Debug> AssertionBuilder<T> {
    pub fn equals(self, expected: T) -> Result<()> {
        if self.value != expected {
            bail!("{} does not equal expected value.\nActual: {:?}\nExpected: {:?}", 
                self.name, self.value, expected);
        }
        Ok(())
    }

    pub fn not_equals(self, unexpected: T) -> Result<()> {
        if self.value == unexpected {
            bail!("{} should not equal {:?}", self.name, unexpected);
        }
        Ok(())
    }
}

impl<T: PartialOrd + Debug> AssertionBuilder<T> {
    pub fn greater_than(self, threshold: T) -> Result<()> {
        if self.value <= threshold {
            bail!("{} should be greater than {:?}, but was {:?}", 
                self.name, threshold, self.value);
        }
        Ok(())
    }

    pub fn less_than(self, threshold: T) -> Result<()> {
        if self.value >= threshold {
            bail!("{} should be less than {:?}, but was {:?}", 
                self.name, threshold, self.value);
        }
        Ok(())
    }

    pub fn in_range(self, min: T, max: T) -> Result<()> {
        if self.value < min || self.value > max {
            bail!("{} out of range. Value: {:?}, Expected: [{:?}, {:?}]", 
                self.name, self.value, min, max);
        }
        Ok(())
    }
}

/// Create an assertion builder
pub fn assert_that<T>(value: T, name: impl Into<String>) -> AssertionBuilder<T> {
    AssertionBuilder::new(value, name)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_assertion_builder() {
        assert!(assert_that(5, "value").equals(5).is_ok());
        assert!(assert_that(5, "value").equals(6).is_err());
        
        assert!(assert_that(10, "number").greater_than(5).is_ok());
        assert!(assert_that(3, "number").greater_than(5).is_err());
        
        assert!(assert_that(5, "number").in_range(1, 10).is_ok());
        assert!(assert_that(15, "number").in_range(1, 10).is_err());
    }

    #[test]
    fn test_render_command_assertions() {
        let commands = vec![
            RenderCommand::DrawRect {
                position: Vec2::new(10.0, 10.0),
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
            RenderCommand::DrawText {
                position: Vec2::new(20.0, 20.0),
                text: "Hello".to_string(),
                font_size: 16.0,
                color: Vec4::new(1.0, 1.0, 1.0, 1.0),
                alignment: kryon_core::TextAlignment::Start,
                max_width: None,
                max_height: None,
                transform: None,
                font_family: None,
                z_index: 1,
            },
        ];

        assert!(RenderCommandAssertions::assert_rect_at_position(
            &commands,
            Vec2::new(10.0, 10.0),
            0.1
        ).is_ok());

        assert!(RenderCommandAssertions::assert_text_rendered(
            &commands,
            "Hello",
            Some(16.0),
            None
        ).is_ok());

        assert!(RenderCommandAssertions::assert_z_index_order(&commands).is_ok());
    }

    #[test]
    fn test_layout_assertions() {
        let elements = vec![
            (Vec2::new(0.0, 0.0), Vec2::new(100.0, 100.0)),
            (Vec2::new(110.0, 0.0), Vec2::new(100.0, 100.0)),
        ];

        assert!(LayoutAssertions::assert_no_overlap(&elements).is_ok());

        let overlapping = vec![
            (Vec2::new(0.0, 0.0), Vec2::new(100.0, 100.0)),
            (Vec2::new(50.0, 50.0), Vec2::new(100.0, 100.0)),
        ];

        assert!(LayoutAssertions::assert_no_overlap(&overlapping).is_err());
    }
}
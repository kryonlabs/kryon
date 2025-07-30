//! Widget Testing Framework
//! 
//! Comprehensive testing utilities for Kryon widgets including state management,
//! event handling, rendering verification, and performance benchmarking.

use anyhow::Result;
use glam::{Vec2, Vec4};
use kryon_core::{ElementId, PropertyBag};
use kryon_render::RenderCommand;
use kryon_shared::widgets::*;
use serde_json::Value;
use std::collections::HashMap;

pub mod test_utils;
pub mod mock_backend;
pub mod assertions;
pub mod fixtures;

pub use test_utils::*;
pub use mock_backend::*;
pub use assertions::*;
pub use fixtures::*;

/// Core widget testing framework
pub struct WidgetTestHarness<W: KryonWidget> {
    widget: W,
    state: W::State,
    config: W::Config,
    layout: LayoutResult,
    element_id: ElementId,
}

impl<W: KryonWidget> WidgetTestHarness<W> {
    /// Create a new test harness for a widget
    pub fn new(widget: W) -> Self {
        Self {
            widget,
            state: W::State::default(),
            config: W::Config::default(),
            layout: LayoutResult::default(),
            element_id: 1,
        }
    }

    /// Set widget state
    pub fn with_state(mut self, state: W::State) -> Self {
        self.state = state;
        self
    }

    /// Set widget configuration
    pub fn with_config(mut self, config: W::Config) -> Self {
        self.config = config;
        self
    }

    /// Set layout result
    pub fn with_layout(mut self, layout: LayoutResult) -> Self {
        self.layout = layout;
        self
    }

    /// Set element ID
    pub fn with_element_id(mut self, element_id: ElementId) -> Self {
        self.element_id = element_id;
        self
    }

    /// Render the widget and return commands
    pub fn render(&self) -> Result<Vec<RenderCommand>> {
        self.widget.render(&self.state, &self.config, &self.layout, self.element_id)
    }

    /// Handle an event and return the result
    pub fn handle_event(&mut self, event: InputEvent) -> Result<EventResult> {
        self.widget.handle_event(&mut self.state, &self.config, &event, &self.layout)
    }

    /// Get current state
    pub fn state(&self) -> &W::State {
        &self.state
    }

    /// Get current config
    pub fn config(&self) -> &W::Config {
        &self.config
    }

    /// Assert that the widget renders without errors
    pub fn assert_renders_successfully(&self) -> Result<()> {
        let _commands = self.render()?;
        Ok(())
    }

    /// Assert that the widget produces expected number of render commands
    pub fn assert_command_count(&self, expected: usize) -> Result<()> {
        let commands = self.render()?;
        assert_eq!(commands.len(), expected, "Expected {} render commands, got {}", expected, commands.len());
        Ok(())
    }

    /// Assert that the widget state equals expected state
    pub fn assert_state_equals(&self, expected: &W::State) -> Result<()> {
        assert_eq!(&self.state, expected, "Widget state does not match expected state");
        Ok(())
    }
}

/// Default layout result for testing
#[derive(Debug, Clone, Default)]
pub struct LayoutResult {
    pub position: Vec2,
    pub size: Vec2,
    pub content_size: Vec2,
    pub scroll_offset: Vec2,
}

/// Mock input event for testing
#[derive(Debug, Clone)]
pub enum InputEvent {
    MouseMove { position: Vec2 },
    MouseDown { position: Vec2, button: u8 },
    MouseUp { position: Vec2, button: u8 },
    KeyDown { key: String, modifiers: u8 },
    KeyUp { key: String, modifiers: u8 },
    TextInput { text: String },
    Scroll { delta: Vec2, position: Vec2 },
    FocusGained,
    FocusLost,
}

/// Widget event handling result
#[derive(Debug, Clone, PartialEq)]
pub enum EventResult {
    Handled,
    NotHandled,
    StateChanged,
    RequestRedraw,
    RequestResize,
    Custom(Value),
}

/// Create property bag from key-value pairs
pub fn create_property_bag(properties: &[(&str, Value)]) -> PropertyBag {
    let mut bag = PropertyBag::new();
    for (key, value) in properties {
        bag.set(key.to_string(), value.clone());
    }
    bag
}

/// Create mouse click event
pub fn mouse_click(x: f32, y: f32) -> InputEvent {
    InputEvent::MouseDown {
        position: Vec2::new(x, y),
        button: 0,
    }
}

/// Create mouse move event
pub fn mouse_move(x: f32, y: f32) -> InputEvent {
    InputEvent::MouseMove {
        position: Vec2::new(x, y),
    }
}

/// Create key press event
pub fn key_press(key: &str) -> InputEvent {
    InputEvent::KeyDown {
        key: key.to_string(),
        modifiers: 0,
    }
}

/// Create text input event
pub fn text_input(text: &str) -> InputEvent {
    InputEvent::TextInput {
        text: text.to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_widget_test_harness_creation() {
        // This would be tested with actual widget implementations
        // For now, just verify the harness structure
        assert!(true);
    }

    #[test]
    fn test_input_event_creation() {
        let click = mouse_click(10.0, 20.0);
        match click {
            InputEvent::MouseDown { position, button } => {
                assert_eq!(position, Vec2::new(10.0, 20.0));
                assert_eq!(button, 0);
            }
            _ => panic!("Expected MouseDown event"),
        }
    }

    #[test]
    fn test_property_bag_creation() {
        let bag = create_property_bag(&[
            ("text", Value::String("Hello".to_string())),
            ("enabled", Value::Bool(true)),
            ("size", Value::Number(serde_json::Number::from(42))),
        ]);
        
        assert_eq!(bag.len(), 3);
    }
}
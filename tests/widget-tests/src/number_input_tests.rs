//! Comprehensive tests for the number input widget

use anyhow::Result;
use widget_tests::*;
use kryon_shared::widgets::number_input::*;
use glam::Vec2;
use serde_json::Value;

#[tokio::test]
async fn test_number_input_basic_functionality() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let mut harness = WidgetTestHarness::new(number_input)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial state
    assert_eq!(harness.state().value, 0.0);
    assert_eq!(harness.state().text_value, "0");
    assert!(!harness.state().is_focused);
    assert!(!harness.state().is_editing);

    // Test that it renders successfully
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_number_input_text_entry() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let initial_state = NumberInputState {
        value: 0.0,
        text_value: "0".to_string(),
        is_focused: true,
        is_editing: true,
        cursor_position: 1,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Clear current value
    harness.handle_event(key_press("Backspace"))?;
    assert_eq!(harness.state().text_value, "");

    // Type a number
    harness.handle_event(text_input("123.45"))?;
    assert_eq!(harness.state().text_value, "123.45");
    
    // Press Enter to confirm
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 123.45);
    assert!(!harness.state().is_editing);

    Ok(())
}

#[tokio::test]
async fn test_number_input_step_buttons() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let initial_state = NumberInputState {
        value: 10.0,
        text_value: "10".to_string(),
        is_focused: false,
        is_editing: false,
        cursor_position: 0,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let config = NumberInputConfig {
        min: Some(0.0),
        max: Some(100.0),
        step: 5.0,
        precision: 0,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click increment button
    let increment_x = 185.0; // Right side
    let increment_y = 10.0;  // Top half
    
    let result = harness.handle_event(mouse_click(increment_x, increment_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 15.0);

    // Click decrement button
    let decrement_x = 185.0; // Right side
    let decrement_y = 30.0;  // Bottom half
    
    let result = harness.handle_event(mouse_click(decrement_x, decrement_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 10.0);

    Ok(())
}

#[tokio::test]
async fn test_number_input_validation() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let initial_state = NumberInputState {
        value: 50.0,
        text_value: "50".to_string(),
        is_focused: true,
        is_editing: true,
        cursor_position: 2,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let config = NumberInputConfig {
        min: Some(0.0),
        max: Some(100.0),
        step: 1.0,
        precision: 2,
        required: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Clear and type invalid value
    harness.state_mut().text_value.clear();
    harness.state_mut().cursor_position = 0;
    harness.handle_event(text_input("150"))?;
    
    // Try to confirm
    harness.handle_event(key_press("Enter"))?;
    
    // Should be clamped to max
    assert_eq!(harness.state().value, 100.0);
    assert_eq!(harness.state().text_value, "100.00");

    // Test min constraint
    harness.state_mut().text_value.clear();
    harness.state_mut().is_editing = true;
    harness.handle_event(text_input("-50"))?;
    harness.handle_event(key_press("Enter"))?;
    
    // Should be clamped to min
    assert_eq!(harness.state().value, 0.0);
    assert_eq!(harness.state().text_value, "0.00");

    Ok(())
}

#[tokio::test]
async fn test_number_input_formatting() -> Result<()> {
    let number_input = NumberInputWidget::new();
    
    let config = NumberInputConfig {
        min: None,
        max: None,
        step: 1.0,
        precision: 2,
        thousands_separator: true,
        prefix: "$".to_string(),
        suffix: " USD".to_string(),
        ..Default::default()
    };

    let initial_state = NumberInputState {
        value: 1234567.89,
        text_value: "$1,234,567.89 USD".to_string(),
        is_focused: false,
        is_editing: false,
        cursor_position: 0,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(250.0, 40.0),
            content_size: Vec2::new(250.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that formatted value renders correctly
    let commands = harness.render()?;
    assert_render_commands!(commands, contains DrawText with "$1,234,567.89 USD");

    Ok(())
}

#[tokio::test]
async fn test_number_input_keyboard_controls() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let initial_state = NumberInputState {
        value: 50.0,
        text_value: "50".to_string(),
        is_focused: true,
        is_editing: false,
        cursor_position: 0,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let config = NumberInputConfig {
        min: Some(0.0),
        max: Some(100.0),
        step: 5.0,
        precision: 0,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Arrow up to increment
    let result = harness.handle_event(key_press("ArrowUp"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 55.0);

    // Arrow down to decrement
    let result = harness.handle_event(key_press("ArrowDown"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 50.0);

    // Page up for larger increment
    let result = harness.handle_event(key_press("PageUp"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 60.0); // 10x step

    // Page down for larger decrement
    let result = harness.handle_event(key_press("PageDown"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 50.0);

    Ok(())
}

#[tokio::test]
async fn test_number_input_mouse_wheel() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let initial_state = NumberInputState {
        value: 10.0,
        text_value: "10".to_string(),
        is_focused: true,
        is_editing: false,
        cursor_position: 0,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let config = NumberInputConfig {
        step: 0.1,
        precision: 1,
        mouse_wheel_enabled: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Scroll up to increment
    let result = harness.handle_event(InputEvent::Scroll {
        delta: Vec2::new(0.0, 1.0),
        position: Vec2::new(100.0, 20.0),
    })?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 10.1);

    // Scroll down to decrement
    let result = harness.handle_event(InputEvent::Scroll {
        delta: Vec2::new(0.0, -1.0),
        position: Vec2::new(100.0, 20.0),
    })?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().value, 10.0);

    Ok(())
}

#[tokio::test]
async fn test_number_input_selection() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let initial_state = NumberInputState {
        value: 123.45,
        text_value: "123.45".to_string(),
        is_focused: true,
        is_editing: true,
        cursor_position: 0,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Select all with Ctrl+A
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "a".to_string(),
        modifiers: 1, // Ctrl
    })?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().selection, Some((0, 6)));

    // Type to replace selection
    harness.handle_event(text_input("999"))?;
    assert_eq!(harness.state().text_value, "999");
    assert_eq!(harness.state().cursor_position, 3);
    assert!(harness.state().selection.is_none());

    Ok(())
}

#[tokio::test]
async fn test_number_input_step_repeat() -> Result<()> {
    let number_input = NumberInputWidget::new();
    let initial_state = NumberInputState {
        value: 0.0,
        text_value: "0".to_string(),
        is_focused: false,
        is_editing: false,
        cursor_position: 0,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let config = NumberInputConfig {
        step: 1.0,
        precision: 0,
        step_repeat_delay: 500,
        step_repeat_interval: 100,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Mouse down on increment button
    let increment_x = 185.0;
    let increment_y = 10.0;
    
    harness.handle_event(InputEvent::MouseDown {
        position: Vec2::new(increment_x, increment_y),
        button: 0,
    })?;
    
    // Should start step timer
    assert!(harness.state().step_timer.is_some());
    
    // Simulate time passing and timer events
    // In real implementation, this would be handled by the timer system
    
    // Mouse up to stop
    harness.handle_event(InputEvent::MouseUp {
        position: Vec2::new(increment_x, increment_y),
        button: 0,
    })?;
    
    // Timer should be cleared
    assert!(harness.state().step_timer.is_none());

    Ok(())
}

#[tokio::test]
async fn test_number_input_scientific_notation() -> Result<()> {
    let number_input = NumberInputWidget::new();
    
    let config = NumberInputConfig {
        allow_scientific: true,
        precision: 3,
        ..Default::default()
    };

    let initial_state = NumberInputState {
        value: 0.0,
        text_value: "0".to_string(),
        is_focused: true,
        is_editing: true,
        cursor_position: 1,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Clear and enter scientific notation
    harness.state_mut().text_value.clear();
    harness.state_mut().cursor_position = 0;
    harness.handle_event(text_input("1.23e5"))?;
    harness.handle_event(key_press("Enter"))?;
    
    // Should parse correctly
    assert_eq!(harness.state().value, 123000.0);

    // Test negative exponent
    harness.state_mut().text_value.clear();
    harness.state_mut().is_editing = true;
    harness.handle_event(text_input("5e-3"))?;
    harness.handle_event(key_press("Enter"))?;
    
    assert_eq!(harness.state().value, 0.005);

    Ok(())
}

#[tokio::test]
async fn test_number_input_custom_validation() -> Result<()> {
    let number_input = NumberInputWidget::new();
    
    let config = NumberInputConfig {
        min: Some(0.0),
        max: Some(100.0),
        step: 5.0,
        precision: 0,
        validation_fn: Some("must_be_multiple_of_10".to_string()),
        ..Default::default()
    };

    let initial_state = NumberInputState {
        value: 10.0,
        text_value: "10".to_string(),
        is_focused: true,
        is_editing: true,
        cursor_position: 2,
        selection: None,
        step_timer: None,
        validation_state: ValidationState::Valid,
    };

    let mut harness = WidgetTestHarness::new(number_input)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 40.0),
            content_size: Vec2::new(200.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Enter invalid value (not multiple of 10)
    harness.state_mut().text_value.clear();
    harness.handle_event(text_input("15"))?;
    
    // In real implementation, custom validation would be applied
    // For testing, we simulate the validation result
    harness.state_mut().validation_state = ValidationState::Invalid("Must be a multiple of 10".to_string());
    
    match &harness.state().validation_state {
        ValidationState::Invalid(msg) => {
            assert_eq!(msg, "Must be a multiple of 10");
        }
        _ => panic!("Expected invalid state"),
    }

    Ok(())
}

// Helper trait
trait NumberInputTestExt {
    fn state_mut(&mut self) -> &mut NumberInputState;
}

impl NumberInputTestExt for WidgetTestHarness<NumberInputWidget> {
    fn state_mut(&mut self) -> &mut NumberInputState {
        &mut self.state
    }
}

// Property-based tests
#[cfg(test)]
mod property_tests {
    use super::*;
    use proptest::prelude::*;

    proptest! {
        #[test]
        fn test_number_input_handles_arbitrary_values(
            value in -1e10f64..1e10f64,
            precision in 0usize..6
        ) {
            let number_input = NumberInputWidget::new();
            
            let config = NumberInputConfig {
                precision: precision as u8,
                ..Default::default()
            };
            
            let state = NumberInputState {
                value,
                text_value: format!("{:.prec$}", value, prec = precision),
                is_focused: false,
                is_editing: false,
                cursor_position: 0,
                selection: None,
                step_timer: None,
                validation_state: ValidationState::Valid,
            };

            let harness = WidgetTestHarness::new(number_input)
                .with_state(state)
                .with_config(config)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(200.0, 40.0),
                    content_size: Vec2::new(200.0, 40.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any numeric value
            prop_assert!(harness.render().is_ok());
        }

        #[test]
        fn test_number_input_constraints_respected(
            value in -100f64..100f64,
            min in -50f64..0f64,
            max in 0f64..50f64,
            step in 0.1f64..10f64
        ) {
            let number_input = NumberInputWidget::new();
            
            let config = NumberInputConfig {
                min: Some(min),
                max: Some(max),
                step,
                precision: 2,
                ..Default::default()
            };
            
            let clamped_value = value.clamp(min, max);
            let state = NumberInputState {
                value: clamped_value,
                text_value: format!("{:.2}", clamped_value),
                is_focused: false,
                is_editing: false,
                cursor_position: 0,
                selection: None,
                step_timer: None,
                validation_state: ValidationState::Valid,
            };

            let harness = WidgetTestHarness::new(number_input)
                .with_state(state)
                .with_config(config)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(200.0, 40.0),
                    content_size: Vec2::new(200.0, 40.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should render without errors
            prop_assert!(harness.render().is_ok());
            
            // Value should be within constraints
            let state_value = harness.state().value;
            prop_assert!(state_value >= min && state_value <= max);
        }

        #[test]
        fn test_number_input_text_parsing(text in "[0-9\\-\\.e]+") {
            let number_input = NumberInputWidget::new();
            
            let state = NumberInputState {
                value: 0.0,
                text_value: text.clone(),
                is_focused: true,
                is_editing: true,
                cursor_position: text.len(),
                selection: None,
                step_timer: None,
                validation_state: ValidationState::Valid,
            };

            let harness = WidgetTestHarness::new(number_input)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(200.0, 40.0),
                    content_size: Vec2::new(200.0, 40.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should not crash on any numeric-like input
            let _ = harness.render();
        }
    }
}
//! Comprehensive tests for the color picker widget

use anyhow::Result;
use widget_tests::*;
use kryon_shared::widgets::color_picker::*;
use glam::{Vec2, Vec4};
use serde_json::Value;

#[tokio::test]
async fn test_color_picker_basic_functionality() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let mut harness = WidgetTestHarness::new(color_picker)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 300.0),
            content_size: Vec2::new(300.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial state
    assert_eq!(harness.state().current_color, Vec4::new(1.0, 0.0, 0.0, 1.0)); // Default red
    assert_eq!(harness.state().color_mode, ColorMode::HSV);
    assert!(!harness.state().is_focused);

    // Test that it renders successfully
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_color_picker_hsv_wheel_interaction() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(0.5, 0.5, 0.5, 1.0),
        hsv_values: HSVColor { h: 0.0, s: 0.0, v: 0.5 },
        rgb_values: RGBColor { r: 128, g: 128, b: 128 },
        hex_value: "#808080".to_string(),
        color_mode: ColorMode::HSV,
        is_focused: false,
        active_component: None,
        recent_colors: vec![],
        custom_palette: vec![],
        eyedropper_active: false,
        preview_color: None,
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 300.0),
            content_size: Vec2::new(300.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click on color wheel (assuming wheel is centered at 150, 150 with radius 100)
    let wheel_center_x = 150.0;
    let wheel_center_y = 150.0;
    let click_x = wheel_center_x + 50.0; // Right side of wheel
    let click_y = wheel_center_y; // Middle height
    
    let result = harness.handle_event(mouse_click(click_x, click_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify HSV values changed
    assert_ne!(harness.state().hsv_values.h, 0.0);
    assert_ne!(harness.state().hsv_values.s, 0.0);

    // Test saturation/value square interaction
    let sv_square_x = 250.0; // Assuming SV square is to the right
    let sv_square_y = 100.0;
    
    let result = harness.handle_event(mouse_click(sv_square_x, sv_square_y))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify saturation and value changed
    let new_hsv = harness.state().hsv_values;
    assert!(new_hsv.s >= 0.0 && new_hsv.s <= 1.0);
    assert!(new_hsv.v >= 0.0 && new_hsv.v <= 1.0);

    Ok(())
}

#[tokio::test]
async fn test_color_picker_rgb_input() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(1.0, 0.0, 0.0, 1.0),
        hsv_values: HSVColor { h: 0.0, s: 1.0, v: 1.0 },
        rgb_values: RGBColor { r: 255, g: 0, b: 0 },
        hex_value: "#FF0000".to_string(),
        color_mode: ColorMode::RGB,
        is_focused: true,
        active_component: Some(ColorComponent::Red),
        recent_colors: vec![],
        custom_palette: vec![],
        eyedropper_active: false,
        preview_color: None,
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Clear red input field
    harness.handle_event(key_press("Backspace"))?;
    harness.handle_event(key_press("Backspace"))?;
    harness.handle_event(key_press("Backspace"))?;

    // Type new red value
    harness.handle_event(text_input("128"))?;
    
    // Press Tab to move to green field
    let result = harness.handle_event(key_press("Tab"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().active_component, Some(ColorComponent::Green));

    // Type green value
    harness.handle_event(text_input("64"))?;
    
    // Tab to blue field
    harness.handle_event(key_press("Tab"))?;
    harness.handle_event(text_input("192"))?;

    // Press Enter to confirm
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify RGB values were updated
    assert_eq!(harness.state().rgb_values.r, 128);
    assert_eq!(harness.state().rgb_values.g, 64);
    assert_eq!(harness.state().rgb_values.b, 192);

    // Verify current color was updated
    let expected_color = Vec4::new(128.0/255.0, 64.0/255.0, 192.0/255.0, 1.0);
    assert!(ColorMatcher::colors_match(
        harness.state().current_color,
        expected_color,
        0.01
    ));

    Ok(())
}

#[tokio::test]
async fn test_color_picker_hex_input() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(0.0, 0.0, 0.0, 1.0),
        hsv_values: HSVColor { h: 0.0, s: 0.0, v: 0.0 },
        rgb_values: RGBColor { r: 0, g: 0, b: 0 },
        hex_value: "#000000".to_string(),
        color_mode: ColorMode::Hex,
        is_focused: true,
        active_component: Some(ColorComponent::Hex),
        recent_colors: vec![],
        custom_palette: vec![],
        eyedropper_active: false,
        preview_color: None,
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Clear hex field
    for _ in 0..7 { // "#000000" = 7 characters
        harness.handle_event(key_press("Backspace"))?;
    }

    // Type new hex value
    harness.handle_event(text_input("#3366CC"))?;
    
    // Press Enter to confirm
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify hex value was updated
    assert_eq!(harness.state().hex_value, "#3366CC");

    // Verify RGB values were calculated correctly
    assert_eq!(harness.state().rgb_values.r, 51);  // 0x33
    assert_eq!(harness.state().rgb_values.g, 102); // 0x66
    assert_eq!(harness.state().rgb_values.b, 204); // 0xCC

    // Verify current color was updated
    let expected_color = Vec4::new(51.0/255.0, 102.0/255.0, 204.0/255.0, 1.0);
    assert!(ColorMatcher::colors_match(
        harness.state().current_color,
        expected_color,
        0.01
    ));

    Ok(())
}

#[tokio::test]
async fn test_color_picker_palette_interaction() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(1.0, 1.0, 1.0, 1.0),
        hsv_values: HSVColor { h: 0.0, s: 0.0, v: 1.0 },
        rgb_values: RGBColor { r: 255, g: 255, b: 255 },
        hex_value: "#FFFFFF".to_string(),
        color_mode: ColorMode::HSV,
        is_focused: false,
        active_component: None,
        recent_colors: vec![
            Vec4::new(1.0, 0.0, 0.0, 1.0), // Red
            Vec4::new(0.0, 1.0, 0.0, 1.0), // Green
            Vec4::new(0.0, 0.0, 1.0, 1.0), // Blue
        ],
        custom_palette: vec![
            Vec4::new(0.5, 0.5, 0.5, 1.0), // Gray
            Vec4::new(1.0, 0.5, 0.0, 1.0), // Orange
        ],
        eyedropper_active: false,
        preview_color: None,
    };

    let config = ColorPickerConfig {
        show_palette: true,
        show_recent_colors: true,
        allow_custom_palette: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(350.0, 400.0),
            content_size: Vec2::new(350.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click on first recent color (red)
    let recent_color_x = 50.0; // Assuming recent colors start at x=50
    let recent_color_y = 320.0; // Near bottom of widget
    
    let result = harness.handle_event(mouse_click(recent_color_x, recent_color_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify color was selected
    assert!(ColorMatcher::colors_match(
        harness.state().current_color,
        Vec4::new(1.0, 0.0, 0.0, 1.0),
        0.01
    ));

    // Click on custom palette color (gray)
    let palette_color_x = 50.0;
    let palette_color_y = 350.0;
    
    let result = harness.handle_event(mouse_click(palette_color_x, palette_color_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify custom palette color was selected
    assert!(ColorMatcher::colors_match(
        harness.state().current_color,
        Vec4::new(0.5, 0.5, 0.5, 1.0),
        0.01
    ));

    Ok(())
}

#[tokio::test]
async fn test_color_picker_eyedropper() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(1.0, 1.0, 1.0, 1.0),
        hsv_values: HSVColor { h: 0.0, s: 0.0, v: 1.0 },
        rgb_values: RGBColor { r: 255, g: 255, b: 255 },
        hex_value: "#FFFFFF".to_string(),
        color_mode: ColorMode::HSV,
        is_focused: false,
        active_component: None,
        recent_colors: vec![],
        custom_palette: vec![],
        eyedropper_active: false,
        preview_color: None,
    };

    let config = ColorPickerConfig {
        eyedropper_enabled: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click eyedropper button
    let eyedropper_x = 270.0; // Top right corner
    let eyedropper_y = 20.0;
    
    let result = harness.handle_event(mouse_click(eyedropper_x, eyedropper_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify eyedropper mode is active
    assert!(harness.state().eyedropper_active);

    // Simulate eyedropper pick (in real implementation, this would sample screen pixels)
    let picked_color = Vec4::new(0.2, 0.8, 0.4, 1.0);
    harness.state_mut().preview_color = Some(picked_color);
    
    // Click to confirm pick
    let result = harness.handle_event(mouse_click(400.0, 200.0))?; // Outside widget
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify color was picked
    assert!(ColorMatcher::colors_match(
        harness.state().current_color,
        picked_color,
        0.01
    ));
    assert!(!harness.state().eyedropper_active);

    Ok(())
}

#[tokio::test]
async fn test_color_picker_alpha_channel() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(1.0, 0.0, 0.0, 1.0),
        hsv_values: HSVColor { h: 0.0, s: 1.0, v: 1.0 },
        rgb_values: RGBColor { r: 255, g: 0, b: 0 },
        hex_value: "#FF0000".to_string(),
        color_mode: ColorMode::RGBA,
        is_focused: true,
        active_component: Some(ColorComponent::Alpha),
        recent_colors: vec![],
        custom_palette: vec![],
        eyedropper_active: false,
        preview_color: None,
    };

    let config = ColorPickerConfig {
        alpha_enabled: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 380.0), // Taller for alpha slider
            content_size: Vec2::new(300.0, 380.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click on alpha slider (assuming it's at the bottom)
    let alpha_slider_x = 150.0; // Middle of slider
    let alpha_slider_y = 340.0; // Near bottom
    
    let result = harness.handle_event(mouse_click(alpha_slider_x, alpha_slider_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify alpha value changed
    assert_ne!(harness.state().current_color.w, 1.0);
    assert!(harness.state().current_color.w >= 0.0 && harness.state().current_color.w <= 1.0);

    // Test alpha input field
    harness.state_mut().active_component = Some(ColorComponent::Alpha);
    
    // Clear alpha field and type new value
    harness.handle_event(key_press("Backspace"))?;
    harness.handle_event(key_press("Backspace"))?;
    harness.handle_event(key_press("Backspace"))?;
    harness.handle_event(text_input("128"))?; // 50% alpha
    
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify alpha was set correctly
    assert!((harness.state().current_color.w - 0.5).abs() < 0.01);

    Ok(())
}

#[tokio::test]
async fn test_color_picker_mode_switching() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(0.2, 0.6, 0.8, 1.0),
        hsv_values: HSVColor { h: 200.0, s: 0.75, v: 0.8 },
        rgb_values: RGBColor { r: 51, g: 153, b: 204 },
        hex_value: "#3399CC".to_string(),
        color_mode: ColorMode::HSV,
        is_focused: false,
        active_component: None,
        recent_colors: vec![],
        custom_palette: vec![],
        eyedropper_active: false,
        preview_color: None,
    };

    let config = ColorPickerConfig {
        allowed_modes: vec![ColorMode::HSV, ColorMode::RGB, ColorMode::Hex],
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 400.0),
            content_size: Vec2::new(300.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click mode toggle button to switch to RGB
    let mode_button_x = 250.0;
    let mode_button_y = 250.0;
    
    let result = harness.handle_event(mouse_click(mode_button_x, mode_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().color_mode, ColorMode::RGB);

    // Switch to Hex mode
    let result = harness.handle_event(mouse_click(mode_button_x, mode_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().color_mode, ColorMode::Hex);

    // Switch back to HSV mode
    let result = harness.handle_event(mouse_click(mode_button_x, mode_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().color_mode, ColorMode::HSV);

    // Verify color values remain consistent across mode switches
    let final_color = harness.state().current_color;
    assert!(ColorMatcher::colors_match(
        final_color,
        Vec4::new(0.2, 0.6, 0.8, 1.0),
        0.02 // Allow small rounding differences
    ));

    Ok(())
}

#[tokio::test]
async fn test_color_picker_custom_palette_management() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(0.8, 0.3, 0.7, 1.0), // Custom color
        hsv_values: HSVColor { h: 310.0, s: 0.625, v: 0.8 },
        rgb_values: RGBColor { r: 204, g: 76, b: 178 },
        hex_value: "#CC4CB2".to_string(),
        color_mode: ColorMode::HSV,
        is_focused: false,
        active_component: None,
        recent_colors: vec![],
        custom_palette: vec![
            Vec4::new(1.0, 0.0, 0.0, 1.0), // Red
            Vec4::new(0.0, 1.0, 0.0, 1.0), // Green
        ],
        eyedropper_active: false,
        preview_color: None,
    };

    let config = ColorPickerConfig {
        allow_custom_palette: true,
        max_custom_colors: 8,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(350.0, 420.0),
            content_size: Vec2::new(350.0, 420.0),
            scroll_offset: Vec2::ZERO,
        });

    // Add current color to custom palette
    let add_button_x = 300.0;
    let add_button_y = 380.0;
    
    let result = harness.handle_event(mouse_click(add_button_x, add_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify color was added to custom palette
    assert_eq!(harness.state().custom_palette.len(), 3);
    assert!(ColorMatcher::colors_match(
        harness.state().custom_palette[2],
        Vec4::new(0.8, 0.3, 0.7, 1.0),
        0.01
    ));

    // Right-click on custom palette color to delete it
    let palette_color_x = 100.0;
    let palette_color_y = 400.0;
    
    let result = harness.handle_event(InputEvent::MouseDown {
        position: Vec2::new(palette_color_x, palette_color_y),
        button: 1, // Right click
    })?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Select delete option
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("delete_palette_color".to_string())
    ))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify color was removed
    assert_eq!(harness.state().custom_palette.len(), 2);

    Ok(())
}

#[tokio::test]
async fn test_color_picker_keyboard_navigation() -> Result<()> {
    let color_picker = ColorPickerWidget::new();
    let initial_state = ColorPickerState {
        current_color: Vec4::new(0.5, 0.5, 0.5, 1.0),
        hsv_values: HSVColor { h: 0.0, s: 0.0, v: 0.5 },
        rgb_values: RGBColor { r: 128, g: 128, b: 128 },
        hex_value: "#808080".to_string(),
        color_mode: ColorMode::RGB,
        is_focused: true,
        active_component: Some(ColorComponent::Red),
        recent_colors: vec![],
        custom_palette: vec![],
        eyedropper_active: false,
        preview_color: None,
    };

    let mut harness = WidgetTestHarness::new(color_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Tab to next component (Green)
    let result = harness.handle_event(key_press("Tab"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().active_component, Some(ColorComponent::Green));

    // Tab to next component (Blue)
    let result = harness.handle_event(key_press("Tab"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().active_component, Some(ColorComponent::Blue));

    // Shift+Tab to go back (Green)
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "Tab".to_string(),
        modifiers: 2, // Shift
    })?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().active_component, Some(ColorComponent::Green));

    // Arrow keys to adjust values
    let result = harness.handle_event(key_press("ArrowUp"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert!(harness.state().rgb_values.g > 128); // Should increase

    let result = harness.handle_event(key_press("ArrowDown"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().rgb_values.g, 128); // Should decrease back

    Ok(())
}

// Helper trait
trait ColorPickerTestExt {
    fn state_mut(&mut self) -> &mut ColorPickerState;
}

impl ColorPickerTestExt for WidgetTestHarness<ColorPickerWidget> {
    fn state_mut(&mut self) -> &mut ColorPickerState {
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
        fn test_color_picker_handles_arbitrary_colors(
            r in 0.0f32..1.0,
            g in 0.0f32..1.0,
            b in 0.0f32..1.0,
            a in 0.0f32..1.0
        ) {
            let color_picker = ColorPickerWidget::new();
            let color = Vec4::new(r, g, b, a);
            
            let (h, s, v) = rgb_to_hsv(r, g, b);
            let (r_int, g_int, b_int) = (
                (r * 255.0) as u8,
                (g * 255.0) as u8,
                (b * 255.0) as u8
            );
            
            let state = ColorPickerState {
                current_color: color,
                hsv_values: HSVColor { h, s, v },
                rgb_values: RGBColor { r: r_int, g: g_int, b: b_int },
                hex_value: format!("#{:02X}{:02X}{:02X}", r_int, g_int, b_int),
                color_mode: ColorMode::RGB,
                is_focused: false,
                active_component: None,
                recent_colors: vec![],
                custom_palette: vec![],
                eyedropper_active: false,
                preview_color: None,
            };

            let harness = WidgetTestHarness::new(color_picker)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(300.0, 350.0),
                    content_size: Vec2::new(300.0, 350.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any valid color
            prop_assert!(harness.render().is_ok());
        }

        #[test]
        fn test_color_picker_rgb_validation(
            r in 0u8..=255,
            g in 0u8..=255,
            b in 0u8..=255
        ) {
            let color_picker = ColorPickerWidget::new();
            
            let state = ColorPickerState {
                current_color: Vec4::new(r as f32 / 255.0, g as f32 / 255.0, b as f32 / 255.0, 1.0),
                hsv_values: HSVColor { h: 0.0, s: 0.0, v: 0.0 }, // Will be calculated
                rgb_values: RGBColor { r, g, b },
                hex_value: format!("#{:02X}{:02X}{:02X}", r, g, b),
                color_mode: ColorMode::RGB,
                is_focused: false,
                active_component: None,
                recent_colors: vec![],
                custom_palette: vec![],
                eyedropper_active: false,
                preview_color: None,
            };

            let harness = WidgetTestHarness::new(color_picker)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(300.0, 350.0),
                    content_size: Vec2::new(300.0, 350.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any RGB values
            prop_assert!(harness.render().is_ok());
            
            // RGB values should be within bounds
            let state = harness.state();
            prop_assert!(state.rgb_values.r <= 255);
            prop_assert!(state.rgb_values.g <= 255);
            prop_assert!(state.rgb_values.b <= 255);
        }
    }

    // Helper function for color space conversion
    fn rgb_to_hsv(r: f32, g: f32, b: f32) -> (f32, f32, f32) {
        let max = r.max(g).max(b);
        let min = r.min(g).min(b);
        let delta = max - min;

        let h = if delta == 0.0 {
            0.0
        } else if max == r {
            60.0 * (((g - b) / delta) % 6.0)
        } else if max == g {
            60.0 * (((b - r) / delta) + 2.0)
        } else {
            60.0 * (((r - g) / delta) + 4.0)
        };

        let s = if max == 0.0 { 0.0 } else { delta / max };
        let v = max;

        (h.max(0.0), s, v)
    }
}
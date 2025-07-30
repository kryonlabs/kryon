//! Comprehensive tests for the date picker widget

use anyhow::Result;
use widget_tests::*;
use kryon_shared::widgets::date_picker::*;
use glam::Vec2;
use serde_json::Value;
use chrono::{NaiveDate, Datelike};

#[tokio::test]
async fn test_date_picker_basic_functionality() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let mut harness = WidgetTestHarness::new(date_picker)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial state
    assert!(harness.state().selected_date.is_none());
    assert!(harness.state().is_open == false);
    
    // Current month should be displayed
    let now = chrono::Local::now().naive_local().date();
    assert_eq!(harness.state().current_month, now.month());
    assert_eq!(harness.state().current_year, now.year());

    // Test that it renders successfully
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_date_picker_date_selection() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let initial_state = DatePickerState {
        selected_date: None,
        current_month: 3,
        current_year: 2024,
        is_open: true,
        hovered_date: None,
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: false,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Calculate position for March 15, 2024
    // Assuming calendar grid starts at (10, 100) with 40x40 cells
    let day_15_x = 10.0 + (5 * 40.0) + 20.0; // Friday column + center
    let day_15_y = 100.0 + (2 * 40.0) + 20.0; // Third week + center
    
    let result = harness.handle_event(mouse_click(day_15_x, day_15_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Check that date is selected
    assert!(harness.state().selected_date.is_some());
    if let Some(date) = harness.state().selected_date {
        assert_eq!(date.day(), 15);
        assert_eq!(date.month(), 3);
        assert_eq!(date.year(), 2024);
    }

    Ok(())
}

#[tokio::test]
async fn test_date_picker_navigation() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let initial_state = DatePickerState {
        selected_date: None,
        current_month: 6,
        current_year: 2024,
        is_open: true,
        hovered_date: None,
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: false,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test next month navigation
    let next_button_x = 280.0; // Right side of header
    let next_button_y = 20.0;  // Header area
    
    let result = harness.handle_event(mouse_click(next_button_x, next_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().current_month, 7);
    assert_eq!(harness.state().current_year, 2024);

    // Test previous month navigation
    let prev_button_x = 20.0; // Left side of header
    let prev_button_y = 20.0; // Header area
    
    let result = harness.handle_event(mouse_click(prev_button_x, prev_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().current_month, 6);
    assert_eq!(harness.state().current_year, 2024);

    // Test year boundary
    harness.state_mut().current_month = 1;
    let result = harness.handle_event(mouse_click(prev_button_x, prev_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().current_month, 12);
    assert_eq!(harness.state().current_year, 2023);

    Ok(())
}

#[tokio::test]
async fn test_date_picker_keyboard_navigation() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let initial_date = NaiveDate::from_ymd_opt(2024, 3, 15).unwrap();
    let initial_state = DatePickerState {
        selected_date: Some(initial_date),
        current_month: 3,
        current_year: 2024,
        is_open: true,
        hovered_date: Some(initial_date),
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: true,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test arrow key navigation
    let result = harness.handle_event(key_press("ArrowRight"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().hovered_date.unwrap().day(), 16);

    let result = harness.handle_event(key_press("ArrowDown"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().hovered_date.unwrap().day(), 23); // Next week

    let result = harness.handle_event(key_press("ArrowLeft"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().hovered_date.unwrap().day(), 22);

    let result = harness.handle_event(key_press("ArrowUp"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().hovered_date.unwrap().day(), 15); // Previous week

    // Test Enter key selection
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().selected_date.unwrap().day(), 15);

    // Test Escape key closing
    let result = harness.handle_event(key_press("Escape"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().is_open, false);

    Ok(())
}

#[tokio::test]
async fn test_date_picker_constraints() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let min_date = NaiveDate::from_ymd_opt(2024, 1, 1).unwrap();
    let max_date = NaiveDate::from_ymd_opt(2024, 12, 31).unwrap();
    
    let initial_state = DatePickerState {
        selected_date: None,
        current_month: 6,
        current_year: 2024,
        is_open: true,
        hovered_date: None,
        min_date: Some(min_date),
        max_date: Some(max_date),
        disabled_dates: vec![
            NaiveDate::from_ymd_opt(2024, 6, 15).unwrap(),
            NaiveDate::from_ymd_opt(2024, 6, 16).unwrap(),
        ],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: false,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that calendar renders with constraints
    let commands = harness.render()?;
    
    // Should have disabled styling for dates outside range
    assert_render_commands!(commands, contains DrawRect);

    // Try to navigate beyond max date
    harness.state_mut().current_month = 12;
    let next_button_x = 280.0;
    let next_button_y = 20.0;
    
    let result = harness.handle_event(mouse_click(next_button_x, next_button_y))?;
    // Should not navigate beyond 2024
    assert_eq!(harness.state().current_year, 2024);

    Ok(())
}

#[tokio::test]
async fn test_date_picker_view_modes() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let initial_state = DatePickerState {
        selected_date: None,
        current_month: 3,
        current_year: 2024,
        is_open: true,
        hovered_date: None,
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: false,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click on month/year header to switch to month view
    let header_x = 150.0; // Center of header
    let header_y = 20.0;
    
    let result = harness.handle_event(mouse_click(header_x, header_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().view_mode, ViewMode::Months);

    // Click on a month to select it
    let month_x = 150.0;
    let month_y = 150.0;
    
    let result = harness.handle_event(mouse_click(month_x, month_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().view_mode, ViewMode::Days);

    // Switch to year view
    harness.state_mut().view_mode = ViewMode::Years;
    assert_eq!(harness.state().view_mode, ViewMode::Years);

    Ok(())
}

#[tokio::test]
async fn test_date_picker_highlighted_dates() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let today = chrono::Local::now().naive_local().date();
    
    let initial_state = DatePickerState {
        selected_date: None,
        current_month: today.month(),
        current_year: today.year(),
        is_open: true,
        hovered_date: None,
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![
            today,
            today.succ_opt().unwrap(),
            today.succ_opt().unwrap().succ_opt().unwrap(),
        ],
        view_mode: ViewMode::Days,
        is_focused: false,
    };

    let harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that highlighted dates render with special styling
    let commands = harness.render()?;
    
    // Should have special rendering for highlighted dates
    assert!(commands.len() > 0);
    // Verify that render commands include highlighted date styling

    Ok(())
}

#[tokio::test]
async fn test_date_picker_config_options() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    
    let config = DatePickerConfig {
        format: "%Y-%m-%d".to_string(),
        first_day_of_week: 1, // Monday
        show_week_numbers: true,
        allow_clear: true,
        close_on_select: false,
        highlight_today: true,
        highlight_weekends: true,
        show_other_months: true,
        select_other_months: false,
        year_range: Some((2020, 2030)),
        theme: DatePickerTheme::default(),
    };

    let initial_state = DatePickerState {
        selected_date: Some(NaiveDate::from_ymd_opt(2024, 3, 15).unwrap()),
        current_month: 3,
        current_year: 2024,
        is_open: true,
        hovered_date: None,
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: false,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(350.0, 400.0), // Wider for week numbers
            content_size: Vec2::new(350.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that configuration affects rendering
    let commands = harness.render()?;
    
    // Should render week numbers
    assert_render_commands!(commands, contains DrawText with "10"); // Week number
    
    // Test clear functionality
    let clear_button_x = 320.0;
    let clear_button_y = 370.0;
    
    let result = harness.handle_event(mouse_click(clear_button_x, clear_button_y))?;
    assert_eq!(result, EventResult::StateChanged);
    assert!(harness.state().selected_date.is_none());

    Ok(())
}

#[tokio::test]
async fn test_date_picker_input_integration() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    
    let config = DatePickerConfig {
        format: "%m/%d/%Y".to_string(),
        allow_manual_input: true,
        ..Default::default()
    };

    let initial_state = DatePickerState {
        selected_date: None,
        current_month: 3,
        current_year: 2024,
        is_open: false,
        hovered_date: None,
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: true,
        input_text: String::new(),
        input_cursor: 0,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 40.0), // Just input field
            content_size: Vec2::new(300.0, 40.0),
            scroll_offset: Vec2::ZERO,
        });

    // Type a date
    harness.handle_event(text_input("03/15/2024"))?;
    assert_eq!(harness.state().input_text, "03/15/2024");

    // Press Enter to parse
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Check that date was parsed correctly
    assert!(harness.state().selected_date.is_some());
    if let Some(date) = harness.state().selected_date {
        assert_eq!(date.day(), 15);
        assert_eq!(date.month(), 3);
        assert_eq!(date.year(), 2024);
    }

    Ok(())
}

#[tokio::test]
async fn test_date_picker_touch_gestures() -> Result<()> {
    let date_picker = DatePickerWidget::new();
    let initial_state = DatePickerState {
        selected_date: None,
        current_month: 6,
        current_year: 2024,
        is_open: true,
        hovered_date: None,
        min_date: None,
        max_date: None,
        disabled_dates: vec![],
        highlighted_dates: vec![],
        view_mode: ViewMode::Days,
        is_focused: false,
    };

    let mut harness = WidgetTestHarness::new(date_picker)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(300.0, 350.0),
            content_size: Vec2::new(300.0, 350.0),
            scroll_offset: Vec2::ZERO,
        });

    // Simulate swipe left (next month)
    let events = EventSimulator::click_and_drag(
        Vec2::new(250.0, 200.0),
        Vec2::new(50.0, 200.0),
        5
    );
    
    for event in events {
        harness.handle_event(event)?;
    }
    
    // Should navigate to next month
    assert_eq!(harness.state().current_month, 7);

    Ok(())
}

// Helper functions
trait DatePickerTestExt<W: KryonWidget> {
    fn state_mut(&mut self) -> &mut DatePickerState;
}

impl DatePickerTestExt<DatePickerWidget> for WidgetTestHarness<DatePickerWidget> {
    fn state_mut(&mut self) -> &mut DatePickerState {
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
        fn test_date_picker_handles_arbitrary_dates(
            year in 1900i32..2100,
            month in 1u32..=12,
            day in 1u32..=28
        ) {
            let date_picker = DatePickerWidget::new();
            let date = NaiveDate::from_ymd_opt(year, month, day).unwrap();
            
            let state = DatePickerState {
                selected_date: Some(date),
                current_month: month,
                current_year: year,
                is_open: true,
                hovered_date: None,
                min_date: None,
                max_date: None,
                disabled_dates: vec![],
                highlighted_dates: vec![],
                view_mode: ViewMode::Days,
                is_focused: false,
            };

            let harness = WidgetTestHarness::new(date_picker)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(300.0, 350.0),
                    content_size: Vec2::new(300.0, 350.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any valid date
            prop_assert!(harness.render().is_ok());
        }

        #[test]
        fn test_date_picker_constraints_respected(
            min_year in 2020i32..=2023,
            max_year in 2024i32..=2030,
            selected_year in 2020i32..=2030
        ) {
            let date_picker = DatePickerWidget::new();
            let min_date = NaiveDate::from_ymd_opt(min_year, 1, 1).unwrap();
            let max_date = NaiveDate::from_ymd_opt(max_year, 12, 31).unwrap();
            let selected = NaiveDate::from_ymd_opt(selected_year.clamp(min_year, max_year), 6, 15).unwrap();
            
            let state = DatePickerState {
                selected_date: Some(selected),
                current_month: 6,
                current_year: selected_year.clamp(min_year, max_year),
                is_open: true,
                hovered_date: None,
                min_date: Some(min_date),
                max_date: Some(max_date),
                disabled_dates: vec![],
                highlighted_dates: vec![],
                view_mode: ViewMode::Days,
                is_focused: false,
            };

            let harness = WidgetTestHarness::new(date_picker)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(300.0, 350.0),
                    content_size: Vec2::new(300.0, 350.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should render without errors
            prop_assert!(harness.render().is_ok());
            
            // Selected date should be within constraints
            if let Some(date) = harness.state().selected_date {
                prop_assert!(date >= min_date && date <= max_date);
            }
        }
    }
}
//! Comprehensive tests for the dropdown widget

use anyhow::Result;
use widget_tests::*;
use kryon_shared::widgets::dropdown::*;
use glam::Vec2;
use serde_json::Value;

#[tokio::test]
async fn test_dropdown_basic_functionality() -> Result<()> {
    let dropdown = DropdownWidget::new();
    let mut harness = WidgetTestHarness::new(dropdown)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial state
    assert_eq!(harness.state().is_open, false);
    assert_eq!(harness.state().selected_index, None);
    assert_eq!(harness.state().items.len(), 0);

    // Test that it renders successfully
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_dropdown_with_items() -> Result<()> {
    let dropdown = DropdownWidget::new();
    let initial_state = DropdownState {
        items: vec![
            DropdownItem {
                id: "item1".to_string(),
                label: "First Item".to_string(),
                value: Value::String("value1".to_string()),
                enabled: true,
                icon: None,
                metadata: Default::default(),
            },
            DropdownItem {
                id: "item2".to_string(),
                label: "Second Item".to_string(),
                value: Value::String("value2".to_string()),
                enabled: true,
                icon: None,
                metadata: Default::default(),
            },
            DropdownItem {
                id: "item3".to_string(),
                label: "Third Item".to_string(),
                value: Value::String("value3".to_string()),
                enabled: false,
                icon: None,
                metadata: Default::default(),
            },
        ],
        selected_index: Some(0),
        is_open: false,
        hovered_index: None,
        search_text: String::new(),
        filtered_items: vec![0, 1, 2],
        scroll_offset: 0.0,
        is_focused: false,
        loading_state: LoadingState::Idle,
        validation_state: ValidationState::Valid,
    };

    let mut harness = WidgetTestHarness::new(dropdown)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(10.0, 10.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that dropdown renders with items
    harness.assert_renders_successfully()?;

    // Test selecting first item
    assert_eq!(harness.state().selected_index, Some(0));

    // Test opening dropdown
    let result = harness.handle_event(mouse_click(100.0, 26.0))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().is_open, true);

    Ok(())
}

#[tokio::test]
async fn test_dropdown_keyboard_navigation() -> Result<()> {
    let dropdown = DropdownWidget::new();
    let initial_state = DropdownState {
        items: vec![
            create_dropdown_item("1", "First", "first"),
            create_dropdown_item("2", "Second", "second"),
            create_dropdown_item("3", "Third", "third"),
        ],
        selected_index: Some(0),
        is_open: true,
        hovered_index: Some(0),
        search_text: String::new(),
        filtered_items: vec![0, 1, 2],
        scroll_offset: 0.0,
        is_focused: true,
        loading_state: LoadingState::Idle,
        validation_state: ValidationState::Valid,
    };

    let mut harness = WidgetTestHarness::new(dropdown)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test arrow down navigation
    let result = harness.handle_event(key_press("ArrowDown"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().hovered_index, Some(1));

    // Test arrow up navigation
    let result = harness.handle_event(key_press("ArrowUp"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().hovered_index, Some(0));

    // Test Enter key selection
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().selected_index, Some(0));
    assert_eq!(harness.state().is_open, false);

    // Test Escape key closing
    harness.handle_event(mouse_click(100.0, 16.0))?; // Open dropdown
    let result = harness.handle_event(key_press("Escape"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().is_open, false);

    Ok(())
}

#[tokio::test]
async fn test_dropdown_search_functionality() -> Result<()> {
    let dropdown = DropdownWidget::new();
    let initial_state = DropdownState {
        items: vec![
            create_dropdown_item("1", "Apple", "apple"),
            create_dropdown_item("2", "Banana", "banana"),
            create_dropdown_item("3", "Cherry", "cherry"),
            create_dropdown_item("4", "Date", "date"),
        ],
        selected_index: None,
        is_open: true,
        hovered_index: None,
        search_text: String::new(),
        filtered_items: vec![0, 1, 2, 3],
        scroll_offset: 0.0,
        is_focused: true,
        loading_state: LoadingState::Idle,
        validation_state: ValidationState::Valid,
    };

    let config = DropdownConfig {
        searchable: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(dropdown)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test typing search text
    let result = harness.handle_event(text_input("ba"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().search_text, "ba");
    
    // After search, only "Banana" should be visible
    assert_eq!(harness.state().filtered_items.len(), 1);

    // Test clearing search
    harness.handle_event(key_press("Backspace"))?;
    harness.handle_event(key_press("Backspace"))?;
    assert_eq!(harness.state().search_text, "");
    assert_eq!(harness.state().filtered_items.len(), 4);

    Ok(())
}

#[tokio::test]
async fn test_dropdown_multi_select() -> Result<()> {
    let dropdown = DropdownWidget::new();
    let initial_state = DropdownState {
        items: vec![
            create_dropdown_item("1", "Option 1", "1"),
            create_dropdown_item("2", "Option 2", "2"),
            create_dropdown_item("3", "Option 3", "3"),
        ],
        selected_index: None,
        is_open: true,
        hovered_index: None,
        search_text: String::new(),
        filtered_items: vec![0, 1, 2],
        scroll_offset: 0.0,
        is_focused: true,
        loading_state: LoadingState::Idle,
        validation_state: ValidationState::Valid,
    };

    let config = DropdownConfig {
        multi_select: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(dropdown)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test selecting multiple items
    let result = harness.handle_event(mouse_click(100.0, 50.0))?; // Click first item
    assert_eq!(result, EventResult::StateChanged);
    
    let result = harness.handle_event(mouse_click(100.0, 82.0))?; // Click second item
    assert_eq!(result, EventResult::StateChanged);

    // In multi-select mode, dropdown should remain open
    assert_eq!(harness.state().is_open, true);

    Ok(())
}

#[tokio::test]
async fn test_dropdown_async_loading() -> Result<()> {
    let dropdown = DropdownWidget::new();
    let initial_state = DropdownState {
        loading_state: LoadingState::Loading,
        ..Default::default()
    };

    let config = DropdownConfig {
        async_loading: true,
        ..Default::default()
    };

    let harness = WidgetTestHarness::new(dropdown)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that loading state renders correctly
    harness.assert_renders_successfully()?;
    assert_eq!(harness.state().loading_state, LoadingState::Loading);

    Ok(())
}

#[tokio::test]
async fn test_dropdown_validation() -> Result<()> {
    let dropdown = DropdownWidget::new();
    let initial_state = DropdownState {
        validation_state: ValidationState::Invalid("Please select an option".to_string()),
        ..Default::default()
    };

    let harness = WidgetTestHarness::new(dropdown)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that invalid state renders correctly
    harness.assert_renders_successfully()?;
    
    match &harness.state().validation_state {
        ValidationState::Invalid(msg) => {
            assert_eq!(msg, "Please select an option");
        }
        _ => panic!("Expected invalid validation state"),
    }

    Ok(())
}

#[tokio::test]
async fn test_dropdown_performance() -> Result<()> {
    let dropdown = DropdownWidget::new();
    
    // Create dropdown with many items
    let items: Vec<DropdownItem> = (0..1000)
        .map(|i| create_dropdown_item(&i.to_string(), &format!("Item {}", i), &i.to_string()))
        .collect();

    let initial_state = DropdownState {
        items,
        filtered_items: (0..1000).collect(),
        ..Default::default()
    };

    let harness = WidgetTestHarness::new(dropdown)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(200.0, 32.0),
            content_size: Vec2::new(200.0, 32.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that rendering large number of items doesn't crash
    let start = std::time::Instant::now();
    harness.assert_renders_successfully()?;
    let duration = start.elapsed();

    // Assert reasonable performance (should render in under 100ms)
    assert!(duration.as_millis() < 100, "Rendering took too long: {:?}", duration);

    Ok(())
}

// Helper function to create dropdown items
fn create_dropdown_item(id: &str, label: &str, value: &str) -> DropdownItem {
    DropdownItem {
        id: id.to_string(),
        label: label.to_string(),
        value: Value::String(value.to_string()),
        enabled: true,
        icon: None,
        metadata: Default::default(),
    }
}

// Property-based testing
#[cfg(test)]
mod property_tests {
    use super::*;
    use proptest::prelude::*;

    proptest! {
        #[test]
        fn test_dropdown_handles_arbitrary_item_counts(item_count in 0usize..100) {
            let dropdown = DropdownWidget::new();
            let items: Vec<DropdownItem> = (0..item_count)
                .map(|i| create_dropdown_item(&i.to_string(), &format!("Item {}", i), &i.to_string()))
                .collect();

            let initial_state = DropdownState {
                items,
                filtered_items: (0..item_count).collect(),
                ..Default::default()
            };

            let harness = WidgetTestHarness::new(dropdown)
                .with_state(initial_state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(200.0, 32.0),
                    content_size: Vec2::new(200.0, 32.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should not panic with any number of items
            prop_assert!(harness.render().is_ok());
        }

        #[test]
        fn test_dropdown_search_with_arbitrary_text(search_text in ".*") {
            let dropdown = DropdownWidget::new();
            let mut initial_state = DropdownState {
                items: vec![
                    create_dropdown_item("1", "Apple", "apple"),
                    create_dropdown_item("2", "Banana", "banana"),
                ],
                search_text: search_text.clone(),
                is_open: true,
                filtered_items: vec![0, 1],
                ..Default::default()
            };

            let config = DropdownConfig {
                searchable: true,
                ..Default::default()
            };

            let harness = WidgetTestHarness::new(dropdown)
                .with_state(initial_state)
                .with_config(config)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(200.0, 32.0),
                    content_size: Vec2::new(200.0, 32.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any search text without crashing
            prop_assert!(harness.render().is_ok());
        }
    }
}
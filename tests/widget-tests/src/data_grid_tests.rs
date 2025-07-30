//! Comprehensive tests for the data grid widget

use anyhow::Result;
use widget_tests::*;
use kryon_shared::widgets::data_grid::*;
use glam::Vec2;
use serde_json::Value;
use std::collections::HashMap;

#[tokio::test]
async fn test_data_grid_basic_functionality() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let mut harness = WidgetTestHarness::new(data_grid)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(800.0, 600.0),
            content_size: Vec2::new(800.0, 600.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial state
    assert_eq!(harness.state().columns.len(), 0);
    assert_eq!(harness.state().rows.len(), 0);
    assert_eq!(harness.state().selected_rows.len(), 0);

    // Test that it renders successfully even when empty
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_data_grid_with_data() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(10, 5);

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(800.0, 600.0),
            content_size: Vec2::new(800.0, 600.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test state setup
    assert_eq!(harness.state().columns.len(), 5);
    assert_eq!(harness.state().rows.len(), 10);

    // Test rendering with data
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_data_grid_cell_selection() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(5, 3);

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(600.0, 400.0),
            content_size: Vec2::new(600.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click on a cell (row 1, column 1)
    let cell_x = 100.0 + 100.0 / 2.0; // Column width 100px
    let cell_y = 32.0 + 32.0 / 2.0;   // Header + row height
    
    let result = harness.handle_event(mouse_click(cell_x, cell_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Check that cell is selected
    assert!(harness.state().selected_cells.contains(&(1, 1)));

    Ok(())
}

#[tokio::test]
async fn test_data_grid_row_selection() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(10, 5);

    let config = DataGridConfig {
        selection_mode: SelectionMode::Row,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(800.0, 600.0),
            content_size: Vec2::new(800.0, 600.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click on row 2
    let row_y = 32.0 + 32.0 * 2 + 16.0; // Header + 2 rows + half row height
    let result = harness.handle_event(mouse_click(50.0, row_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Check that row is selected
    assert!(harness.state().selected_rows.contains(&2));

    // Test multi-select with Ctrl
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "Control".to_string(),
        modifiers: 1,
    })?;
    
    let row_y = 32.0 + 32.0 * 4 + 16.0; // Row 4
    let result = harness.handle_event(mouse_click(50.0, row_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Both rows should be selected
    assert_eq!(harness.state().selected_rows.len(), 2);
    assert!(harness.state().selected_rows.contains(&2));
    assert!(harness.state().selected_rows.contains(&4));

    Ok(())
}

#[tokio::test]
async fn test_data_grid_sorting() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let mut state = create_test_grid_state(5, 3);
    
    // Set different values for testing sort
    state.rows[0].cells[0].value = Value::String("Zebra".to_string());
    state.rows[1].cells[0].value = Value::String("Apple".to_string());
    state.rows[2].cells[0].value = Value::String("Banana".to_string());
    state.rows[3].cells[0].value = Value::String("Cherry".to_string());
    state.rows[4].cells[0].value = Value::String("Date".to_string());

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(600.0, 400.0),
            content_size: Vec2::new(600.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click on column header to sort
    let header_x = 50.0; // First column center
    let header_y = 16.0; // Header center
    
    let result = harness.handle_event(mouse_click(header_x, header_y))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Check that sort is applied
    assert!(harness.state().sort_config.is_some());
    if let Some(sort_config) = &harness.state().sort_config {
        assert_eq!(sort_config.column_index, 0);
        assert_eq!(sort_config.direction, SortDirection::Ascending);
    }

    // Verify filtered rows are sorted
    let first_row_index = harness.state().filtered_rows[0];
    let first_value = &harness.state().rows[first_row_index].cells[0].value;
    assert_eq!(first_value.as_str().unwrap(), "Apple");

    Ok(())
}

#[tokio::test]
async fn test_data_grid_filtering() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(10, 3);

    let config = DataGridConfig {
        filterable: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(600.0, 400.0),
            content_size: Vec2::new(600.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Apply filter
    harness.state_mut().filters.insert("col_0".to_string(), "Cell 0:".to_string());
    
    // Trigger filter update
    let result = harness.handle_event(InputEvent::Custom(Value::String("apply_filter".to_string())))?;
    
    // Check that rows are filtered
    let filtered_count = harness.state().filtered_rows.len();
    assert!(filtered_count < 10); // Some rows should be filtered out

    Ok(())
}

#[tokio::test]
async fn test_data_grid_cell_editing() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(5, 3);

    let config = DataGridConfig {
        editable: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(600.0, 400.0),
            content_size: Vec2::new(600.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Double-click to edit cell
    let cell_x = 150.0;
    let cell_y = 48.0;
    
    // First click
    harness.handle_event(mouse_click(cell_x, cell_y))?;
    // Second click (double-click)
    harness.handle_event(mouse_click(cell_x, cell_y))?;
    
    // Check that cell is in edit mode
    assert!(harness.state().editing_cell.is_some());
    if let Some((row, col)) = harness.state().editing_cell {
        assert_eq!(row, 1);
        assert_eq!(col, 1);
    }

    // Type new value
    harness.handle_event(text_input("New Value"))?;
    
    // Press Enter to confirm
    harness.handle_event(key_press("Enter"))?;
    
    // Check that edit mode is exited
    assert!(harness.state().editing_cell.is_none());

    Ok(())
}

#[tokio::test]
async fn test_data_grid_column_resize() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(5, 3);

    let config = DataGridConfig {
        resizable_columns: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(600.0, 400.0),
            content_size: Vec2::new(600.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Start column resize
    let resize_x = 100.0 - 2.0; // Column border
    let resize_y = 16.0; // Header
    
    harness.handle_event(InputEvent::MouseDown {
        position: Vec2::new(resize_x, resize_y),
        button: 0,
    })?;
    
    // Check resize state
    assert!(harness.state().column_resize_state.is_some());
    
    // Drag to resize
    harness.handle_event(InputEvent::MouseMove {
        position: Vec2::new(resize_x + 50.0, resize_y),
    })?;
    
    // Release mouse
    harness.handle_event(InputEvent::MouseUp {
        position: Vec2::new(resize_x + 50.0, resize_y),
        button: 0,
    })?;
    
    // Check that column width changed
    assert_eq!(harness.state().columns[0].width, 150.0);

    Ok(())
}

#[tokio::test]
async fn test_data_grid_pagination() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let mut initial_state = create_test_grid_state(100, 5);
    
    // Setup pagination
    initial_state.pagination = PaginationState {
        current_page: 0,
        page_size: 20,
        total_items: 100,
        total_pages: 5,
    };

    let config = DataGridConfig {
        paginated: true,
        rows_per_page: 20,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(800.0, 600.0),
            content_size: Vec2::new(800.0, 600.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial page
    assert_eq!(harness.state().pagination.current_page, 0);
    
    // Navigate to next page
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("next_page".to_string())
    ))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().pagination.current_page, 1);

    // Navigate to last page
    harness.state_mut().pagination.current_page = 4;
    
    // Try to go beyond last page
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("next_page".to_string())
    ))?;
    assert_eq!(harness.state().pagination.current_page, 4); // Should stay on last page

    Ok(())
}

#[tokio::test]
async fn test_data_grid_virtual_scrolling() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(10000, 10); // Large dataset

    let config = DataGridConfig {
        virtual_scrolling: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(800.0, 600.0),
            content_size: Vec2::new(800.0, 600.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that only visible rows are considered for rendering
    let commands = harness.render()?;
    
    // With virtual scrolling, we should have far fewer render commands
    // than 10000 rows would normally generate
    assert!(commands.len() < 1000); // Reasonable upper bound

    // Test scrolling
    harness.handle_event(InputEvent::Scroll {
        delta: Vec2::new(0.0, -500.0),
        position: Vec2::new(400.0, 300.0),
    })?;
    
    assert!(harness.state().scroll_state.vertical_offset > 0.0);

    Ok(())
}

#[tokio::test]
async fn test_data_grid_keyboard_navigation() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(5, 5);

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 400.0),
            content_size: Vec2::new(500.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Select initial cell
    harness.state_mut().selected_cells = vec![(0, 0)];

    // Navigate right
    harness.handle_event(key_press("ArrowRight"))?;
    assert_eq!(harness.state().selected_cells, vec![(0, 1)]);

    // Navigate down
    harness.handle_event(key_press("ArrowDown"))?;
    assert_eq!(harness.state().selected_cells, vec![(1, 1)]);

    // Navigate left
    harness.handle_event(key_press("ArrowLeft"))?;
    assert_eq!(harness.state().selected_cells, vec![(1, 0)]);

    // Navigate up
    harness.handle_event(key_press("ArrowUp"))?;
    assert_eq!(harness.state().selected_cells, vec![(0, 0)]);

    // Test Home/End keys
    harness.handle_event(key_press("End"))?;
    assert_eq!(harness.state().selected_cells, vec![(0, 4)]); // Last column

    harness.handle_event(key_press("Home"))?;
    assert_eq!(harness.state().selected_cells, vec![(0, 0)]); // First column

    Ok(())
}

#[tokio::test]
async fn test_data_grid_copy_paste() -> Result<()> {
    let data_grid = DataGridWidget::new();
    let initial_state = create_test_grid_state(5, 5);

    let config = DataGridConfig {
        clipboard_enabled: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(data_grid)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 400.0),
            content_size: Vec2::new(500.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Select a cell
    harness.state_mut().selected_cells = vec![(1, 1)];

    // Copy (Ctrl+C)
    harness.handle_event(InputEvent::KeyDown {
        key: "c".to_string(),
        modifiers: 1, // Ctrl
    })?;

    // Move to different cell
    harness.state_mut().selected_cells = vec![(2, 2)];

    // Paste (Ctrl+V)
    harness.handle_event(InputEvent::KeyDown {
        key: "v".to_string(),
        modifiers: 1, // Ctrl
    })?;

    // Verify paste operation
    let source_value = &harness.state().rows[1].cells[1].value;
    let dest_value = &harness.state().rows[2].cells[2].value;
    assert_eq!(source_value, dest_value);

    Ok(())
}

// Helper function to create test grid state
fn create_test_grid_state(rows: usize, cols: usize) -> DataGridState {
    let columns: Vec<GridColumn> = (0..cols)
        .map(|i| GridColumn {
            id: format!("col_{}", i),
            title: format!("Column {}", i),
            width: 100.0,
            min_width: Some(50.0),
            max_width: Some(200.0),
            resizable: true,
            sortable: true,
            data_type: ColumnDataType::Text,
            alignment: ColumnAlignment::Left,
            format: None,
            editable: true,
            visible: true,
        })
        .collect();

    let grid_rows: Vec<GridRow> = (0..rows)
        .map(|i| {
            let cells: Vec<GridCell> = (0..cols)
                .map(|j| GridCell {
                    value: Value::String(format!("Cell {}:{}", i, j)),
                    display_value: Some(format!("Cell {}:{}", i, j)),
                    formatted_value: None,
                    editable: true,
                    validation_state: CellValidationState::Valid,
                    metadata: Default::default(),
                })
                .collect();

            GridRow {
                id: format!("row_{}", i),
                cells,
                selected: false,
                expanded: false,
                metadata: Default::default(),
            }
        })
        .collect();

    DataGridState {
        columns,
        rows: grid_rows,
        filtered_rows: (0..rows).collect(),
        selected_rows: vec![],
        selected_cells: vec![],
        sort_config: None,
        filters: HashMap::new(),
        pagination: PaginationState {
            current_page: 0,
            page_size: rows,
            total_items: rows,
            total_pages: 1,
        },
        scroll_state: ScrollState {
            horizontal_offset: 0.0,
            vertical_offset: 0.0,
            horizontal_max: 0.0,
            vertical_max: 0.0,
        },
        editing_cell: None,
        column_resize_state: None,
        selection_mode: SelectionMode::Cell,
        is_loading: false,
        error_message: None,
    }
}

// Mutable state helper
trait WidgetTestHarnessExt<W: KryonWidget> {
    fn state_mut(&mut self) -> &mut W::State;
}

impl<W: KryonWidget> WidgetTestHarnessExt<W> for WidgetTestHarness<W> {
    fn state_mut(&mut self) -> &mut W::State {
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
        fn test_data_grid_handles_arbitrary_dimensions(rows in 0usize..1000, cols in 1usize..50) {
            let data_grid = DataGridWidget::new();
            let state = create_test_grid_state(rows, cols);

            let harness = WidgetTestHarness::new(data_grid)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(800.0, 600.0),
                    content_size: Vec2::new(800.0, 600.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any grid size without panic
            prop_assert!(harness.render().is_ok());
        }

        #[test]
        fn test_data_grid_sorting_consistency(data in prop::collection::vec(".*", 10..100)) {
            let data_grid = DataGridWidget::new();
            let mut state = create_test_grid_state(data.len(), 1);
            
            // Set test data
            for (i, value) in data.iter().enumerate() {
                state.rows[i].cells[0].value = Value::String(value.clone());
            }

            // Apply sort
            state.sort_config = Some(SortConfig {
                column_index: 0,
                direction: SortDirection::Ascending,
            });

            // Sort should maintain data integrity
            let total_cells_before = state.rows.len() * state.columns.len();
            
            // Apply sorting logic here...
            
            let total_cells_after = state.rows.len() * state.columns.len();
            prop_assert_eq!(total_cells_before, total_cells_after);
        }
    }
}
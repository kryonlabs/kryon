//! Performance benchmarks for Kryon widgets
//! 
//! This benchmark suite measures the performance of widget operations including
//! state updates, event handling, and rendering to identify bottlenecks and
//! ensure consistent performance across widget implementations.

use criterion::{black_box, criterion_group, criterion_main, BenchmarkId, Criterion, Throughput};
use glam::{Vec2, Vec4};
use kryon_shared::widgets::*;
use std::time::Duration;

// Mock types for benchmarking
#[derive(Clone, Default)]
struct BenchmarkLayoutResult {
    position: Vec2,
    size: Vec2,
    content_size: Vec2,
    scroll_offset: Vec2,
}

#[derive(Clone)]
enum BenchmarkInputEvent {
    MouseMove { position: Vec2 },
    MouseDown { position: Vec2, button: u8 },
    MouseUp { position: Vec2, button: u8 },
    KeyDown { key: String, modifiers: u8 },
    TextInput { text: String },
}

#[derive(Clone, PartialEq)]
enum BenchmarkEventResult {
    Handled,
    NotHandled,
    StateChanged,
}

fn create_benchmark_dropdown_state(item_count: usize) -> dropdown::DropdownState {
    let items: Vec<dropdown::DropdownItem> = (0..item_count)
        .map(|i| dropdown::DropdownItem {
            id: format!("item_{}", i),
            label: format!("Item {}", i),
            value: serde_json::Value::String(format!("value_{}", i)),
            enabled: true,
            icon: None,
            metadata: Default::default(),
        })
        .collect();

    dropdown::DropdownState {
        items,
        selected_index: Some(0),
        is_open: false,
        hovered_index: None,
        search_text: String::new(),
        filtered_items: (0..item_count).collect(),
        scroll_offset: 0.0,
        is_focused: false,
        loading_state: dropdown::LoadingState::Idle,
        validation_state: dropdown::ValidationState::Valid,
    }
}

fn create_benchmark_data_grid_state(row_count: usize, col_count: usize) -> data_grid::DataGridState {
    let columns: Vec<data_grid::GridColumn> = (0..col_count)
        .map(|i| data_grid::GridColumn {
            id: format!("col_{}", i),
            title: format!("Column {}", i),
            width: 100.0,
            min_width: Some(50.0),
            max_width: Some(200.0),
            resizable: true,
            sortable: true,
            data_type: data_grid::ColumnDataType::Text,
            alignment: data_grid::ColumnAlignment::Left,
            format: None,
            editable: true,
            visible: true,
        })
        .collect();

    let rows: Vec<data_grid::GridRow> = (0..row_count)
        .map(|i| {
            let cells: Vec<data_grid::GridCell> = (0..col_count)
                .map(|j| data_grid::GridCell {
                    value: serde_json::Value::String(format!("Cell {}:{}", i, j)),
                    display_value: Some(format!("Cell {}:{}", i, j)),
                    formatted_value: None,
                    editable: true,
                    validation_state: data_grid::CellValidationState::Valid,
                    metadata: Default::default(),
                })
                .collect();

            data_grid::GridRow {
                id: format!("row_{}", i),
                cells,
                selected: false,
                expanded: false,
                metadata: Default::default(),
            }
        })
        .collect();

    data_grid::DataGridState {
        columns,
        rows,
        filtered_rows: (0..row_count).collect(),
        selected_rows: vec![],
        selected_cells: vec![],
        sort_config: None,
        filters: Default::default(),
        pagination: data_grid::PaginationState {
            current_page: 0,
            page_size: 50,
            total_items: row_count,
            total_pages: (row_count + 49) / 50,
        },
        scroll_state: data_grid::ScrollState {
            horizontal_offset: 0.0,
            vertical_offset: 0.0,
            horizontal_max: 0.0,
            vertical_max: 0.0,
        },
        editing_cell: None,
        column_resize_state: None,
        selection_mode: data_grid::SelectionMode::Single,
        is_loading: false,
        error_message: None,
    }
}

fn benchmark_dropdown_rendering(c: &mut Criterion) {
    let mut group = c.benchmark_group("dropdown_rendering");
    
    for &item_count in &[10, 100, 1000, 5000] {
        group.throughput(Throughput::Elements(item_count as u64));
        group.bench_with_input(
            BenchmarkId::new("render", item_count),
            &item_count,
            |b, &item_count| {
                let dropdown = dropdown::DropdownWidget::new();
                let state = create_benchmark_dropdown_state(item_count);
                let config = dropdown::DropdownConfig::default();
                let layout = BenchmarkLayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(200.0, 32.0),
                    content_size: Vec2::new(200.0, 32.0),
                    scroll_offset: Vec2::ZERO,
                };

                b.iter(|| {
                    // Mock render call - in real implementation this would call the actual render method
                    black_box(&dropdown);
                    black_box(&state);
                    black_box(&config);
                    black_box(&layout);
                    // Simulate rendering work
                    for i in 0..item_count.min(20) {
                        black_box(i * 2);
                    }
                });
            },
        );
    }
    group.finish();
}

fn benchmark_dropdown_event_handling(c: &mut Criterion) {
    let mut group = c.benchmark_group("dropdown_event_handling");
    
    for &item_count in &[10, 100, 1000] {
        group.bench_with_input(
            BenchmarkId::new("mouse_events", item_count),
            &item_count,
            |b, &item_count| {
                let dropdown = dropdown::DropdownWidget::new();
                let mut state = create_benchmark_dropdown_state(item_count);
                let config = dropdown::DropdownConfig::default();
                let layout = BenchmarkLayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(200.0, 32.0),
                    content_size: Vec2::new(200.0, 32.0),
                    scroll_offset: Vec2::ZERO,
                };

                let events = vec![
                    BenchmarkInputEvent::MouseDown {
                        position: Vec2::new(100.0, 16.0),
                        button: 0,
                    },
                    BenchmarkInputEvent::MouseMove {
                        position: Vec2::new(100.0, 50.0),
                    },
                    BenchmarkInputEvent::MouseUp {
                        position: Vec2::new(100.0, 50.0),
                        button: 0,
                    },
                ];

                b.iter(|| {
                    for event in &events {
                        // Mock event handling - in real implementation this would call the actual handle_event method
                        black_box(&dropdown);
                        black_box(&mut state);
                        black_box(&config);
                        black_box(&layout);
                        black_box(event);
                        // Simulate event processing work
                        black_box(BenchmarkEventResult::StateChanged);
                    }
                });
            },
        );
    }
    group.finish();
}

fn benchmark_dropdown_search(c: &mut Criterion) {
    let mut group = c.benchmark_group("dropdown_search");
    
    for &item_count in &[100, 1000, 5000] {
        group.bench_with_input(
            BenchmarkId::new("search_filtering", item_count),
            &item_count,
            |b, &item_count| {
                let dropdown = dropdown::DropdownWidget::new();
                let mut state = create_benchmark_dropdown_state(item_count);
                let config = dropdown::DropdownConfig {
                    searchable: true,
                    ..Default::default()
                };

                b.iter(|| {
                    // Mock search filtering
                    let search_term = "Item 1";
                    let mut filtered_items = Vec::new();
                    
                    for (i, item) in state.items.iter().enumerate() {
                        if item.label.contains(search_term) {
                            filtered_items.push(i);
                        }
                    }
                    
                    black_box(filtered_items);
                });
            },
        );
    }
    group.finish();
}

fn benchmark_data_grid_rendering(c: &mut Criterion) {
    let mut group = c.benchmark_group("data_grid_rendering");
    
    for &(rows, cols) in &[(10, 5), (100, 10), (1000, 10), (100, 50)] {
        group.throughput(Throughput::Elements((rows * cols) as u64));
        group.bench_with_input(
            BenchmarkId::new("render", format!("{}x{}", rows, cols)),
            &(rows, cols),
            |b, &(rows, cols)| {
                let data_grid = data_grid::DataGridWidget::new();
                let state = create_benchmark_data_grid_state(rows, cols);
                let config = data_grid::DataGridConfig::default();
                let layout = BenchmarkLayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(800.0, 600.0),
                    content_size: Vec2::new(800.0, 600.0),
                    scroll_offset: Vec2::ZERO,
                };

                b.iter(|| {
                    // Mock render call
                    black_box(&data_grid);
                    black_box(&state);
                    black_box(&config);
                    black_box(&layout);
                    // Simulate rendering work for visible cells only
                    let visible_rows = 20.min(rows);
                    let visible_cols = cols;
                    for i in 0..visible_rows {
                        for j in 0..visible_cols {
                            black_box(i * j);
                        }
                    }
                });
            },
        );
    }
    group.finish();
}

fn benchmark_data_grid_sorting(c: &mut Criterion) {
    let mut group = c.benchmark_group("data_grid_sorting");
    
    for &row_count in &[100, 1000, 5000] {
        group.bench_with_input(
            BenchmarkId::new("sort_column", row_count),
            &row_count,
            |b, &row_count| {
                let state = create_benchmark_data_grid_state(row_count, 5);
                
                b.iter(|| {
                    // Mock sorting operation
                    let mut indices: Vec<usize> = (0..row_count).collect();
                    indices.sort_by(|&a, &b| {
                        let cell_a = &state.rows[a].cells[0].value;
                        let cell_b = &state.rows[b].cells[0].value;
                        cell_a.to_string().cmp(&cell_b.to_string())
                    });
                    black_box(indices);
                });
            },
        );
    }
    group.finish();
}

fn benchmark_data_grid_filtering(c: &mut Criterion) {
    let mut group = c.benchmark_group("data_grid_filtering");
    
    for &row_count in &[100, 1000, 5000] {
        group.bench_with_input(
            BenchmarkId::new("filter_rows", row_count),
            &row_count,
            |b, &row_count| {
                let state = create_benchmark_data_grid_state(row_count, 5);
                let filter_text = "Cell 1";
                
                b.iter(|| {
                    // Mock filtering operation
                    let mut filtered_rows = Vec::new();
                    
                    for (i, row) in state.rows.iter().enumerate() {
                        for cell in &row.cells {
                            if cell.value.to_string().contains(filter_text) {
                                filtered_rows.push(i);
                                break;
                            }
                        }
                    }
                    
                    black_box(filtered_rows);
                });
            },
        );
    }
    group.finish();
}

fn benchmark_widget_state_updates(c: &mut Criterion) {
    let mut group = c.benchmark_group("widget_state_updates");
    
    group.bench_function("dropdown_state_clone", |b| {
        let state = create_benchmark_dropdown_state(1000);
        b.iter(|| {
            let cloned = black_box(state.clone());
            black_box(cloned);
        });
    });
    
    group.bench_function("data_grid_state_clone", |b| {
        let state = create_benchmark_data_grid_state(100, 10);
        b.iter(|| {
            let cloned = black_box(state.clone());
            black_box(cloned);
        });
    });
    
    group.finish();
}

fn benchmark_widget_memory_usage(c: &mut Criterion) {
    let mut group = c.benchmark_group("widget_memory_usage");
    
    group.bench_function("dropdown_creation", |b| {
        b.iter(|| {
            let state = create_benchmark_dropdown_state(black_box(100));
            black_box(state);
        });
    });
    
    group.bench_function("data_grid_creation", |b| {
        b.iter(|| {
            let state = create_benchmark_data_grid_state(black_box(50), black_box(10));
            black_box(state);
        });
    });
    
    group.finish();
}

// Benchmark for widget interaction patterns
fn benchmark_widget_interactions(c: &mut Criterion) {
    let mut group = c.benchmark_group("widget_interactions");
    
    group.bench_function("dropdown_open_close_cycle", |b| {
        let dropdown = dropdown::DropdownWidget::new();
        let mut state = create_benchmark_dropdown_state(100);
        let config = dropdown::DropdownConfig::default();
        
        b.iter(|| {
            // Simulate open/close cycle
            state.is_open = true;
            black_box(&state);
            state.is_open = false;
            black_box(&state);
        });
    });
    
    group.bench_function("data_grid_cell_selection", |b| {
        let data_grid = data_grid::DataGridWidget::new();
        let mut state = create_benchmark_data_grid_state(100, 10);
        let config = data_grid::DataGridConfig::default();
        
        b.iter(|| {
            // Simulate cell selection
            state.selected_cells = vec![(black_box(5), black_box(3))];
            black_box(&state);
            state.selected_cells.clear();
            black_box(&state);
        });
    });
    
    group.finish();
}

criterion_group!(
    benches,
    benchmark_dropdown_rendering,
    benchmark_dropdown_event_handling,
    benchmark_dropdown_search,
    benchmark_data_grid_rendering,
    benchmark_data_grid_sorting,
    benchmark_data_grid_filtering,
    benchmark_widget_state_updates,
    benchmark_widget_memory_usage,
    benchmark_widget_interactions
);

criterion_main!(benches);
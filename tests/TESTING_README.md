# Kryon Testing Framework Documentation

## Overview

The Kryon testing framework provides a comprehensive, organized testing infrastructure for the entire Kryon UI framework. It includes unit tests, widget tests, integration tests, performance benchmarks, and visual regression testing capabilities.

## Architecture

```
kryon/tests/
├── Cargo.toml                    # Workspace configuration
├── test-runner.toml              # Centralized test configuration
├── run_comprehensive_tests.sh    # Main test runner script
├── kryon-tests/                  # Core unit tests
├── widget-tests/                 # Widget-specific tests
├── integration-tests/            # Cross-component integration tests
├── performance-tests/            # Performance benchmarks
└── fixtures/                     # Test data and fixtures
```

## Quick Start

### Running All Tests

```bash
# Run comprehensive test suite
./tests/run_comprehensive_tests.sh --all

# Run with verbose output
./tests/run_comprehensive_tests.sh --all --verbose
```

### Running Specific Test Categories

```bash
# Unit tests only
./tests/run_comprehensive_tests.sh --unit-only

# Widget tests only
./tests/run_comprehensive_tests.sh --widget-only

# Integration tests only
./tests/run_comprehensive_tests.sh --integration-only

# Include performance benchmarks
./tests/run_comprehensive_tests.sh --performance

# Include visual regression tests
./tests/run_comprehensive_tests.sh --visual
```

### Running Individual Test Suites

```bash
# Run specific widget tests
cd tests/widget-tests
cargo test dropdown_widget_tests --features dropdown

# Run specific integration tests
cd tests/integration-tests
cargo test cross_backend_tests

# Run performance benchmarks
cd tests/performance-tests
cargo bench widget_benchmarks
```

## Widget Testing

The widget testing framework provides comprehensive testing utilities for all Kryon widgets.

### Widget Test Harness

```rust
use widget_tests::*;

let dropdown = DropdownWidget::new();
let mut harness = WidgetTestHarness::new(dropdown)
    .with_state(initial_state)
    .with_config(config)
    .with_layout(layout);

// Test rendering
harness.assert_renders_successfully()?;

// Test event handling
let result = harness.handle_event(mouse_click(100.0, 50.0))?;
assert_eq!(result, EventResult::StateChanged);

// Test state
assert_widget_state!(harness, is_open == true);
```

### Available Widget Tests

- **Dropdown**: Selection, search, multi-select, async loading
- **Data Grid**: Sorting, filtering, editing, virtual scrolling
- **Date Picker**: Date selection, navigation, validation
- **Rich Text**: Text editing, formatting, import/export
- **Color Picker**: Color selection, format conversion
- **File Upload**: Drag-drop, validation, progress tracking
- **Number Input**: Formatting, validation, step controls

### Mock Backend

The testing framework includes a mock rendering backend for capturing and verifying render commands:

```rust
let mut backend = MockRenderBackend::new(800.0, 600.0);
backend.submit_commands(render_commands);

let frame_result = backend.render_frame();
assert_eq!(frame_result.draw_calls, expected_count);
```

## Integration Testing

Integration tests verify the complete Kryon pipeline from source to rendered output.

### Test Harness

```rust
let mut harness = IntegrationTestHarness::new()?;
harness.add_source_file("test.kry", source_content);
harness.compile_all()?;

// Test with specific backend
let result = harness.run_with_backend("test.kry", TestBackend::Html)?;
assert!(result.success);

// Cross-backend testing
let cross_result = harness.cross_backend_test("test.kry")?;
assert!(cross_result.consistent);
```

### Test Categories

1. **Compilation Pipeline**: KRY → AST → IR → KRB transformation
2. **Rendering Pipeline**: Element tree → Layout → Styling → Commands
3. **Cross-Backend**: Consistency across Ratatui, HTML, Raylib, WGPU
4. **End-to-End**: Complete user workflows with interaction

## Performance Testing

Performance benchmarks ensure Kryon maintains optimal performance.

### Benchmark Categories

- **Widget Rendering**: Measures render performance with varying data sizes
- **Event Handling**: Tests event processing latency
- **State Updates**: Benchmarks state management efficiency
- **Memory Usage**: Tracks memory consumption patterns

### Running Benchmarks

```bash
# Run all benchmarks
cd tests/performance-tests
cargo bench

# Run specific benchmark suite
cargo bench widget_benchmarks

# Generate HTML reports
cargo bench -- --output-format html
```

### Performance Thresholds

The test configuration defines performance limits:

```toml
[performance_tests]
max_compilation_time_ms = 500
max_render_time_ms = 16  # 60 FPS target
max_memory_usage_mb = 100
```

## Test Configuration

The `test-runner.toml` file provides centralized configuration:

```toml
[general]
parallel_execution = true
max_parallel_jobs = 8
timeout_seconds = 300
generate_reports = true

[widget_tests]
test_features = ["dropdown", "data-grid", "date-picker"]

[coverage]
minimum_line_coverage = 80
minimum_branch_coverage = 70
```

## Custom Assertions

The framework provides specialized assertions for widget testing:

```rust
// Widget state assertions
assert_widget_state!(harness, is_open == true);
assert_widget_state!(harness, selected_index != None);

// Render command assertions
assert_render_commands!(commands, contains DrawRect);
assert_render_commands!(commands, contains DrawText with "Hello");
assert_render_commands!(commands, count == 5);

// Event handling assertions
assert_event_handled!(result);
assert_event_handled!(result, StateChanged);

// Layout assertions
LayoutAssertions::assert_within_bounds(position, size, canvas_size)?;
LayoutAssertions::assert_no_overlap(&elements)?;
```

## Test Data Generation

The framework includes utilities for generating test data:

```rust
let mut generator = TestDataGenerator::new(seed);

// Generate various data types
let text = generator.generate_text(10, 50);
let email = generator.generate_email();
let date = generator.generate_date();
let json = generator.generate_json_value(depth);

// Use predefined fixtures
let dropdown_items = WidgetTestFixtures::dropdown_items();
let grid_data = WidgetTestFixtures::data_grid_sample_data();
```

## Continuous Integration

### Pre-commit Testing

```bash
# Quick tests for development
./tests/run_comprehensive_tests.sh --unit-only --fail-fast
```

### Pull Request Testing

```bash
# Full test suite
./tests/run_comprehensive_tests.sh --all --generate-reports
```

### Nightly Testing

```bash
# Extended testing with performance and visual tests
./tests/run_comprehensive_tests.sh --all --performance --visual
```

## Visual Regression Testing

Visual tests ensure UI consistency across changes:

```bash
# Run visual tests
./tests/run_comprehensive_tests.sh --visual

# Update snapshots after intentional changes
./tests/run_comprehensive_tests.sh --visual --update-snapshots
```

## Test Reports

The framework generates comprehensive test reports:

- **Console Output**: Colored terminal output with progress
- **JSON Reports**: Machine-readable test results
- **HTML Reports**: Interactive dashboards with graphs
- **JUnit XML**: CI/CD integration format

Reports are saved to `tests/reports/` with timestamps.

## Debugging Failed Tests

### Verbose Output

```bash
# Run with detailed output
./tests/run_comprehensive_tests.sh --verbose

# Run specific test with output
cargo test test_name -- --nocapture
```

### Test Isolation

```bash
# Run single test
cargo test specific_test_name --exact

# Run tests from specific file
cargo test --test widget_tests dropdown
```

### Performance Profiling

```bash
# Run with profiling
CARGO_PROFILE_BENCH_DEBUG=true cargo bench

# Analyze with perf
perf record cargo bench
perf report
```

## Writing New Tests

### Widget Test Template

```rust
#[tokio::test]
async fn test_widget_feature() -> Result<()> {
    // Setup
    let widget = MyWidget::new();
    let initial_state = create_test_state();
    let config = WidgetConfig::default();
    
    let mut harness = WidgetTestHarness::new(widget)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(test_layout());

    // Test rendering
    harness.assert_renders_successfully()?;
    
    // Test interaction
    let result = harness.handle_event(test_event())?;
    assert_event_handled!(result, StateChanged);
    
    // Verify state
    assert_widget_state!(harness, expected_field == expected_value);
    
    Ok(())
}
```

### Integration Test Template

```rust
#[test]
fn test_integration_scenario() -> Result<()> {
    let mut harness = IntegrationTestHarness::new()?;
    
    // Add source files
    harness.add_source_file("app.kry", include_str!("fixtures/app.kry"));
    
    // Compile
    harness.compile_all()?;
    
    // Test execution
    let result = harness.run_with_backend("app.kry", TestBackend::Html)?;
    assert!(result.success);
    
    Ok(())
}
```

## Best Practices

1. **Test Organization**
   - Group related tests in modules
   - Use descriptive test names
   - Keep tests focused and independent

2. **Performance**
   - Use `cargo test --release` for performance-sensitive tests
   - Mock expensive operations
   - Parallelize independent tests

3. **Maintainability**
   - Use test fixtures for common data
   - Extract helper functions
   - Document complex test scenarios

4. **Coverage**
   - Aim for >80% line coverage
   - Test edge cases and error paths
   - Include property-based tests

## Troubleshooting

### Common Issues

**Tests timing out**
- Increase timeout in `test-runner.toml`
- Check for infinite loops or deadlocks
- Use `--no-parallel` for debugging

**Flaky tests**
- Add deterministic seeds to random generators
- Avoid time-dependent assertions
- Mock external dependencies

**Performance regressions**
- Compare benchmark results over time
- Profile specific operations
- Check for memory leaks

## Contributing

When adding new tests:

1. Choose appropriate test category
2. Follow existing patterns and conventions
3. Add documentation for complex scenarios
4. Update this README if adding new features
5. Ensure tests pass in CI before merging

For questions or issues, please refer to the main Kryon documentation or open an issue on GitHub.
# Kryon Testing Guide

A comprehensive guide for testing the Kryon UI framework, covering unit tests, integration tests, and the centralized test suite.

## 🎯 Testing Philosophy

Kryon's testing approach is built on three pillars:

1. **Comprehensive Coverage**: Every component, renderer, and feature is thoroughly tested
2. **Cross-Platform Consistency**: Tests ensure behavior is consistent across different renderers and platforms
3. **Performance Awareness**: All tests include performance considerations and regression detection

## 📚 Test Categories

### 1. Unit Tests
**Location**: Individual crate `src/` directories
**Purpose**: Test individual components and functions in isolation

```bash
# Run unit tests for specific crates
cargo test -p kryon-core
cargo test -p kryon-layout
cargo test -p kryon-render
cargo test -p kryon-runtime
```

### 2. Integration Tests
**Location**: `tests/` directory and `kryon-tests` crate
**Purpose**: Test interaction between components and end-to-end workflows

```bash
# Run integration tests
cargo test -p kryon-tests
cargo test --test integration
```

### 3. Test Suite Applications
**Location**: `tests/test_suites/`
**Purpose**: Real-world application testing with visual validation

```bash
# Run the complete test suite
./tests/run_test_suite.sh
```

## 🧪 Test Suite Applications

### Core Components Test (01_core_components.kry)

**What it tests**:
- Basic UI elements (Text, Button, Container)
- Event handling and state management
- Template variable binding
- Layout fundamentals

**How to run**:
```bash
# Compile
cargo run --bin kryon-compiler -- compile tests/test_suites/01_core_components.kry tests/test_suites/01_core_components.krb

# Run with different renderers
cargo run --bin kryon-renderer-sdl2 -- tests/test_suites/01_core_components.krb
cargo run --bin kryon-renderer-wgpu -- tests/test_suites/01_core_components.krb
```

**Interactive tests**:
- Click buttons to test event handling
- Observe counter updates for state management
- Verify text changes for template binding

### Input Fields Test (02_input_fields.kry)

**What it tests**:
- All input field types (text, password, email, number)
- Form controls (checkbox, radio, dropdown, slider)
- Focus management and keyboard navigation
- Form validation and data handling

**How to run**:
```bash
cargo run --bin kryon-compiler -- compile tests/test_suites/02_input_fields.kry tests/test_suites/02_input_fields.krb
cargo run --bin kryon-renderer-sdl2 -- tests/test_suites/02_input_fields.krb
```

**Interactive tests**:
- Type in various input fields
- Use Tab to navigate between fields
- Test form validation (email format, number ranges)
- Export form data to verify collection

### Layout Engine Test (03_layout_engine.kry)

**What it tests**:
- Flexbox layout system
- Justify content and align items
- Gap sizing and container dimensions
- Nested layout hierarchies

**How to run**:
```bash
cargo run --bin kryon-compiler -- compile tests/test_suites/03_layout_engine.kry tests/test_suites/03_layout_engine.krb
cargo run --bin kryon-renderer-sdl2 -- tests/test_suites/03_layout_engine.krb
```

**Interactive tests**:
- Change flex direction and observe layout changes
- Test different justify-content values
- Adjust gap sizes and container dimensions
- Measure layout performance

### Renderer Comparison Test (04_renderer_comparison.kry)

**What it tests**:
- Rendering consistency across backends
- Color accuracy and alpha blending
- Typography rendering quality
- Layout precision and border rendering

**How to run**:
```bash
# Compile once
cargo run --bin kryon-compiler -- compile tests/test_suites/04_renderer_comparison.kry tests/test_suites/04_renderer_comparison.krb

# Test with multiple renderers for comparison
cargo run --bin kryon-renderer-sdl2 -- tests/test_suites/04_renderer_comparison.krb
cargo run --bin kryon-renderer-wgpu -- tests/test_suites/04_renderer_comparison.krb
cargo run --bin kryon-renderer-ratatui -- tests/test_suites/04_renderer_comparison.krb
```

**Visual validation**:
- Compare color rendering between renderers
- Verify typography consistency
- Check layout precision alignment
- Test alpha blending and transparency

### Performance Benchmark Test (05_performance_benchmark.kry)

**What it tests**:
- Element rendering performance scaling
- Memory usage and CPU consumption
- Layout computation performance
- Stress testing with many elements

**How to run**:
```bash
cargo run --bin kryon-compiler -- compile tests/test_suites/05_performance_benchmark.kry tests/test_suites/05_performance_benchmark.krb
cargo run --bin kryon-renderer-sdl2 -- tests/test_suites/05_performance_benchmark.krb
```

**Performance tests**:
- Start stress tests with increasing element counts
- Monitor FPS and resource usage
- Test layout complexity scaling
- Export performance metrics

## 🛠️ Writing Tests

### Unit Test Example

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_element_creation() {
        let element = Element::new(ElementType::Text);
        assert_eq!(element.element_type, ElementType::Text);
        assert_eq!(element.visible, true);
    }

    #[test]
    fn test_layout_computation() {
        let mut engine = TaffyLayoutEngine::new();
        let elements = create_test_elements();
        let result = engine.compute_layout(&elements, 1, Vec2::new(800.0, 600.0));
        
        assert!(result.computed_positions.contains_key(&1));
        assert!(result.computed_sizes.contains_key(&1));
    }
}
```

### Integration Test Example

```rust
#[tokio::test]
async fn test_full_rendering_pipeline() {
    // Create test KRY content
    let kry_content = r#"
        App {
            window_width: 400
            window_height: 300
            Text { text: "Test" }
        }
    "#;
    
    // Compile to KRB
    let krb_file = compile_kry_content(kry_content).await?;
    
    // Create renderer and app
    let renderer = TestRenderer::new();
    let mut app = KryonApp::new_with_krb(krb_file, renderer, None)?;
    
    // Test rendering
    app.render()?;
    
    // Validate results
    let commands = app.renderer().get_last_commands();
    assert!(!commands.is_empty());
}
```

### Test Suite Application Structure

```kry
# Test metadata and styling
style "test_container" {
    # Styling for test UI
}

# Test variables
@variables {
    test_state: "initial"
    counter: 0
}

# UI definition
App {
    window_width: 800
    window_height: 600
    window_title: "Test Name"
    
    Container {
        # Test UI elements
        Button {
            text: "Test Button"
            onClick: "testFunction"
        }
    }
}

# Test functions
@function "lua" testFunction() {
    # Test implementation
    counter = counter + 1
    test_state = "tested"
    print("Test executed: " .. counter)
}

# Initialization
@init {
    print("Test suite initialized")
}
```

## 🔍 Test Validation

### Visual Validation Checklist

When running test suites, verify:

1. **Layout Accuracy**:
   - Elements are positioned correctly
   - Containers have proper sizing
   - Flexbox behaviors work as expected

2. **Interaction Responsiveness**:
   - Buttons respond to clicks
   - Hover states work correctly
   - Input fields accept and display text

3. **Visual Consistency**:
   - Colors render accurately
   - Fonts display clearly
   - Borders and shapes are crisp

4. **Performance Smoothness**:
   - UI remains responsive under load
   - Animations are smooth (when supported)
   - No visible lag or stuttering

### Automated Validation

```bash
# Run comprehensive validation
./tests/run_test_suite.sh --bench

# Check for specific issues
cargo test --workspace
cargo check --workspace
cargo clippy --workspace
```

## 📊 Performance Testing

### Benchmark Categories

1. **Element Count Scaling**:
   - Test with 50, 100, 200, 500, 1000+ elements
   - Measure FPS degradation
   - Identify performance bottlenecks

2. **Layout Complexity**:
   - Simple flat layouts vs. deeply nested
   - Flexbox vs. absolute positioning
   - Dynamic layout changes

3. **Memory Usage**:
   - Element allocation patterns
   - Garbage collection impact
   - Memory leak detection

4. **Renderer Comparison**:
   - SDL2 vs. WGPU performance
   - CPU vs. GPU utilization
   - Platform-specific optimizations

### Performance Thresholds

- **Minimum FPS**: 30 (target: 60)
- **Maximum Memory**: 500MB for 1000 elements
- **Maximum CPU**: 80% during stress tests
- **Layout Time**: <5ms for complex layouts

## 🚨 Troubleshooting

### Common Test Issues

1. **Compilation Failures**:
   ```bash
   # Check KRY syntax
   cargo run --bin kryon-compiler -- compile tests/test_suites/test.kry tests/test_suites/test.krb --verbose
   ```

2. **Renderer Not Available**:
   ```bash
   # Build all renderers
   cargo build --workspace
   # Check binary exists
   ls target/debug/kryon-renderer-*
   ```

3. **Performance Issues**:
   ```bash
   # Use release build for performance testing
   cargo build --release
   ./target/release/kryon-renderer-sdl2 tests/test_suites/05_performance_benchmark.krb
   ```

4. **Test Timeouts**:
   - Increase timeout in test configuration
   - Check for infinite loops in Lua scripts
   - Verify system resources

### Debug Mode

```bash
# Enable debug logging
RUST_LOG=debug ./tests/run_test_suite.sh

# Run with debug renderer
cargo run --bin kryon-debug-interactive -- tests/test_suites/test.krb
```

## 🔧 Test Configuration

### Environment Variables

```bash
export KRYON_TEST_RENDERER=sdl2    # Default renderer for tests
export KRYON_TEST_TIMEOUT=60       # Test timeout in seconds
export KRYON_LOG_LEVEL=INFO        # Logging level
export KRYON_HEADLESS=false        # Run tests with display
```

### Configuration File

Edit `tests/test_config.toml` to customize:
- Renderer availability
- Performance thresholds
- Test timeouts
- Quality check settings

## 📈 Continuous Integration

### CI Test Strategy

1. **Fast Feedback Loop**:
   ```bash
   # Quick tests (unit + integration)
   ./tests/run_test_suite.sh --unit-only --integration-only
   ```

2. **Comprehensive Testing**:
   ```bash
   # Full test suite with benchmarks
   ./tests/run_test_suite.sh --bench
   ```

3. **Quality Gates**:
   ```bash
   # Code quality checks
   cargo fmt --check
   cargo clippy --workspace -- -D warnings
   cargo check --workspace
   ```

### GitHub Actions Example

```yaml
name: Test Suite
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions-rs/toolchain@v1
        with:
          toolchain: stable
      
      - name: Run Test Suite
        run: ./tests/run_test_suite.sh --no-quality
      
      - name: Upload Test Results
        uses: actions/upload-artifact@v3
        with:
          name: test-results
          path: tests/results/
```

## 🤝 Contributing Tests

### Adding New Tests

1. **Create Test File**: Add new `.kry` file in appropriate directory
2. **Follow Conventions**: Use established naming and structure patterns
3. **Document Purpose**: Clear description of what the test validates
4. **Include Interactivity**: Add Lua functions for user interaction
5. **Test All Renderers**: Ensure compatibility across backends

### Test Review Checklist

- [ ] Test covers all relevant functionality
- [ ] Works with all supported renderers
- [ ] Includes performance considerations
- [ ] Has clear documentation
- [ ] Follows established patterns
- [ ] Includes error handling
- [ ] Validates expected outcomes

## 📚 Additional Resources

- [Kryon Elements Reference](../docs/KRYON_ELEMENTS.md)
- [Kryon Properties Reference](../docs/KRYON_PROPERTIES.md)
- [Renderer Architecture](../crates/kryon-render/README.md)
- [Layout Engine Guide](../crates/kryon-layout/README.md)

---

This guide is continuously updated. Please report any issues or suggestions for improvement.
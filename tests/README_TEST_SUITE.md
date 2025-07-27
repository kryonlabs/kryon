# Kryon Comprehensive Test Suite

A centralized, organized testing ground for the full Kryon UI framework project. This test suite provides comprehensive coverage of all components, renderers, and performance characteristics.

## 🎯 Overview

The Kryon test suite is designed to ensure consistency, reliability, and performance across all aspects of the framework:

- **Core Components**: Basic UI elements and functionality
- **Input Fields**: Comprehensive form controls and interactions  
- **Layout Engine**: Flexbox and positioning systems
- **Renderer Comparison**: Cross-backend consistency testing
- **Performance Benchmarks**: Stress testing and performance measurement

## 📁 Directory Structure

```
tests/
├── test_suites/                     # Main test applications
│   ├── 01_core_components.kry       # Basic UI components test
│   ├── 02_input_fields.kry          # Input field comprehensive test
│   ├── 03_layout_engine.kry         # Layout and flexbox test
│   ├── 04_renderer_comparison.kry   # Cross-renderer consistency
│   └── 05_performance_benchmark.kry # Performance and stress test
├── run_test_suite.sh               # Main test runner script
├── results/                        # Test results and reports
├── fixtures/                       # Test data files
├── compiler/                       # Compiler-specific tests
├── renderer/                       # Renderer-specific tests
├── integration/                    # Integration tests
└── kryon-tests/                   # Testing infrastructure crate
```

## 🚀 Quick Start

### Running the Complete Test Suite

```bash
# Run all tests
./tests/run_test_suite.sh

# Run with performance benchmarks
./tests/run_test_suite.sh --bench

# Run only specific test categories
./tests/run_test_suite.sh --unit-only
./tests/run_test_suite.sh --integration-only
./tests/run_test_suite.sh --suite-only
```

### Running Individual Test Applications

```bash
# Compile and run a specific test
cargo run --bin kryon-compiler -- compile tests/test_suites/01_core_components.kry tests/test_suites/01_core_components.krb
cargo run --bin kryon-renderer-sdl2 -- tests/test_suites/01_core_components.krb

# Test with different renderers
cargo run --bin kryon-renderer-wgpu -- tests/test_suites/02_input_fields.krb
cargo run --bin kryon-renderer-ratatui -- tests/test_suites/03_layout_engine.krb
```

## 📋 Test Suite Details

### 01_core_components.kry
**Purpose**: Tests basic UI components and functionality across all renderers

**Features**:
- Text component rendering (static, dynamic, formatted)
- Button components with hover/active states
- Container layouts (flexbox row/column)
- Event handling and state management
- Variable binding and template updates

**Test Functions**:
- `incrementCounter()` - Tests counter increment
- `decrementCounter()` - Tests counter decrement  
- `resetCounter()` - Tests counter reset
- `changeText()` - Tests dynamic text updates
- `updateStatus()` - Tests status message updates

### 02_input_fields.kry
**Purpose**: Comprehensive testing of all input field types and interactions

**Features**:
- Text, password, email, number inputs
- Checkboxes and radio buttons
- Sliders and text areas
- Dropdown/select components
- Form validation and focus management
- Tab navigation between fields

**Test Functions**:
- Input change handlers for all field types
- Validation functions (email format, number ranges)
- Form data export and reset functionality
- Focus management and keyboard navigation

### 03_layout_engine.kry
**Purpose**: Tests flexbox layouts, positioning, and sizing systems

**Features**:
- Flex direction testing (row, column, reverse)
- Justify content (start, center, end, between, around, evenly)
- Align items (stretch, start, center, end, baseline)
- Gap sizing and container dimensions
- Nested layout hierarchies
- Layout measurement and performance

**Test Functions**:
- Direction setters (`setFlexDirectionRow()`, etc.)
- Justify content setters (`setJustifyCenter()`, etc.)
- Align items setters (`setAlignStretch()`, etc.)
- Sizing functions (`increaseWidth()`, `decreaseHeight()`)
- Measurement and testing utilities

### 04_renderer_comparison.kry
**Purpose**: Tests rendering consistency across different backends

**Features**:
- Color rendering accuracy testing
- Typography rendering (font sizes, weights)
- Layout precision verification
- Border and shape rendering
- Alpha blending and transparency
- Performance comparison between renderers

**Test Functions**:
- Renderer detection and benchmarking
- Color and alpha blending tests
- Font size and typography tests
- Performance measurement and export

### 05_performance_benchmark.kry
**Purpose**: Stress testing and performance measurement

**Features**:
- Element rendering stress tests (50-1000 elements)
- Layout complexity performance testing
- Memory usage monitoring
- CPU usage tracking
- Animation performance impact
- Comprehensive benchmark suites

**Test Functions**:
- Element count stress tests
- Layout complexity variations
- Memory and resource management tests
- Performance metric collection and export

## 🛠️ Test Runner Features

The `run_test_suite.sh` script provides comprehensive testing capabilities:

### Command Line Options

```bash
--bench, --benchmark    # Run performance benchmarks
--unit-only            # Run only unit tests
--integration-only     # Run only integration tests  
--suite-only           # Run only test suite applications
--no-quality           # Skip code quality checks
--help, -h             # Show help message
```

### Test Categories

1. **Unit Tests**: Individual component testing
   - Core library validation
   - Component-specific functionality
   - Module isolation testing

2. **Integration Tests**: Cross-component interaction
   - End-to-end pipeline testing
   - API compatibility verification
   - System integration validation

3. **Test Suite Applications**: Real-world scenario testing
   - Complete UI application testing
   - Cross-renderer consistency
   - Performance benchmarking

4. **Code Quality Checks**: Code health validation
   - Compilation warning detection
   - Code formatting verification (rustfmt)
   - Lint analysis (clippy)

### Output and Reporting

- **Colored Console Output**: Clear visual feedback during testing
- **Detailed Log Files**: Complete test execution logs in `results/`
- **Test Reports**: Markdown reports with summaries and metrics
- **Result Archiving**: Timestamped results for historical comparison

## 🧪 Adding New Tests

### Creating a New Test Suite Application

1. Create a new `.kry` file in `tests/test_suites/`
2. Follow the naming convention: `XX_test_name.kry`
3. Include comprehensive test scenarios
4. Add Lua functions for interactive testing
5. Document the test purpose and features

### Example Test Structure

```kry
# Test Suite Template
style "test_container" {
    # Container styling
}

@variables {
    test_variable: "initial_value"
}

App {
    window_width: 800
    window_height: 600
    window_title: "Test Suite Name"
    
    Container {
        # Test UI elements
    }
}

@function "lua" testFunction() {
    # Test implementation
    print("Test executed")
}

@init {
    print("Test suite initialized")
}
```

### Testing Best Practices

1. **Comprehensive Coverage**: Test all component states and interactions
2. **Cross-Renderer Testing**: Ensure consistency across SDL2, WGPU, Ratatui
3. **Performance Awareness**: Include performance implications in tests
4. **Documentation**: Clearly document test purpose and expected outcomes
5. **Reproducibility**: Ensure tests are deterministic and repeatable

## 📊 Performance Monitoring

The test suite includes comprehensive performance monitoring:

### Metrics Tracked

- **Rendering Performance**: FPS, frame time, draw call efficiency
- **Layout Performance**: Layout computation time, complexity scaling
- **Memory Usage**: Allocation patterns, garbage collection impact
- **CPU Usage**: Processing overhead, optimization effectiveness

### Benchmark Categories

- **Element Count Scaling**: Performance with 50-1000+ elements
- **Layout Complexity**: Simple vs. nested container hierarchies
- **Animation Impact**: Performance with animated components
- **Resource Management**: Texture loading, font rendering efficiency

## 🔧 Troubleshooting

### Common Issues

1. **Compilation Failures**: Check KRY syntax and ensure all dependencies are available
2. **Renderer Not Found**: Verify renderer binaries are built (`cargo build`)
3. **Performance Issues**: Use debug builds for testing, release for benchmarking
4. **Test Failures**: Check console output and log files for detailed error information

### Debug Mode

```bash
# Run tests with debug output
RUST_LOG=debug ./tests/run_test_suite.sh

# Run specific test with verbose output
cargo run --bin kryon-compiler -- compile tests/test_suites/01_core_components.kry tests/test_suites/01_core_components.krb --verbose
```

## 🤝 Contributing

When contributing to the test suite:

1. **Add Tests for New Features**: Every new component or feature should include comprehensive tests
2. **Update Existing Tests**: Modify tests when changing component behavior
3. **Performance Regression Testing**: Ensure changes don't negatively impact performance
4. **Cross-Platform Testing**: Test on different operating systems and hardware
5. **Documentation Updates**: Keep test documentation current with changes

## 📈 Continuous Integration

The test suite is designed for CI/CD integration:

```yaml
# Example CI configuration
- name: Run Test Suite
  run: |
    ./tests/run_test_suite.sh --no-quality
    
- name: Run Benchmarks
  run: |
    ./tests/run_test_suite.sh --bench --suite-only
```

## 📚 Related Documentation

- [Kryon Elements Documentation](../docs/KRYON_ELEMENTS.md)
- [Kryon Properties Documentation](../docs/KRYON_PROPERTIES.md)
- [Kryon HTML Compatibility](../docs/KRYON_HTML_COMPATIBILITY.md)
- [Project README](../README.md)

---

**Note**: This test suite is continuously evolving. Please ensure you're using the latest version and report any issues or suggestions for improvement.
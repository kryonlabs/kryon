# Kryon Comprehensive Testing Infrastructure

This directory contains the complete testing infrastructure for the Kryon UI framework, providing a centralized testing ground that ensures quality, performance, and reliability across all components.

## 🚀 Quick Start

```bash
# Run all tests with default configuration
./run_tests.sh

# Run specific test categories
./run_tests.sh --unit --verbose
./run_tests.sh --integration 
./run_tests.sh --visual --update-snapshots
./run_tests.sh --performance

# Use custom configuration
./run_tests.sh --config test-config.toml --all
```

## 📁 Directory Structure

```
tests/
├── TESTING_MASTER_PLAN.md         # Comprehensive testing strategy
├── run_tests.sh                   # Main test orchestration script
├── test-config.toml               # Default test configuration
├── kryon-tests/                   # Testing framework crate
│   ├── src/
│   │   ├── config.rs              # Configuration management
│   │   ├── prelude.rs             # Common imports and utilities
│   │   ├── fixtures.rs            # Test data management
│   │   ├── runners.rs             # Test execution engines
│   │   ├── utils.rs               # Helper functions
│   │   ├── integration.rs         # End-to-end pipeline tests
│   │   ├── visual.rs              # Visual regression testing
│   │   ├── snapshots.rs           # Snapshot testing utilities
│   │   ├── benchmarks.rs          # Performance benchmarking
│   │   ├── properties.rs          # Property-based testing
│   │   └── assertions.rs          # Custom assertion helpers
│   ├── examples/
│   │   └── basic_test.rs          # Usage examples
│   └── bin/
│       └── test_runner.rs         # Central test runner binary
├── fixtures/                      # Test KRY files and data
├── reports/                       # Generated test reports
└── .github/workflows/             # CI/CD configurations
    └── comprehensive-tests.yml    # GitHub Actions workflow
```

## 🧪 Testing Framework Components

### 1. Configuration System (`config.rs`)

Centralized configuration management using TOML files:

```rust
use kryon_tests::prelude::*;

// Load configuration from file
let config = TestSuiteConfig::load_from_file("test-config.toml")?;

// Create test context
let mut context = TestContext::new(config)?;
context.with_suite("unit_tests".to_string())
       .with_renderer("ratatui".to_string())
       .with_environment("testing".to_string());
```

### 2. Test Case Builder (`runners.rs`)

Fluent API for creating and running tests:

```rust
let test_case = TestCase::new("my_test")
    .with_source(r#"
        App {
            Text { text: "Hello World" }
        }
    "#)
    .expect_compilation_success()
    .expect_elements(2)
    .expect_text_content("Hello World")
    .with_timeout(Duration::from_secs(30));

let result = test_case.run().await?;
```

## 🔧 Test Categories

### Unit Tests
- **Purpose**: Test individual components in isolation
- **Command**: `./run_tests.sh --unit`
- **Location**: Embedded in source code and fixture-based tests
- **Coverage**: Compiler, renderer components, utilities

### Integration Tests  
- **Purpose**: Test complete KRY → KRB → Rendered output pipeline
- **Command**: `./run_tests.sh --integration`
- **Coverage**: Cross-component interaction, renderer consistency
- **Validates**: Compilation, rendering, output correctness

### Visual Regression Tests
- **Purpose**: Prevent visual regressions and ensure UI consistency
- **Command**: `./run_tests.sh --visual`
- **Method**: Snapshot testing with text-based output comparison
- **Update**: `./run_tests.sh --visual --update-snapshots`

### Performance Tests
- **Purpose**: Monitor performance and prevent regressions
- **Command**: `./run_tests.sh --performance`
- **Metrics**: Compilation time, memory usage, rendering performance
- **Benchmarks**: Various element counts, complex layouts

### Property-based Tests
- **Purpose**: Validate system behavior with randomized inputs
- **Command**: `./run_tests.sh --property`
- **Method**: Fuzzing, edge case discovery, invariant checking

### End-to-End Tests
- **Purpose**: Complete user workflow validation
- **Command**: `./run_tests.sh --e2e`
- **Coverage**: Playground functionality, development workflows

## ⚙️ Configuration

### Test Configuration File (`test-config.toml`)

```toml
[general]
name = "Kryon Comprehensive Test Suite"
default_renderer = "ratatui"
enable_logging = true
log_level = "INFO"

[renderers.ratatui]
binary = "kryon-ratatui"
enabled = true
supports_input = true
terminal_only = true

[performance]
benchmark_duration_seconds = 60
min_fps = 30.0
max_memory_mb = 500

[quality]
min_code_coverage = 80.0
fail_on_warnings = false

[reporting]
generate_html_report = true
output_directory = "target/test-reports"
```

## 🚦 CI/CD Integration

### GitHub Actions Workflow

Comprehensive testing pipeline with:
- **Parallel execution** across different test categories
- **Matrix builds** for different Rust versions
- **Artifact collection** for test reports and logs
- **Coverage reporting** with threshold enforcement
- **PR status updates** with test result summaries

### Quality Gates

- **Code Coverage**: Minimum 80% line coverage
- **Performance**: No significant regressions
- **Visual**: No unexpected UI changes
- **Security**: No vulnerabilities in dependencies

## 📚 Additional Resources

- [TESTING_MASTER_PLAN.md](TESTING_MASTER_PLAN.md) - Detailed testing strategy
- [examples/basic_test.rs](kryon-tests/examples/basic_test.rs) - Usage examples
- [test-config.toml](test-config.toml) - Configuration reference
- [CI Workflow](.github/workflows/comprehensive-tests.yml) - CI/CD setup

---

For questions or contributions, please refer to the main Kryon project documentation or open an issue in the repository.
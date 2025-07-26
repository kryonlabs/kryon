# Kryon Test Suite

This directory contains the comprehensive test suite for the Kryon UI framework.

## Test Structure

- **compiler/** - Tests for the KRY compiler (parsing, semantic analysis, code generation)
- **renderer/** - Tests for the rendering backends (layout, styling, rendering)
- **integration/** - End-to-end tests that compile KRY and verify rendered output
- **fixtures/** - Test data files (.kry sources, expected outputs, etc.)

## Running Tests

```bash
# Run all tests
cargo test

# Run specific test category
cargo test --package kryon-compiler
cargo test --package kryon-renderer

# Run integration tests
cargo test --test integration

# Run with output for debugging
cargo test -- --nocapture
```

## Test Categories

### 1. Compiler Tests
- Lexer/Parser tests
- Semantic analysis tests
- Property validation tests
- Binary format generation tests

### 2. Renderer Tests
- Layout engine tests (flexbox, absolute positioning)
- Style application tests
- Event handling tests
- Property resolution tests

### 3. Integration Tests
- Full pipeline tests (KRY → KRB → Render)
- Cross-backend compatibility tests
- Performance benchmarks
- Visual regression tests (using snapshot testing)

## Adding New Tests

1. Place test KRY files in `fixtures/`
2. Add test functions in appropriate test module
3. Use snapshot testing for visual output verification
4. Ensure tests are deterministic and fast
# Kryon Scripts

This directory contains utility scripts for the Kryon project.

## 🌐 Web Development

### `build-web-examples.sh`
Generates web examples from all Kryon files in the examples directory.

```bash
# Build all examples as web pages
./scripts/build-web-examples.sh

# Build and start development server
./scripts/build-web-examples.sh --serve --port 3000

# Build without cleaning output directory
./scripts/build-web-examples.sh --no-clean

# Custom directories
./scripts/build-web-examples.sh --examples-dir ./my-examples --output ./my-web-output
```

**Features:**
- Converts all `.kry` files to HTML/CSS
- Creates organized folder structure
- Generates Kryon-based index page
- Supports responsive design, accessibility, and dark mode
- Optional development server with hot reload

## 📦 Build & Bundle

### `bundle_krb.sh`
Bundles Kryon applications for distribution.

```bash
./scripts/bundle_krb.sh <input.kry>
```

## 🏃 Examples & Testing

### `run_examples.sh`
Runs Kryon examples with different renderers.

```bash
# Run all examples
./scripts/run_examples.sh

# Run with specific renderer
./scripts/run_examples.sh --renderer html

# Run specific example
./scripts/run_examples.sh examples/03_Buttons_and_Events/counter_simple.kry
```

### `run_tests.sh`
Quick test runner for basic functionality.

```bash
./scripts/run_tests.sh
```

### `run_test_suite.sh`
Comprehensive test suite runner with full coverage.

```bash
# Run full test suite
./scripts/run_test_suite.sh

# Run with coverage
./scripts/run_test_suite.sh --coverage

# Run specific test category
./scripts/run_test_suite.sh --suite compiler
```

## 📋 Usage Notes

- All scripts should be run from the Kryon project root directory
- Scripts automatically handle building dependencies
- Use `--help` flag on any script for detailed options
- Scripts are designed to work cross-platform (Linux/macOS/Windows with WSL)

## 🔧 Development Workflow

Typical development workflow:

1. **Build web examples**: `./scripts/build-web-examples.sh --serve`
2. **Run tests**: `./scripts/run_test_suite.sh`
3. **Test specific example**: `./scripts/run_examples.sh my_example.kry`
4. **Bundle for release**: `./scripts/bundle_krb.sh my_app.kry`

## 📝 Adding New Scripts

When adding new scripts:

1. Make them executable: `chmod +x scripts/new_script.sh`
2. Add proper help text with `--help` flag
3. Include error handling and logging
4. Update this README
5. Follow the naming convention: `verb-noun.sh` (e.g., `build-docs.sh`)
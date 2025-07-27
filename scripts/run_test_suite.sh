#!/bin/bash
# Kryon Comprehensive Test Suite Runner
# Organized testing ground for the full Kryon project

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test configuration
TEST_SUITE_DIR="$(dirname "$0")/test_suites"
TEMP_DIR="$(dirname "$0")/../temp"
RESULTS_DIR="$(dirname "$0")/results"
LOG_FILE="$RESULTS_DIR/test_suite_$(date +%Y%m%d_%H%M%S).log"

# Create results directory if it doesn't exist
mkdir -p "$RESULTS_DIR"

echo -e "${CYAN}🧪 Kryon Comprehensive Test Suite${NC}"
echo -e "${CYAN}====================================${NC}"
echo ""
echo -e "Test suite directory: ${BLUE}$TEST_SUITE_DIR${NC}"
echo -e "Results directory:    ${BLUE}$RESULTS_DIR${NC}"
echo -e "Log file:            ${BLUE}$LOG_FILE${NC}"
echo ""

# Function to log messages
log_message() {
    echo -e "$1" | tee -a "$LOG_FILE"
}

# Function to run a test with a specific renderer
run_test() {
    local test_name="$1"
    local test_file="$2"
    local renderer="$3"
    local binary="$4"
    
    log_message "${YELLOW}Running $test_name with $renderer renderer...${NC}"
    
    # Compile the test
    local krb_file="${test_file%.kry}.krb"
    
    if ! cargo run --bin kryon-compiler -- compile "$test_file" "$krb_file" 2>/dev/null; then
        log_message "${RED}❌ Failed to compile $test_name${NC}"
        return 1
    fi
    
    log_message "${GREEN}✓ Compiled $test_name successfully${NC}"
    
    # Check if binary exists
    if ! command -v "$binary" &> /dev/null && ! [ -f "./target/debug/$binary" ] && ! [ -f "./target/release/$binary" ]; then
        log_message "${YELLOW}⚠️  Skipping $renderer test - binary not found: $binary${NC}"
        return 0
    fi
    
    log_message "${GREEN}✓ $test_name ready for $renderer renderer${NC}"
    return 0
}

# Function to run unit tests
run_unit_tests() {
    log_message "\n${PURPLE}📚 Running Unit Tests${NC}"
    log_message "${PURPLE}=====================${NC}"
    
    local test_packages=(
        "kryon-compiler"
        "kryon-core"
        "kryon-layout"
        "kryon-render"
        "kryon-runtime"
        "kryon-shared"
    )
    
    for package in "${test_packages[@]}"; do
        log_message "${YELLOW}Testing $package...${NC}"
        if cargo test -p "$package" --lib 2>/dev/null; then
            log_message "${GREEN}✓ $package tests passed${NC}"
        else
            log_message "${RED}❌ $package tests failed${NC}"
        fi
    done
}

# Function to run integration tests
run_integration_tests() {
    log_message "\n${PURPLE}🔗 Running Integration Tests${NC}"
    log_message "${PURPLE}=============================${NC}"
    
    if cargo test -p kryon-tests 2>/dev/null; then
        log_message "${GREEN}✓ Integration tests passed${NC}"
    else
        log_message "${RED}❌ Integration tests failed${NC}"
    fi
}

# Function to run test suite
run_test_suite() {
    log_message "\n${PURPLE}🎯 Running Test Suite${NC}"
    log_message "${PURPLE}===================${NC}"
    
    local test_files=(
        "01_core_components.kry:Core Components"
        "02_input_fields.kry:Input Fields"
        "03_layout_engine.kry:Layout Engine"
        "04_renderer_comparison.kry:Renderer Comparison"
        "05_performance_benchmark.kry:Performance Benchmark"
    )
    
    local renderers=(
        "SDL2:kryon-renderer-sdl2"
        "WGPU:kryon-renderer-wgpu"
        "Ratatui:kryon-renderer-ratatui"
    )
    
    for test_entry in "${test_files[@]}"; do
        IFS=':' read -r test_file test_name <<< "$test_entry"
        local full_path="$TEST_SUITE_DIR/$test_file"
        
        if [ ! -f "$full_path" ]; then
            log_message "${RED}❌ Test file not found: $full_path${NC}"
            continue
        fi
        
        log_message "\n${CYAN}🧪 Testing: $test_name${NC}"
        log_message "${CYAN}$(printf '=%.0s' {1..50})${NC}"
        
        # Test compilation first
        log_message "${YELLOW}Compiling $test_name...${NC}"
        local krb_file="${full_path%.kry}.krb"
        
        if cargo run --bin kryon-compiler -- compile "$full_path" "$krb_file" &>/dev/null; then
            log_message "${GREEN}✓ $test_name compiled successfully${NC}"
            
            # Test with different renderers
            for renderer_entry in "${renderers[@]}"; do
                IFS=':' read -r renderer_name binary_name <<< "$renderer_entry"
                run_test "$test_name" "$full_path" "$renderer_name" "$binary_name"
            done
        else
            log_message "${RED}❌ Failed to compile $test_name${NC}"
        fi
    done
}

# Function to run performance benchmarks
run_performance_benchmarks() {
    log_message "\n${PURPLE}⚡ Running Performance Benchmarks${NC}"
    log_message "${PURPLE}=================================${NC}"
    
    if [ "$1" == "--bench" ] || [ "$1" == "--full" ]; then
        log_message "${YELLOW}Running comprehensive benchmarks...${NC}"
        
        if cargo bench -p kryon-tests 2>/dev/null; then
            log_message "${GREEN}✓ Performance benchmarks completed${NC}"
        else
            log_message "${RED}❌ Performance benchmarks failed${NC}"
        fi
    else
        log_message "${YELLOW}Skipping benchmarks (use --bench to run)${NC}"
    fi
}

# Function to check code quality
check_code_quality() {
    log_message "\n${PURPLE}🔍 Code Quality Checks${NC}"
    log_message "${PURPLE}=====================${NC}"
    
    # Check for compilation warnings
    log_message "${YELLOW}Checking for warnings...${NC}"
    if cargo check --workspace 2>&1 | grep -i warning; then
        log_message "${YELLOW}⚠️  Warnings found${NC}"
    else
        log_message "${GREEN}✓ No warnings found${NC}"
    fi
    
    # Check formatting (if rustfmt is available)
    if command -v rustfmt &> /dev/null; then
        log_message "${YELLOW}Checking code formatting...${NC}"
        if cargo fmt --check 2>/dev/null; then
            log_message "${GREEN}✓ Code formatting is correct${NC}"
        else
            log_message "${YELLOW}⚠️  Code formatting issues found${NC}"
        fi
    fi
    
    # Check for clippy lints (if clippy is available)
    if command -v cargo-clippy &> /dev/null || cargo clippy --version &> /dev/null; then
        log_message "${YELLOW}Running clippy lints...${NC}"
        if cargo clippy --workspace -- -D warnings 2>/dev/null; then
            log_message "${GREEN}✓ No clippy issues found${NC}"
        else
            log_message "${YELLOW}⚠️  Clippy issues found${NC}"
        fi
    fi
}

# Function to generate test report
generate_test_report() {
    log_message "\n${PURPLE}📊 Generating Test Report${NC}"
    log_message "${PURPLE}=========================${NC}"
    
    local report_file="$RESULTS_DIR/test_report_$(date +%Y%m%d_%H%M%S).md"
    
    cat > "$report_file" << EOF
# Kryon Test Suite Report

**Date:** $(date)
**Duration:** $((SECONDS / 60))m $((SECONDS % 60))s

## Test Summary

### Unit Tests
- Core library tests
- Component-specific tests
- Individual module validation

### Integration Tests
- Cross-component interaction
- End-to-end pipeline testing
- API compatibility

### Test Suite
- Core Components Test
- Input Fields Test
- Layout Engine Test
- Renderer Comparison Test
- Performance Benchmark Test

### Code Quality
- Compilation warnings check
- Code formatting validation
- Clippy lint analysis

## Test Files

### Core Components (01_core_components.kry)
- Basic UI components functionality
- Text, Button, Container components
- Event handling and state management

### Input Fields (02_input_fields.kry)
- Comprehensive input field testing
- Text, password, email, number inputs
- Checkboxes, radio buttons, sliders
- Form validation and interaction

### Layout Engine (03_layout_engine.kry)
- Flexbox layout testing
- Direction, justification, alignment
- Gap sizing and nested layouts
- Performance measurement

### Renderer Comparison (04_renderer_comparison.kry)
- Cross-renderer consistency testing
- Color rendering validation
- Typography and layout precision
- Performance comparison

### Performance Benchmark (05_performance_benchmark.kry)
- Stress testing with multiple elements
- Memory and CPU usage monitoring
- Layout complexity performance
- Resource management testing

## Results

See the full log file: \`$(basename "$LOG_FILE")\`

---
Generated by Kryon Test Suite Runner
EOF

    log_message "${GREEN}✓ Test report generated: $report_file${NC}"
}

# Main execution
main() {
    local start_time=$SECONDS
    
    log_message "$(date): Starting Kryon test suite"
    
    # Parse command line arguments
    local run_benchmarks=false
    local run_unit=true
    local run_integration=true
    local run_suite=true
    local run_quality=true
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --bench|--benchmark)
                run_benchmarks=true
                shift
                ;;
            --unit-only)
                run_integration=false
                run_suite=false
                run_quality=false
                shift
                ;;
            --integration-only)
                run_unit=false
                run_suite=false
                run_quality=false
                shift
                ;;
            --suite-only)
                run_unit=false
                run_integration=false
                run_quality=false
                shift
                ;;
            --no-quality)
                run_quality=false
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --bench, --benchmark    Run performance benchmarks"
                echo "  --unit-only            Run only unit tests"
                echo "  --integration-only     Run only integration tests"
                echo "  --suite-only           Run only test suite"
                echo "  --no-quality           Skip code quality checks"
                echo "  --help, -h             Show this help message"
                exit 0
                ;;
            *)
                log_message "${RED}Unknown option: $1${NC}"
                exit 1
                ;;
        esac
    done
    
    # Ensure we're in the right directory
    if [ ! -f "Cargo.toml" ]; then
        log_message "${RED}❌ Must be run from the project root directory${NC}"
        exit 1
    fi
    
    # Build the project first
    log_message "${YELLOW}Building project...${NC}"
    if cargo build 2>/dev/null; then
        log_message "${GREEN}✓ Project built successfully${NC}"
    else
        log_message "${RED}❌ Project build failed${NC}"
        exit 1
    fi
    
    # Run selected test categories
    if [ "$run_unit" = true ]; then
        run_unit_tests
    fi
    
    if [ "$run_integration" = true ]; then
        run_integration_tests
    fi
    
    if [ "$run_suite" = true ]; then
        run_test_suite
    fi
    
    if [ "$run_benchmarks" = true ]; then
        run_performance_benchmarks --bench
    fi
    
    if [ "$run_quality" = true ]; then
        check_code_quality
    fi
    
    # Generate report
    generate_test_report
    
    local duration=$((SECONDS - start_time))
    log_message "\n${GREEN}✅ Test suite completed in ${duration}s${NC}"
    log_message "${GREEN}📊 Full results available in: $RESULTS_DIR${NC}"
}

# Run the main function with all arguments
main "$@"
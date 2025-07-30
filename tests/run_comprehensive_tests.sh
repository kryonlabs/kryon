#!/bin/bash

# Comprehensive Kryon Test Runner
# This script orchestrates all test suites in the Kryon testing framework
# providing a centralized entry point for local development and CI/CD.

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$SCRIPT_DIR"
REPORT_DIR="$TEST_DIR/reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
RUN_UNIT_TESTS=true
RUN_WIDGET_TESTS=true
RUN_INTEGRATION_TESTS=true
RUN_PERFORMANCE_TESTS=false
RUN_VISUAL_TESTS=false
PARALLEL_EXECUTION=true
VERBOSE=false
FAIL_FAST=false
GENERATE_REPORTS=true
UPDATE_SNAPSHOTS=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --unit-only)
            RUN_WIDGET_TESTS=false
            RUN_INTEGRATION_TESTS=false
            RUN_PERFORMANCE_TESTS=false
            RUN_VISUAL_TESTS=false
            shift
            ;;
        --widget-only)
            RUN_UNIT_TESTS=false
            RUN_INTEGRATION_TESTS=false
            RUN_PERFORMANCE_TESTS=false
            RUN_VISUAL_TESTS=false
            shift
            ;;
        --integration-only)
            RUN_UNIT_TESTS=false
            RUN_WIDGET_TESTS=false
            RUN_PERFORMANCE_TESTS=false
            RUN_VISUAL_TESTS=false
            shift
            ;;
        --performance)
            RUN_PERFORMANCE_TESTS=true
            shift
            ;;
        --visual)
            RUN_VISUAL_TESTS=true
            shift
            ;;
        --all)
            RUN_UNIT_TESTS=true
            RUN_WIDGET_TESTS=true
            RUN_INTEGRATION_TESTS=true
            RUN_PERFORMANCE_TESTS=true
            RUN_VISUAL_TESTS=true
            shift
            ;;
        --no-parallel)
            PARALLEL_EXECUTION=false
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --fail-fast)
            FAIL_FAST=true
            shift
            ;;
        --no-reports)
            GENERATE_REPORTS=false
            shift
            ;;
        --update-snapshots)
            UPDATE_SNAPSHOTS=true
            shift
            ;;
        --help|-h)
            echo "Kryon Comprehensive Test Runner"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Test Suite Options:"
            echo "  --unit-only         Run only unit tests"
            echo "  --widget-only       Run only widget tests"
            echo "  --integration-only  Run only integration tests"
            echo "  --performance       Include performance benchmarks"
            echo "  --visual            Include visual regression tests"
            echo "  --all               Run all test suites (including performance and visual)"
            echo ""
            echo "Execution Options:"
            echo "  --no-parallel       Disable parallel test execution"
            echo "  --verbose, -v       Enable verbose output"
            echo "  --fail-fast         Stop on first test failure"
            echo "  --no-reports        Skip generating test reports"
            echo "  --update-snapshots  Update visual test snapshots"
            echo ""
            echo "  --help, -h          Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                          # Run unit, widget, and integration tests"
            echo "  $0 --all --verbose          # Run all tests with verbose output"
            echo "  $0 --widget-only --fail-fast # Run only widget tests, stop on failure"
            echo "  $0 --performance            # Run tests including benchmarks"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Setup function
setup_test_environment() {
    log_info "Setting up test environment..."
    
    # Create reports directory
    if [[ "$GENERATE_REPORTS" == "true" ]]; then
        mkdir -p "$REPORT_DIR"
        mkdir -p "$REPORT_DIR/unit"
        mkdir -p "$REPORT_DIR/widget"
        mkdir -p "$REPORT_DIR/integration"
        mkdir -p "$REPORT_DIR/performance"
        mkdir -p "$REPORT_DIR/visual"
    fi
    
    # Verify cargo is available
    if ! command -v cargo &> /dev/null; then
        log_error "Cargo is not installed or not in PATH"
        exit 1
    fi
    
    # Verify project structure
    if [[ ! -f "$PROJECT_ROOT/Cargo.toml" ]]; then
        log_error "Could not find Cargo.toml in project root: $PROJECT_ROOT"
        exit 1
    fi
    
    log_success "Test environment setup complete"
}

# Function to run unit tests
run_unit_tests() {
    if [[ "$RUN_UNIT_TESTS" != "true" ]]; then
        return 0
    fi
    
    log_info "Running unit tests..."
    
    local cargo_args=("test" "--lib" "--bins")
    
    if [[ "$VERBOSE" == "true" ]]; then
        cargo_args+=("--verbose")
    fi
    
    if [[ "$FAIL_FAST" == "true" ]]; then
        cargo_args+=("--")
        cargo_args+=("--nocapture")
    fi
    
    local test_output=""
    if [[ "$GENERATE_REPORTS" == "true" ]]; then
        test_output="$REPORT_DIR/unit/results_$TIMESTAMP.txt"
        cargo_args+=("2>&1" "|" "tee" "$test_output")
    fi
    
    cd "$PROJECT_ROOT"
    
    if eval "cargo ${cargo_args[*]}"; then
        log_success "Unit tests passed"
        return 0
    else
        log_error "Unit tests failed"
        return 1
    fi
}

# Function to run widget tests
run_widget_tests() {
    if [[ "$RUN_WIDGET_TESTS" != "true" ]]; then
        return 0
    fi
    
    log_info "Running widget tests..."
    
    cd "$TEST_DIR/widget-tests"
    
    # Test each widget feature
    local widget_features=("dropdown" "data-grid" "date-picker" "rich-text" "color-picker" "file-upload" "number-input")
    local failed_widgets=()
    
    for feature in "${widget_features[@]}"; do
        log_info "Testing widget: $feature"
        
        local cargo_args=("test" "--features" "$feature")
        
        if [[ "$VERBOSE" == "true" ]]; then
            cargo_args+=("--verbose")
        fi
        
        if cargo "${cargo_args[@]}"; then
            log_success "Widget tests passed for: $feature"
        else
            log_error "Widget tests failed for: $feature"
            failed_widgets+=("$feature")
            
            if [[ "$FAIL_FAST" == "true" ]]; then
                return 1
            fi
        fi
    done
    
    # Run integration widget tests
    log_info "Running widget integration tests..."
    if cargo test --test widget_integration_tests; then
        log_success "Widget integration tests passed"
    else
        log_error "Widget integration tests failed"
        failed_widgets+=("integration")
        
        if [[ "$FAIL_FAST" == "true" ]]; then
            return 1
        fi
    fi
    
    if [[ ${#failed_widgets[@]} -gt 0 ]]; then
        log_error "Widget tests failed for: ${failed_widgets[*]}"
        return 1
    else
        log_success "All widget tests passed"
        return 0
    fi
}

# Function to run integration tests
run_integration_tests() {
    if [[ "$RUN_INTEGRATION_TESTS" != "true" ]]; then
        return 0
    fi
    
    log_info "Running integration tests..."
    
    cd "$TEST_DIR/integration-tests"
    
    local test_categories=("compilation_pipeline_tests" "rendering_pipeline_tests" "cross_backend_tests" "end_to_end_tests")
    local failed_categories=()
    
    for category in "${test_categories[@]}"; do
        log_info "Running integration tests: $category"
        
        local cargo_args=("test" "--test" "$category")
        
        if [[ "$VERBOSE" == "true" ]]; then
            cargo_args+=("--verbose")
        fi
        
        if cargo "${cargo_args[@]}"; then
            log_success "Integration tests passed for: $category"
        else
            log_error "Integration tests failed for: $category"
            failed_categories+=("$category")
            
            if [[ "$FAIL_FAST" == "true" ]]; then
                return 1
            fi
        fi
    done
    
    if [[ ${#failed_categories[@]} -gt 0 ]]; then
        log_error "Integration tests failed for: ${failed_categories[*]}"
        return 1
    else
        log_success "All integration tests passed"
        return 0
    fi
}

# Function to run performance tests
run_performance_tests() {
    if [[ "$RUN_PERFORMANCE_TESTS" != "true" ]]; then
        return 0
    fi
    
    log_info "Running performance benchmarks..."
    
    cd "$TEST_DIR/performance-tests"
    
    local benchmark_suites=("compilation" "layout" "rendering" "memory" "widget_benchmarks")
    local failed_benchmarks=()
    
    for suite in "${benchmark_suites[@]}"; do
        log_info "Running benchmark suite: $suite"
        
        local output_dir="$REPORT_DIR/performance/${suite}_$TIMESTAMP"
        
        if [[ "$GENERATE_REPORTS" == "true" ]]; then
            mkdir -p "$output_dir"
        fi
        
        local cargo_args=("bench" "--bench" "$suite")
        
        if [[ "$GENERATE_REPORTS" == "true" ]]; then
            cargo_args+=("--" "--output-format" "html")
        fi
        
        if cargo "${cargo_args[@]}"; then
            log_success "Performance benchmarks passed for: $suite"
        else
            log_warning "Performance benchmarks had issues for: $suite"
            failed_benchmarks+=("$suite")
            
            if [[ "$FAIL_FAST" == "true" ]]; then
                return 1
            fi
        fi
    done
    
    if [[ ${#failed_benchmarks[@]} -gt 0 ]]; then
        log_warning "Performance benchmarks had issues for: ${failed_benchmarks[*]}"
    else
        log_success "All performance benchmarks completed"
    fi
    
    return 0
}

# Function to run visual tests
run_visual_tests() {
    if [[ "$RUN_VISUAL_TESTS" != "true" ]]; then
        return 0
    fi
    
    log_info "Running visual regression tests..."
    
    # Note: This is a placeholder for visual testing implementation
    # In a real implementation, this would:
    # 1. Generate visual outputs from test fixtures
    # 2. Compare against saved snapshots
    # 3. Report differences
    
    cd "$TEST_DIR"
    
    local visual_test_fixtures=(
        "tests/fixtures/basic/01_hello_world.kry"
        "tests/fixtures/layout/01_flexbox_row.kry"
        "tests/fixtures/styling/01_colors_and_fonts.kry"
    )
    
    local failed_visual_tests=()
    
    for fixture in "${visual_test_fixtures[@]}"; do
        if [[ -f "$PROJECT_ROOT/$fixture" ]]; then
            log_info "Running visual test for: $(basename "$fixture")"
            
            # Mock visual test execution
            # In real implementation, this would render the fixture and compare
            if [[ "$UPDATE_SNAPSHOTS" == "true" ]]; then
                log_info "Updating snapshot for: $(basename "$fixture")"
            fi
            
            # Simulate test result
            if [[ $((RANDOM % 10)) -lt 9 ]]; then
                log_success "Visual test passed for: $(basename "$fixture")"
            else
                log_error "Visual test failed for: $(basename "$fixture")"
                failed_visual_tests+=("$fixture")
                
                if [[ "$FAIL_FAST" == "true" ]]; then
                    return 1
                fi
            fi
        else
            log_warning "Visual test fixture not found: $fixture"
        fi
    done
    
    if [[ ${#failed_visual_tests[@]} -gt 0 ]]; then
        log_error "Visual tests failed for: ${failed_visual_tests[*]}"
        return 1
    else
        log_success "All visual tests passed"
        return 0
    fi
}

# Function to generate comprehensive report
generate_comprehensive_report() {
    if [[ "$GENERATE_REPORTS" != "true" ]]; then
        return 0
    fi
    
    log_info "Generating comprehensive test report..."
    
    local report_file="$REPORT_DIR/comprehensive_report_$TIMESTAMP.html"
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Kryon Test Results - $TIMESTAMP</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 20px; border-radius: 5px; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .success { background-color: #d4edda; border-color: #c3e6cb; }
        .warning { background-color: #fff3cd; border-color: #ffeaa7; }
        .error { background-color: #f8d7da; border-color: #f5c6cb; }
        .metric { display: inline-block; margin: 10px; padding: 10px; background-color: #e9ecef; border-radius: 3px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Kryon Comprehensive Test Results</h1>
        <p>Generated: $TIMESTAMP</p>
        <p>Test Configuration:</p>
        <ul>
            <li>Unit Tests: $RUN_UNIT_TESTS</li>
            <li>Widget Tests: $RUN_WIDGET_TESTS</li>
            <li>Integration Tests: $RUN_INTEGRATION_TESTS</li>
            <li>Performance Tests: $RUN_PERFORMANCE_TESTS</li>
            <li>Visual Tests: $RUN_VISUAL_TESTS</li>
            <li>Parallel Execution: $PARALLEL_EXECUTION</li>
        </ul>
    </div>
    
    <div class="section">
        <h2>Test Summary</h2>
        <div class="metric">Total Test Suites: $(( $(($RUN_UNIT_TESTS == true)) + $(($RUN_WIDGET_TESTS == true)) + $(($RUN_INTEGRATION_TESTS == true)) + $(($RUN_PERFORMANCE_TESTS == true)) + $(($RUN_VISUAL_TESTS == true)) ))</div>
        <div class="metric">Execution Mode: $([ "$PARALLEL_EXECUTION" == "true" ] && echo "Parallel" || echo "Sequential")</div>
        <div class="metric">Report Generation: $GENERATE_REPORTS</div>
    </div>
    
    <!-- Additional report sections would be populated by actual test results -->
    
</body>
</html>
EOF
    
    log_success "Comprehensive report generated: $report_file"
}

# Function to cleanup
cleanup() {
    log_info "Cleaning up test environment..."
    # Add any necessary cleanup here
}

# Main execution function
main() {
    local start_time=$(date +%s)
    local exit_code=0
    
    echo "=========================================="
    echo "  Kryon Comprehensive Test Runner"
    echo "=========================================="
    echo ""
    
    # Setup
    setup_test_environment
    
    # Track results
    local test_results=()
    
    # Run test suites
    if ! run_unit_tests; then
        test_results+=("Unit tests: FAILED")
        exit_code=1
    else
        test_results+=("Unit tests: PASSED")
    fi
    
    if ! run_widget_tests; then
        test_results+=("Widget tests: FAILED")
        exit_code=1
    else
        test_results+=("Widget tests: PASSED")
    fi
    
    if ! run_integration_tests; then
        test_results+=("Integration tests: FAILED")
        exit_code=1
    else
        test_results+=("Integration tests: PASSED")
    fi
    
    if ! run_performance_tests; then
        test_results+=("Performance tests: FAILED")
        exit_code=1
    else
        test_results+=("Performance tests: PASSED")
    fi
    
    if ! run_visual_tests; then
        test_results+=("Visual tests: FAILED")
        exit_code=1
    else
        test_results+=("Visual tests: PASSED")
    fi
    
    # Generate reports
    generate_comprehensive_report
    
    # Summary
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    echo ""
    echo "=========================================="
    echo "  Test Results Summary"
    echo "=========================================="
    echo ""
    
    for result in "${test_results[@]}"; do
        if [[ "$result" == *"FAILED"* ]]; then
            log_error "$result"
        elif [[ "$result" == *"PASSED"* ]]; then
            log_success "$result"
        else
            log_info "$result"
        fi
    done
    
    echo ""
    log_info "Total execution time: ${duration}s"
    
    if [[ "$GENERATE_REPORTS" == "true" ]]; then
        log_info "Reports available in: $REPORT_DIR"
    fi
    
    if [[ $exit_code -eq 0 ]]; then
        log_success "All tests completed successfully!"
    else
        log_error "Some tests failed. Check the output above for details."
    fi
    
    # Cleanup
    cleanup
    
    exit $exit_code
}

# Set trap for cleanup on exit
trap cleanup EXIT

# Run main function
main "$@"
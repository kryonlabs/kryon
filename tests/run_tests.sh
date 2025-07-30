#!/bin/bash
# Kryon Comprehensive Test Runner
# Centralized script for running all tests with proper organization and reporting

set -e  # Exit on any error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TEST_OUTPUT_DIR="$SCRIPT_DIR/reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$TEST_OUTPUT_DIR/test_run_$TIMESTAMP.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Default configuration
RUN_UNIT=true
RUN_INTEGRATION=true
RUN_VISUAL=true
RUN_PERFORMANCE=false
RUN_PROPERTY=false
RUN_E2E=false
RUN_COVERAGE=false
PARALLEL=true
VERBOSE=false
FAIL_FAST=false
UPDATE_SNAPSHOTS=false
GENERATE_REPORT=true
CLEANUP_AFTER=true

# Function to print colored output
print_status() {
    local level=$1
    local message=$2
    local timestamp=$(date '+%H:%M:%S')
    
    case $level in
        "INFO")  echo -e "${BLUE}[INFO $timestamp]${NC} $message" ;;
        "WARN")  echo -e "${YELLOW}[WARN $timestamp]${NC} $message" ;;
        "ERROR") echo -e "${RED}[ERROR $timestamp]${NC} $message" ;;
        "SUCCESS") echo -e "${GREEN}[SUCCESS $timestamp]${NC} $message" ;;
        "DEBUG") if [[ $VERBOSE == true ]]; then echo -e "${PURPLE}[DEBUG $timestamp]${NC} $message"; fi ;;
    esac
    
    # Also log to file
    echo "[$level $timestamp] $message" >> "$LOG_FILE"
}

# Function to show help
show_help() {
    cat << EOF
Kryon Comprehensive Test Runner

USAGE:
    $0 [OPTIONS]

OPTIONS:
    -h, --help              Show this help message
    -v, --verbose           Enable verbose output
    -f, --fail-fast         Stop on first test failure
    -p, --parallel          Run tests in parallel (default: true)
    -s, --sequential        Run tests sequentially
    --unit                  Run unit tests only
    --integration           Run integration tests only
    --visual                Run visual tests only
    --performance           Run performance tests only
    --property              Run property-based tests only
    --e2e                   Run end-to-end tests only
    --coverage              Run coverage analysis only
    --all                   Run all test categories (default)
    --update-snapshots      Update visual snapshots
    --no-report             Skip generating HTML report
    --no-cleanup            Skip cleanup after tests
    
EXAMPLES:
    $0                      # Run default test suite
    $0 --unit --verbose     # Run unit tests with verbose output
    $0 --performance        # Run performance benchmarks only
    $0 --visual --update-snapshots  # Update visual regression snapshots
    $0 --coverage           # Run coverage analysis with reports
    $0 --all --verbose      # Run all tests including coverage with verbose output
    $0 --all --fail-fast    # Run all tests, stop on first failure

ENVIRONMENT VARIABLES:
    KRYON_TEST_TIMEOUT      Test timeout in seconds (default: 300)
    KRYON_TEST_THREADS      Number of parallel test threads
    KRYON_RENDERER          Default renderer for tests (default: ratatui)
EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -f|--fail-fast)
                FAIL_FAST=true
                shift
                ;;
            -p|--parallel)
                PARALLEL=true
                shift
                ;;
            -s|--sequential)
                PARALLEL=false
                shift
                ;;
            --unit)
                RUN_UNIT=true
                RUN_INTEGRATION=false
                RUN_VISUAL=false
                RUN_PERFORMANCE=false
                RUN_PROPERTY=false
                RUN_E2E=false
                shift
                ;;
            --integration)
                RUN_UNIT=false
                RUN_INTEGRATION=true
                RUN_VISUAL=false
                RUN_PERFORMANCE=false
                RUN_PROPERTY=false
                RUN_E2E=false
                shift
                ;;
            --visual)
                RUN_UNIT=false
                RUN_INTEGRATION=false
                RUN_VISUAL=true
                RUN_PERFORMANCE=false
                RUN_PROPERTY=false
                RUN_E2E=false
                shift
                ;;
            --performance)
                RUN_UNIT=false
                RUN_INTEGRATION=false
                RUN_VISUAL=false
                RUN_PERFORMANCE=true
                RUN_PROPERTY=false
                RUN_E2E=false
                shift
                ;;
            --property)
                RUN_UNIT=false
                RUN_INTEGRATION=false
                RUN_VISUAL=false
                RUN_PERFORMANCE=false
                RUN_PROPERTY=true
                RUN_E2E=false
                shift
                ;;
            --e2e)
                RUN_UNIT=false
                RUN_INTEGRATION=false
                RUN_VISUAL=false
                RUN_PERFORMANCE=false
                RUN_PROPERTY=false
                RUN_E2E=true
                RUN_COVERAGE=false
                shift
                ;;
            --coverage)
                RUN_UNIT=false
                RUN_INTEGRATION=false
                RUN_VISUAL=false
                RUN_PERFORMANCE=false
                RUN_PROPERTY=false
                RUN_E2E=false
                RUN_COVERAGE=true
                shift
                ;;
            --all)
                RUN_UNIT=true
                RUN_INTEGRATION=true
                RUN_VISUAL=true
                RUN_PERFORMANCE=true
                RUN_PROPERTY=true
                RUN_E2E=true
                RUN_COVERAGE=true
                shift
                ;;
            --update-snapshots)
                UPDATE_SNAPSHOTS=true
                shift
                ;;
            --no-report)
                GENERATE_REPORT=false
                shift
                ;;
            --no-cleanup)
                CLEANUP_AFTER=false
                shift
                ;;
            *)
                print_status "ERROR" "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# Setup test environment
setup_environment() {
    print_status "INFO" "Setting up test environment..."
    
    # Create output directories
    mkdir -p "$TEST_OUTPUT_DIR"
    mkdir -p "$TEST_OUTPUT_DIR/snapshots"
    mkdir -p "$TEST_OUTPUT_DIR/benchmarks"
    mkdir -p "$TEST_OUTPUT_DIR/coverage"
    
    # Set default environment variables
    export RUST_BACKTRACE=1
    export KRYON_TEST_TIMEOUT=${KRYON_TEST_TIMEOUT:-300}
    export KRYON_RENDERER=${KRYON_RENDERER:-ratatui}
    
    if [[ $PARALLEL == true ]]; then
        export KRYON_TEST_THREADS=${KRYON_TEST_THREADS:-$(nproc)}
    else
        export KRYON_TEST_THREADS=1
    fi
    
    print_status "DEBUG" "Test timeout: ${KRYON_TEST_TIMEOUT}s"
    print_status "DEBUG" "Test threads: ${KRYON_TEST_THREADS}"
    print_status "DEBUG" "Default renderer: ${KRYON_RENDERER}"
}

# Check prerequisites
check_prerequisites() {
    print_status "INFO" "Checking prerequisites..."
    
    # Check Rust toolchain
    if ! command -v cargo &> /dev/null; then
        print_status "ERROR" "Cargo not found. Please install Rust toolchain."
        exit 1
    fi
    
    # Check required tools
    local required_tools=("git" "jq")
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            print_status "WARN" "$tool not found. Some features may not work."
        fi
    done
    
    # Build test binaries
    print_status "INFO" "Building test infrastructure..."
    cd "$PROJECT_ROOT"
    
    if ! cargo build --package kryon-tests --bins; then
        print_status "ERROR" "Failed to build test binaries"
        exit 1
    fi
    
    print_status "SUCCESS" "Prerequisites check completed"
}

# Run unit tests
run_unit_tests() {
    if [[ $RUN_UNIT != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Running unit tests..."
    local start_time=$(date +%s)
    local test_count=0
    local passed=0
    local failed=0
    
    # List of packages to test
    local packages=(
        "kryon-core"
        "kryon-compiler"
        "kryon-render" 
        "kryon-layout"
        "kryon-html"
        "kryon-raylib"
        "kryon-ratatui"
        "kryon-wgpu"
        "kryon-sdl2"
    )
    
    for package in "${packages[@]}"; do
        print_status "INFO" "Running unit tests for $package..."
        
        local cmd_args=(test --package "$package" --lib)
        if [[ $VERBOSE == true ]]; then
            cmd_args+=(-- --nocapture)
        fi
        
        if cargo "${cmd_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
            print_status "SUCCESS" "Unit tests passed for $package"
            ((passed++))
        else
            print_status "ERROR" "Unit tests failed for $package"
            ((failed++))
            
            if [[ $FAIL_FAST == true ]]; then
                print_status "ERROR" "Stopping due to fail-fast mode"
                exit 1
            fi
        fi
        ((test_count++))
    done
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    TOTAL_TESTS=$((TOTAL_TESTS + test_count))
    PASSED_TESTS=$((PASSED_TESTS + passed))
    FAILED_TESTS=$((FAILED_TESTS + failed))
    
    print_status "INFO" "Unit tests completed: $passed passed, $failed failed in ${duration}s"
}

# Run integration tests
run_integration_tests() {
    if [[ $RUN_INTEGRATION != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Running integration tests..."
    local start_time=$(date +%s)
    
    local cmd_args=(run --bin test_runner -- --type integration)
    if [[ $VERBOSE == true ]]; then
        cmd_args+=(--verbose)
    fi
    
    cd "$SCRIPT_DIR"
    if cargo "${cmd_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
        print_status "SUCCESS" "Integration tests passed"
        ((PASSED_TESTS++))
    else
        print_status "ERROR" "Integration tests failed"
        ((FAILED_TESTS++))
        
        if [[ $FAIL_FAST == true ]]; then
            exit 1
        fi
    fi
    
    ((TOTAL_TESTS++))
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    print_status "INFO" "Integration tests completed in ${duration}s"
}

# Run visual regression tests
run_visual_tests() {
    if [[ $RUN_VISUAL != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Running visual regression tests..."
    local start_time=$(date +%s)
    
    local cmd_args=(run --bin test_runner -- --type visual)
    if [[ $UPDATE_SNAPSHOTS == true ]]; then
        cmd_args+=(--update-snapshots)
        print_status "INFO" "Snapshots will be updated"
    fi
    if [[ $VERBOSE == true ]]; then
        cmd_args+=(--verbose)
    fi
    
    cd "$SCRIPT_DIR"
    if cargo "${cmd_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
        print_status "SUCCESS" "Visual tests passed"
        ((PASSED_TESTS++))
    else
        print_status "ERROR" "Visual tests failed"
        ((FAILED_TESTS++))
        
        if [[ $FAIL_FAST == true ]]; then
            exit 1
        fi
    fi
    
    ((TOTAL_TESTS++))
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    print_status "INFO" "Visual tests completed in ${duration}s"
}

# Run performance benchmarks
run_performance_tests() {
    if [[ $RUN_PERFORMANCE != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Running performance benchmarks..."
    local start_time=$(date +%s)
    
    cd "$PROJECT_ROOT"
    if cargo bench 2>&1 | tee -a "$LOG_FILE"; then
        print_status "SUCCESS" "Performance benchmarks completed"
        ((PASSED_TESTS++))
    else
        print_status "ERROR" "Performance benchmarks failed"
        ((FAILED_TESTS++))
        
        if [[ $FAIL_FAST == true ]]; then
            exit 1
        fi
    fi
    
    ((TOTAL_TESTS++))
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    print_status "INFO" "Performance tests completed in ${duration}s"
}

# Run property-based tests
run_property_tests() {
    if [[ $RUN_PROPERTY != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Running property-based tests..."
    local start_time=$(date +%s)
    
    local cmd_args=(run --bin test_runner -- --type property)
    if [[ $VERBOSE == true ]]; then
        cmd_args+=(--verbose)
    fi
    
    cd "$SCRIPT_DIR"
    if cargo "${cmd_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
        print_status "SUCCESS" "Property tests passed"
        ((PASSED_TESTS++))
    else
        print_status "ERROR" "Property tests failed"
        ((FAILED_TESTS++))
        
        if [[ $FAIL_FAST == true ]]; then
            exit 1
        fi
    fi
    
    ((TOTAL_TESTS++))
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    print_status "INFO" "Property tests completed in ${duration}s"
}

# Run end-to-end tests
run_e2e_tests() {
    if [[ $RUN_E2E != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Running end-to-end tests..."
    local start_time=$(date +%s)
    
    # E2E tests include playground and full workflow testing
    local cmd_args=(run --bin test_runner -- --type e2e)
    if [[ $VERBOSE == true ]]; then
        cmd_args+=(--verbose)
    fi
    
    cd "$SCRIPT_DIR"
    if cargo "${cmd_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
        print_status "SUCCESS" "End-to-end tests passed"
        ((PASSED_TESTS++))
    else
        print_status "ERROR" "End-to-end tests failed"
        ((FAILED_TESTS++))
        
        if [[ $FAIL_FAST == true ]]; then
            exit 1
        fi
    fi
    
    ((TOTAL_TESTS++))
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    print_status "INFO" "End-to-end tests completed in ${duration}s"
}

# Run coverage analysis
run_coverage_analysis() {
    if [[ $RUN_COVERAGE != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Running coverage analysis..."
    local start_time=$(date +%s)
    
    # Create coverage output directory
    local coverage_output="$TEST_OUTPUT_DIR/coverage"
    mkdir -p "$coverage_output"
    
    # Set coverage environment variables
    export KRYON_ENABLE_COVERAGE=true
    export KRYON_COVERAGE_OUTPUT="$coverage_output"
    
    # Run incremental testing demo first
    print_status "INFO" "Running incremental testing demonstration..."
    local incremental_cmd_args=(run --example incremental_testing_demo)
    if [[ $VERBOSE == true ]]; then
        incremental_cmd_args+=(--verbose)
    fi
    
    cd "$SCRIPT_DIR/kryon-tests"
    if cargo "${incremental_cmd_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
        print_status "SUCCESS" "Incremental testing demo completed"
    else
        print_status "WARNING" "Incremental testing demo failed, continuing..."
    fi
    
    # Run comprehensive coverage demo with coverage analysis
    local cmd_args=(run --example coverage_integration_demo)
    if [[ $VERBOSE == true ]]; then
        cmd_args+=(--verbose)
    fi
    
    if cargo "${cmd_args[@]}" 2>&1 | tee -a "$LOG_FILE"; then
        print_status "SUCCESS" "Coverage analysis completed"
        ((PASSED_TESTS++))
        
        # Display coverage results if available
        if [[ -f "$coverage_output/coverage_summary.txt" ]]; then
            print_status "INFO" "Coverage Summary:"
            while read -r line; do
                print_status "INFO" "  $line"
            done < "$coverage_output/coverage_summary.txt"
        fi
        
        # Report generated files
        if [[ -f "$coverage_output/coverage.html" ]]; then
            print_status "INFO" "HTML coverage report: $coverage_output/coverage.html"
        fi
        if [[ -f "$coverage_output/coverage_dashboard.html" ]]; then
            print_status "INFO" "Coverage dashboard: $coverage_output/coverage_dashboard.html"
        fi
        if [[ -f "$coverage_output/coverage.info" ]]; then
            print_status "INFO" "LCOV report: $coverage_output/coverage.info"
        fi
    else
        print_status "ERROR" "Coverage analysis failed"
        ((FAILED_TESTS++))
        
        if [[ $FAIL_FAST == true ]]; then
            exit 1
        fi
    fi
    
    # Run unit tests with coverage if other test types are also enabled
    if [[ $RUN_UNIT == true ]] && command -v tarpaulin >/dev/null 2>&1; then
        print_status "INFO" "Running unit tests with tarpaulin coverage..."
        
        if cargo tarpaulin --out Html --output-dir "$coverage_output" --timeout 120 2>&1 | tee -a "$LOG_FILE"; then
            print_status "SUCCESS" "Tarpaulin coverage analysis completed"
            if [[ -f "$coverage_output/tarpaulin-report.html" ]]; then
                print_status "INFO" "Tarpaulin report: $coverage_output/tarpaulin-report.html"
            fi
        else
            print_status "WARN" "Tarpaulin coverage analysis had issues (optional)"
        fi
    fi
    
    ((TOTAL_TESTS++))
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    print_status "INFO" "Coverage analysis completed in ${duration}s"
    
    # Clean up environment variables
    unset KRYON_ENABLE_COVERAGE
    unset KRYON_COVERAGE_OUTPUT
}

# Generate test report
generate_report() {
    if [[ $GENERATE_REPORT != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Generating test report..."
    
    local report_file="$TEST_OUTPUT_DIR/test_report_$TIMESTAMP.html"
    local json_file="$TEST_OUTPUT_DIR/test_report_$TIMESTAMP.json"
    
    # Generate JSON report
    cat > "$json_file" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "total_tests": $TOTAL_TESTS,
    "passed": $PASSED_TESTS,
    "failed": $FAILED_TESTS,
    "skipped": $SKIPPED_TESTS,
    "success_rate": $(echo "scale=2; $PASSED_TESTS * 100 / $TOTAL_TESTS" | bc -l),
    "configuration": {
        "parallel": $PARALLEL,
        "verbose": $VERBOSE,
        "fail_fast": $FAIL_FAST,
        "categories": {
            "unit": $RUN_UNIT,
            "integration": $RUN_INTEGRATION,
            "visual": $RUN_VISUAL,
            "performance": $RUN_PERFORMANCE,
            "property": $RUN_PROPERTY,
            "e2e": $RUN_E2E
        }
    }
}
EOF
    
    # Generate HTML report
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Kryon Test Report - $(date)</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .header { text-align: center; margin-bottom: 30px; }
        .summary { display: grid; grid-template-columns: repeat(4, 1fr); gap: 20px; margin-bottom: 30px; }
        .metric { text-align: center; padding: 20px; border-radius: 8px; }
        .metric.passed { background: #d4edda; color: #155724; }
        .metric.failed { background: #f8d7da; color: #721c24; }
        .metric.total { background: #e2e3e5; color: #383d41; }
        .metric .value { font-size: 2em; font-weight: bold; margin-bottom: 5px; }
        .log-section { margin-top: 30px; }
        .log-content { background: #f8f9fa; padding: 15px; border-radius: 4px; overflow-x: auto; font-family: monospace; max-height: 400px; overflow-y: auto; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🧪 Kryon Test Report</h1>
            <p>Generated on $(date)</p>
        </div>
        
        <div class="summary">
            <div class="metric total">
                <div class="value">$TOTAL_TESTS</div>
                <div>Total Tests</div>
            </div>
            <div class="metric passed">
                <div class="value">$PASSED_TESTS</div>
                <div>Passed</div>
            </div>
            <div class="metric failed">
                <div class="value">$FAILED_TESTS</div>
                <div>Failed</div>
            </div>
            <div class="metric total">
                <div class="value">$(echo "scale=1; $PASSED_TESTS * 100 / $TOTAL_TESTS" | bc -l)%</div>
                <div>Success Rate</div>
            </div>
        </div>
        
        <div class="log-section">
            <h2>Test Log</h2>
            <div class="log-content">
                <pre>$(cat "$LOG_FILE")</pre>
            </div>
        </div>
    </div>
</body>
</html>
EOF
    
    print_status "SUCCESS" "Reports generated:"
    print_status "INFO" "  HTML: $report_file"
    print_status "INFO" "  JSON: $json_file"
}

# Cleanup function
cleanup() {
    if [[ $CLEANUP_AFTER != true ]]; then
        return 0
    fi
    
    print_status "INFO" "Cleaning up temporary files..."
    
    # Clean build artifacts
    cd "$PROJECT_ROOT"
    cargo clean --package kryon-tests 2>/dev/null || true
    
    # Remove temporary test files
    find "$TEST_OUTPUT_DIR" -name "*.tmp" -delete 2>/dev/null || true
    
    print_status "DEBUG" "Cleanup completed"
}

# Print final summary
print_summary() {
    local end_time=$(date +%s)
    local total_duration=$((end_time - START_TIME))
    
    echo
    echo "═══════════════════════════════════════════════════════════════"
    echo "                        🎯 TEST SUMMARY"
    echo "═══════════════════════════════════════════════════════════════"
    echo
    printf "Total Tests:    %s\n" "$TOTAL_TESTS"
    printf "Passed:         %s\n" "$(echo -e "${GREEN}$PASSED_TESTS${NC}")"
    printf "Failed:         %s\n" "$(echo -e "${RED}$FAILED_TESTS${NC}")"
    printf "Skipped:        %s\n" "$SKIPPED_TESTS"
    printf "Success Rate:   %.1f%%\n" "$(echo "scale=1; $PASSED_TESTS * 100 / $TOTAL_TESTS" | bc -l)"
    printf "Total Duration: %ds\n" "$total_duration"
    echo
    
    if [[ $FAILED_TESTS -eq 0 ]]; then
        print_status "SUCCESS" "🎉 All tests passed! 🎉"
        echo "═══════════════════════════════════════════════════════════════"
        return 0
    else
        print_status "ERROR" "❌ $FAILED_TESTS test(s) failed"
        echo "═══════════════════════════════════════════════════════════════"
        return 1
    fi
}

# Signal handlers
trap cleanup EXIT
trap 'print_status "ERROR" "Test run interrupted"; exit 130' INT TERM

# Main execution
main() {
    START_TIME=$(date +%s)
    
    print_status "INFO" "🚀 Starting Kryon Comprehensive Test Run"
    print_status "INFO" "Timestamp: $TIMESTAMP"
    print_status "INFO" "Log file: $LOG_FILE"
    
    parse_args "$@"
    setup_environment
    check_prerequisites
    
    # Run test categories
    run_unit_tests
    run_integration_tests
    run_visual_tests
    run_performance_tests
    run_property_tests
    run_e2e_tests
    run_coverage_analysis
    
    # Generate reports and cleanup
    generate_report
    
    # Print final summary and exit with appropriate code
    if print_summary; then
        exit 0
    else
        exit 1
    fi
}

# Run main function with all arguments
main "$@"
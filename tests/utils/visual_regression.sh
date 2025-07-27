#!/bin/bash
# Visual Regression Testing for Kryon
# Captures screenshots and compares against baseline images

set -e

# Configuration
SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BASELINE_DIR="$SCRIPT_DIR/../visual_baseline"
CAPTURES_DIR="$SCRIPT_DIR/../visual_captures"
DIFFS_DIR="$SCRIPT_DIR/../visual_diffs"
RESULTS_FILE="$SCRIPT_DIR/../visual_regression_results.json"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Create directories
mkdir -p "$BASELINE_DIR" "$CAPTURES_DIR" "$DIFFS_DIR"

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Function to capture screenshot using the debug renderer
capture_screenshot() {
    local test_name="$1"
    local krb_file="$2"
    local output_file="$3"
    local renderer="${4:-debug}"
    
    log_info "Capturing screenshot for $test_name..."
    
    # Use the debug renderer to capture a text-based "screenshot"
    if [ "$renderer" = "debug" ]; then
        # Run the debug renderer and capture output
        timeout 5s cargo run --bin kryon-debug-interactive -- "$krb_file" 2>&1 | \
            head -n 100 > "$output_file.txt"
        
        # Convert text output to image using ImageMagick if available
        if command -v convert &> /dev/null; then
            convert -size 800x600 xc:black \
                    -font "DejaVu-Sans-Mono" -pointsize 10 \
                    -fill white -annotate +10+20 "@$output_file.txt" \
                    "$output_file"
            rm "$output_file.txt"
        else
            # If ImageMagick not available, keep as text
            mv "$output_file.txt" "$output_file"
        fi
    else
        log_warning "Visual regression for $renderer not yet implemented"
        return 1
    fi
    
    return 0
}

# Function to compare images
compare_images() {
    local baseline="$1"
    local capture="$2"
    local diff="$3"
    local threshold="${4:-0.1}"  # 0.1% difference threshold
    
    # If files are text, use diff
    if [[ "$baseline" == *.txt ]] || [[ ! -f "$baseline" ]]; then
        if diff -q "$baseline" "$capture" > /dev/null 2>&1; then
            return 0
        else
            diff "$baseline" "$capture" > "$diff" 2>&1 || true
            return 1
        fi
    fi
    
    # Use ImageMagick for image comparison if available
    if command -v compare &> /dev/null; then
        # Get the difference metric
        local metric=$(compare -metric RMSE "$baseline" "$capture" "$diff" 2>&1 | awk '{print $2}' | tr -d '()')
        
        # Convert to percentage
        local percentage=$(echo "$metric * 100" | bc -l 2>/dev/null || echo "100")
        
        # Check if difference is within threshold
        if (( $(echo "$percentage < $threshold" | bc -l 2>/dev/null || echo 0) )); then
            return 0
        else
            return 1
        fi
    else
        # Fallback to simple file comparison
        if cmp -s "$baseline" "$capture"; then
            return 0
        else
            return 1
        fi
    fi
}

# Function to run visual regression test
run_visual_test() {
    local test_file="$1"
    local test_name="$(basename "$test_file" .kry)"
    local krb_file="${test_file%.kry}.krb"
    
    log_info "Running visual regression test: $test_name"
    
    # Compile the test
    if ! cargo run --bin kryon-compiler -- compile "$test_file" "$krb_file" &>/dev/null; then
        log_error "Failed to compile $test_name"
        return 1
    fi
    
    # Capture screenshot
    local capture_file="$CAPTURES_DIR/${test_name}.png"
    if ! capture_screenshot "$test_name" "$krb_file" "$capture_file"; then
        log_error "Failed to capture screenshot for $test_name"
        return 1
    fi
    
    # Check for baseline
    local baseline_file="$BASELINE_DIR/${test_name}.png"
    if [ ! -f "$baseline_file" ]; then
        # No baseline exists, create it
        cp "$capture_file" "$baseline_file"
        log_warning "Created new baseline for $test_name"
        return 0
    fi
    
    # Compare with baseline
    local diff_file="$DIFFS_DIR/${test_name}_diff.png"
    if compare_images "$baseline_file" "$capture_file" "$diff_file"; then
        log_success "Visual test passed: $test_name"
        rm -f "$diff_file"  # Remove diff file if test passed
        return 0
    else
        log_error "Visual regression detected: $test_name"
        log_info "  Baseline: $baseline_file"
        log_info "  Capture:  $capture_file"
        log_info "  Diff:     $diff_file"
        return 1
    fi
}

# Function to update baseline
update_baseline() {
    local test_name="$1"
    local capture_file="$CAPTURES_DIR/${test_name}.png"
    local baseline_file="$BASELINE_DIR/${test_name}.png"
    
    if [ -f "$capture_file" ]; then
        cp "$capture_file" "$baseline_file"
        log_success "Updated baseline for $test_name"
    else
        log_error "No capture found for $test_name"
        return 1
    fi
}

# Function to generate HTML report
generate_html_report() {
    local report_file="$SCRIPT_DIR/../visual_regression_report.html"
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Kryon Visual Regression Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        h1 { color: #333; }
        .test { margin: 20px 0; padding: 20px; border: 1px solid #ddd; }
        .pass { background-color: #e8f5e9; }
        .fail { background-color: #ffebee; }
        .images { display: flex; gap: 20px; margin-top: 10px; }
        .image-container { text-align: center; }
        img { max-width: 300px; border: 1px solid #ccc; }
        .diff { border-color: #f44336; }
    </style>
</head>
<body>
    <h1>Kryon Visual Regression Report</h1>
    <p>Generated: $(date)</p>
EOF

    # Add test results
    for capture in "$CAPTURES_DIR"/*.png; do
        [ -f "$capture" ] || continue
        
        local test_name="$(basename "$capture" .png)"
        local baseline="$BASELINE_DIR/${test_name}.png"
        local diff="$DIFFS_DIR/${test_name}_diff.png"
        
        if [ -f "$diff" ]; then
            echo "<div class='test fail'>" >> "$report_file"
            echo "<h3>❌ $test_name - FAILED</h3>" >> "$report_file"
        else
            echo "<div class='test pass'>" >> "$report_file"
            echo "<h3>✅ $test_name - PASSED</h3>" >> "$report_file"
        fi
        
        echo "<div class='images'>" >> "$report_file"
        
        # Baseline
        if [ -f "$baseline" ]; then
            echo "<div class='image-container'>" >> "$report_file"
            echo "<h4>Baseline</h4>" >> "$report_file"
            echo "<img src='$baseline' alt='Baseline'>" >> "$report_file"
            echo "</div>" >> "$report_file"
        fi
        
        # Current
        echo "<div class='image-container'>" >> "$report_file"
        echo "<h4>Current</h4>" >> "$report_file"
        echo "<img src='$capture' alt='Current'>" >> "$report_file"
        echo "</div>" >> "$report_file"
        
        # Diff
        if [ -f "$diff" ]; then
            echo "<div class='image-container'>" >> "$report_file"
            echo "<h4>Difference</h4>" >> "$report_file"
            echo "<img src='$diff' alt='Diff' class='diff'>" >> "$report_file"
            echo "</div>" >> "$report_file"
        fi
        
        echo "</div></div>" >> "$report_file"
    done
    
    echo "</body></html>" >> "$report_file"
    log_success "HTML report generated: $report_file"
}

# Main function
main() {
    local mode="${1:-test}"
    local specific_test="$2"
    
    echo -e "${BLUE}Kryon Visual Regression Testing${NC}"
    echo -e "${BLUE}==============================${NC}"
    echo
    
    case "$mode" in
        test)
            # Run visual regression tests
            local failed_tests=0
            local passed_tests=0
            
            if [ -n "$specific_test" ]; then
                # Run specific test
                if run_visual_test "$specific_test"; then
                    ((passed_tests++))
                else
                    ((failed_tests++))
                fi
            else
                # Run all tests
                for test_file in "$PROJECT_ROOT"/tests/test_suites/*.kry; do
                    [ -f "$test_file" ] || continue
                    
                    if run_visual_test "$test_file"; then
                        ((passed_tests++))
                    else
                        ((failed_tests++))
                    fi
                done
            fi
            
            # Generate report
            generate_html_report
            
            # Summary
            echo
            echo -e "${BLUE}Visual Regression Summary${NC}"
            echo -e "${GREEN}Passed: $passed_tests${NC}"
            echo -e "${RED}Failed: $failed_tests${NC}"
            
            # Generate JSON results
            cat > "$RESULTS_FILE" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "passed": $passed_tests,
    "failed": $failed_tests,
    "total": $((passed_tests + failed_tests))
}
EOF
            
            # Exit with error if tests failed
            [ $failed_tests -eq 0 ]
            ;;
            
        update)
            # Update baseline(s)
            if [ -n "$specific_test" ]; then
                local test_name="$(basename "$specific_test" .kry)"
                update_baseline "$test_name"
            else
                # Update all baselines
                for capture in "$CAPTURES_DIR"/*.png; do
                    [ -f "$capture" ] || continue
                    local test_name="$(basename "$capture" .png)"
                    update_baseline "$test_name"
                done
            fi
            ;;
            
        clean)
            # Clean captures and diffs
            rm -rf "$CAPTURES_DIR"/* "$DIFFS_DIR"/*
            log_success "Cleaned capture and diff directories"
            ;;
            
        reset)
            # Reset all baselines
            read -p "Are you sure you want to reset all baselines? (y/N) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                rm -rf "$BASELINE_DIR"/*
                log_success "Reset all baselines"
            else
                log_info "Baseline reset cancelled"
            fi
            ;;
            
        *)
            echo "Usage: $0 [test|update|clean|reset] [specific_test.kry]"
            echo
            echo "Commands:"
            echo "  test    - Run visual regression tests (default)"
            echo "  update  - Update baseline image(s)"
            echo "  clean   - Remove capture and diff images"
            echo "  reset   - Reset all baseline images"
            echo
            echo "Examples:"
            echo "  $0                                          # Run all tests"
            echo "  $0 test path/to/specific_test.kry          # Run specific test"
            echo "  $0 update                                   # Update all baselines"
            echo "  $0 update specific_test                     # Update specific baseline"
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
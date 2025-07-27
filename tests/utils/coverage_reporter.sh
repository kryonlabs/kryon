#!/bin/bash
# Test Coverage Reporter for Kryon
# Generates comprehensive test coverage reports using cargo-tarpaulin

set -e

# Configuration
SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
COVERAGE_DIR="$SCRIPT_DIR/../coverage"
REPORT_FILE="$COVERAGE_DIR/coverage_report.html"
JSON_FILE="$COVERAGE_DIR/coverage.json"
XML_FILE="$COVERAGE_DIR/coverage.xml"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m'

# Create coverage directory
mkdir -p "$COVERAGE_DIR"

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

# Check dependencies
check_dependencies() {
    log_info "Checking coverage dependencies..."
    
    local missing_deps=()
    
    # Check for cargo-tarpaulin
    if ! command -v cargo-tarpaulin &> /dev/null; then
        log_warning "cargo-tarpaulin not found, attempting to install..."
        if ! cargo install cargo-tarpaulin; then
            missing_deps+=("cargo-tarpaulin")
        fi
    fi
    
    # Check for cargo-llvm-cov (alternative coverage tool)
    if ! command -v cargo-llvm-cov &> /dev/null; then
        log_info "cargo-llvm-cov not found (optional alternative)"
    fi
    
    # Check for grcov (another alternative)
    if ! command -v grcov &> /dev/null; then
        log_info "grcov not found (optional alternative)"
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        log_error "Missing required dependencies: ${missing_deps[*]}"
        log_info "Please install missing dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  cargo install $dep"
        done
        return 1
    fi
    
    log_success "All coverage dependencies available"
    return 0
}

# Run coverage for specific crate
run_crate_coverage() {
    local crate_name="$1"
    local output_dir="$COVERAGE_DIR/crates/$crate_name"
    
    mkdir -p "$output_dir"
    
    log_info "Running coverage for crate: $crate_name"
    
    # Check if crate exists
    if [ ! -d "crates/$crate_name" ]; then
        log_error "Crate not found: $crate_name"
        return 1
    fi
    
    # Run tarpaulin for specific crate
    local start_time=$(date +%s)
    
    if cargo tarpaulin \
        --package "$crate_name" \
        --out Html \
        --output-dir "$output_dir" \
        --exclude-files "target/*" \
        --exclude-files "*/tests/*" \
        --exclude-files "*/benches/*" \
        --timeout 120 \
        --verbose; then
        
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        log_success "Coverage completed for $crate_name in ${duration}s"
        
        # Extract coverage percentage
        local coverage_percent=""
        if [ -f "$output_dir/tarpaulin-report.html" ]; then
            coverage_percent=$(grep -o '[0-9]\+\.[0-9]\+%' "$output_dir/tarpaulin-report.html" | head -n1)
        fi
        
        echo "{\"crate\":\"$crate_name\",\"coverage\":\"$coverage_percent\",\"duration\":$duration}" >> "$COVERAGE_DIR/crate_results.jsonl"
        
        return 0
    else
        log_error "Coverage failed for $crate_name"
        return 1
    fi
}

# Run workspace-wide coverage
run_workspace_coverage() {
    log_info "Running workspace-wide coverage analysis..."
    
    local start_time=$(date +%s)
    
    # Clean previous coverage data
    cargo clean
    
    # Run tarpaulin for entire workspace
    if cargo tarpaulin \
        --workspace \
        --out Html \
        --out Json \
        --out Xml \
        --output-dir "$COVERAGE_DIR" \
        --exclude-files "target/*" \
        --exclude-files "*/tests/*" \
        --exclude-files "*/benches/*" \
        --exclude-files "examples/*" \
        --exclude-files "benches/*" \
        --timeout 300 \
        --verbose; then
        
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        log_success "Workspace coverage completed in ${duration}s"
        
        # Move and rename files
        if [ -f "$COVERAGE_DIR/tarpaulin-report.html" ]; then
            mv "$COVERAGE_DIR/tarpaulin-report.html" "$REPORT_FILE"
        fi
        
        if [ -f "$COVERAGE_DIR/tarpaulin-report.json" ]; then
            mv "$COVERAGE_DIR/tarpaulin-report.json" "$JSON_FILE"
        fi
        
        if [ -f "$COVERAGE_DIR/cobertura.xml" ]; then
            mv "$COVERAGE_DIR/cobertura.xml" "$XML_FILE"
        fi
        
        return 0
    else
        log_error "Workspace coverage failed"
        return 1
    fi
}

# Analyze coverage results
analyze_coverage() {
    log_info "Analyzing coverage results..."
    
    if [ ! -f "$JSON_FILE" ]; then
        log_warning "JSON coverage file not found, skipping analysis"
        return 0
    fi
    
    # Extract key metrics using jq if available
    if command -v jq &> /dev/null; then
        local total_lines=$(jq '.files | map(.summary.lines.total) | add' "$JSON_FILE" 2>/dev/null || echo "unknown")
        local covered_lines=$(jq '.files | map(.summary.lines.covered) | add' "$JSON_FILE" 2>/dev/null || echo "unknown")
        local coverage_percent=$(jq '.files | map(.summary.lines.percent) | add / length' "$JSON_FILE" 2>/dev/null || echo "unknown")
        
        log_success "Coverage Analysis:"
        echo -e "  Total Lines: $total_lines"
        echo -e "  Covered Lines: $covered_lines" 
        echo -e "  Coverage Percentage: ${coverage_percent}%"
        
        # Generate summary JSON
        cat > "$COVERAGE_DIR/summary.json" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "total_lines": $total_lines,
    "covered_lines": $covered_lines,
    "coverage_percent": $coverage_percent,
    "report_files": {
        "html": "$REPORT_FILE",
        "json": "$JSON_FILE",
        "xml": "$XML_FILE"
    }
}
EOF
        
        # Check coverage thresholds
        local threshold_warning=70
        local threshold_error=50
        
        if (( $(echo "$coverage_percent < $threshold_error" | bc -l 2>/dev/null || echo 0) )); then
            log_error "Coverage below critical threshold: ${coverage_percent}% < ${threshold_error}%"
            return 1
        elif (( $(echo "$coverage_percent < $threshold_warning" | bc -l 2>/dev/null || echo 0) )); then
            log_warning "Coverage below warning threshold: ${coverage_percent}% < ${threshold_warning}%"
        else
            log_success "Coverage meets requirements: ${coverage_percent}%"
        fi
        
    else
        log_warning "jq not available, install for detailed analysis"
    fi
    
    return 0
}

# Generate coverage badge
generate_badge() {
    local coverage_percent="$1"
    local badge_file="$COVERAGE_DIR/coverage-badge.svg"
    
    # Determine badge color based on coverage
    local color="red"
    if (( $(echo "$coverage_percent >= 80" | bc -l 2>/dev/null || echo 0) )); then
        color="brightgreen"
    elif (( $(echo "$coverage_percent >= 70" | bc -l 2>/dev/null || echo 0) )); then
        color="yellow"
    elif (( $(echo "$coverage_percent >= 50" | bc -l 2>/dev/null || echo 0) )); then
        color="orange"
    fi
    
    # Generate badge SVG
    cat > "$badge_file" << EOF
<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="104" height="20">
    <linearGradient id="b" x2="0" y2="100%">
        <stop offset="0" stop-color="#bbb" stop-opacity=".1"/>
        <stop offset="1" stop-opacity=".1"/>
    </linearGradient>
    <clipPath id="a">
        <rect width="104" height="20" rx="3" fill="#fff"/>
    </clipPath>
    <g clip-path="url(#a)">
        <path fill="#555" d="M0 0h63v20H0z"/>
        <path fill="$color" d="M63 0h41v20H63z"/>
        <path fill="url(#b)" d="M0 0h104v20H0z"/>
    </g>
    <g fill="#fff" text-anchor="middle" font-family="DejaVu Sans,Verdana,Geneva,sans-serif" font-size="110">
        <text x="325" y="150" fill="#010101" fill-opacity=".3" transform="scale(.1)" textLength="530">coverage</text>
        <text x="325" y="140" transform="scale(.1)" textLength="530">coverage</text>
        <text x="825" y="150" fill="#010101" fill-opacity=".3" transform="scale(.1)" textLength="310">${coverage_percent}%</text>
        <text x="825" y="140" transform="scale(.1)" textLength="310">${coverage_percent}%</text>
    </g>
</svg>
EOF
    
    log_success "Coverage badge generated: $badge_file"
}

# Generate enhanced HTML report
generate_enhanced_report() {
    local enhanced_report="$COVERAGE_DIR/enhanced_coverage_report.html"
    local timestamp=$(date)
    
    log_info "Generating enhanced coverage report..."
    
    # Get summary data
    local total_lines="unknown"
    local covered_lines="unknown"
    local coverage_percent="unknown"
    
    if [ -f "$COVERAGE_DIR/summary.json" ] && command -v jq &> /dev/null; then
        total_lines=$(jq -r '.total_lines' "$COVERAGE_DIR/summary.json" 2>/dev/null || echo "unknown")
        covered_lines=$(jq -r '.covered_lines' "$COVERAGE_DIR/summary.json" 2>/dev/null || echo "unknown")
        coverage_percent=$(jq -r '.coverage_percent' "$COVERAGE_DIR/summary.json" 2>/dev/null || echo "unknown")
    fi
    
    cat > "$enhanced_report" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Kryon Test Coverage Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .header { background-color: #2c3e50; color: white; padding: 20px; border-radius: 8px; }
        .section { background-color: white; margin: 20px 0; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .metric { display: inline-block; margin: 10px; padding: 15px; background-color: #ecf0f1; border-radius: 8px; text-align: center; min-width: 120px; }
        .metric-value { font-size: 24px; font-weight: bold; color: #2c3e50; }
        .metric-label { font-size: 12px; color: #7f8c8d; }
        .coverage-high { color: #27ae60; }
        .coverage-medium { color: #f39c12; }
        .coverage-low { color: #e74c3c; }
        .progress-bar { width: 100%; height: 20px; background-color: #ecf0f1; border-radius: 10px; overflow: hidden; margin: 10px 0; }
        .progress-fill { height: 100%; transition: width 0.3s ease; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .file-coverage { padding: 5px; border-radius: 3px; }
        .crate-section { margin: 15px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>📊 Kryon Test Coverage Report</h1>
        <p>Generated: $timestamp</p>
        <p>Comprehensive coverage analysis for the Kryon UI framework</p>
    </div>
    
    <div class="section">
        <h2>📈 Coverage Summary</h2>
        <div class="metric">
            <div class="metric-value coverage-$([ "$coverage_percent" != "unknown" ] && (( $(echo "$coverage_percent >= 80" | bc -l 2>/dev/null || echo 0) )) && echo "high" || (( $(echo "$coverage_percent >= 60" | bc -l 2>/dev/null || echo 0) )) && echo "medium" || echo "low")">
                ${coverage_percent}%
            </div>
            <div class="metric-label">Overall Coverage</div>
        </div>
        
        <div class="metric">
            <div class="metric-value">$covered_lines</div>
            <div class="metric-label">Lines Covered</div>
        </div>
        
        <div class="metric">
            <div class="metric-value">$total_lines</div>
            <div class="metric-label">Total Lines</div>
        </div>
        
        <div class="progress-bar">
            <div class="progress-fill coverage-$([ "$coverage_percent" != "unknown" ] && (( $(echo "$coverage_percent >= 80" | bc -l 2>/dev/null || echo 0) )) && echo "high" || echo "medium")" 
                 style="width: ${coverage_percent}%; background-color: $([ "$coverage_percent" != "unknown" ] && (( $(echo "$coverage_percent >= 80" | bc -l 2>/dev/null || echo 0) )) && echo "#27ae60" || (( $(echo "$coverage_percent >= 60" | bc -l 2>/dev/null || echo 0) )) && echo "#f39c12" || echo "#e74c3c");"></div>
        </div>
    </div>
    
    <div class="section">
        <h2>📁 Crate Coverage</h2>
EOF

    # Add crate-specific coverage if available
    if [ -f "$COVERAGE_DIR/crate_results.jsonl" ]; then
        echo "<table>" >> "$enhanced_report"
        echo "<tr><th>Crate</th><th>Coverage</th><th>Duration</th></tr>" >> "$enhanced_report"
        
        while read -r line; do
            if command -v jq &> /dev/null; then
                local crate_name=$(echo "$line" | jq -r '.crate' 2>/dev/null || echo "unknown")
                local crate_coverage=$(echo "$line" | jq -r '.coverage' 2>/dev/null || echo "unknown")
                local crate_duration=$(echo "$line" | jq -r '.duration' 2>/dev/null || echo "unknown")
                
                echo "<tr>" >> "$enhanced_report"
                echo "<td>$crate_name</td>" >> "$enhanced_report"
                echo "<td class='file-coverage'>$crate_coverage</td>" >> "$enhanced_report"
                echo "<td>${crate_duration}s</td>" >> "$enhanced_report"
                echo "</tr>" >> "$enhanced_report"
            fi
        done < "$COVERAGE_DIR/crate_results.jsonl"
        
        echo "</table>" >> "$enhanced_report"
    else
        echo "<p>Individual crate coverage data not available. Run with --per-crate option to generate detailed crate reports.</p>" >> "$enhanced_report"
    fi
    
    cat >> "$enhanced_report" << EOF
    </div>
    
    <div class="section">
        <h2>📋 Report Files</h2>
        <ul>
            <li><a href="$(basename "$REPORT_FILE")">Detailed HTML Report</a></li>
            <li><a href="$(basename "$JSON_FILE")">JSON Data</a> (for CI/CD integration)</li>
            <li><a href="$(basename "$XML_FILE")">XML Report</a> (Cobertura format)</li>
            <li><a href="coverage-badge.svg">Coverage Badge</a></li>
        </ul>
    </div>
    
    <div class="section">
        <h2>🎯 Coverage Guidelines</h2>
        <ul>
            <li><strong>Excellent (≥80%):</strong> <span class="coverage-high">Well-tested code</span></li>
            <li><strong>Good (60-79%):</strong> <span class="coverage-medium">Adequate testing</span></li>
            <li><strong>Needs Improvement (&lt;60%):</strong> <span class="coverage-low">Requires more tests</span></li>
        </ul>
        
        <h3>🔧 Improving Coverage</h3>
        <ul>
            <li>Add unit tests for uncovered functions</li>
            <li>Test error handling paths</li>
            <li>Add integration tests for component interactions</li>
            <li>Test edge cases and boundary conditions</li>
        </ul>
    </div>
    
    <footer style="text-align: center; margin-top: 40px; color: #7f8c8d;">
        <p>Generated by Kryon Test Coverage Reporter</p>
    </footer>
</body>
</html>
EOF
    
    log_success "Enhanced report generated: $enhanced_report"
    
    # Generate badge if coverage is available
    if [ "$coverage_percent" != "unknown" ]; then
        generate_badge "$coverage_percent"
    fi
}

# Run coverage for individual crates
run_per_crate_coverage() {
    log_info "Running per-crate coverage analysis..."
    
    # Clean previous results
    rm -f "$COVERAGE_DIR/crate_results.jsonl"
    
    # Get list of crates
    local crates=()
    for crate_dir in crates/*/; do
        if [ -d "$crate_dir" ] && [ -f "$crate_dir/Cargo.toml" ]; then
            local crate_name=$(basename "$crate_dir")
            crates+=("$crate_name")
        fi
    done
    
    log_info "Found ${#crates[@]} crates: ${crates[*]}"
    
    local successful=0
    local failed=0
    
    for crate in "${crates[@]}"; do
        if run_crate_coverage "$crate"; then
            ((successful++))
        else
            ((failed++))
        fi
    done
    
    log_success "Per-crate coverage completed: $successful successful, $failed failed"
}

# Main function
main() {
    local mode="${1:-workspace}"
    local options="${*:2}"
    
    echo -e "${PURPLE}Kryon Test Coverage Reporter${NC}"
    echo -e "${PURPLE}============================${NC}"
    echo
    
    # Change to project root
    cd "$PROJECT_ROOT"
    
    # Check dependencies
    if ! check_dependencies; then
        exit 1
    fi
    
    case "$mode" in
        workspace)
            if run_workspace_coverage; then
                analyze_coverage
                generate_enhanced_report
            fi
            ;;
        per-crate)
            run_per_crate_coverage
            generate_enhanced_report
            ;;
        both)
            if run_workspace_coverage; then
                analyze_coverage
            fi
            run_per_crate_coverage
            generate_enhanced_report
            ;;
        analyze)
            analyze_coverage
            generate_enhanced_report
            ;;
        clean)
            rm -rf "$COVERAGE_DIR"
            log_success "Coverage directory cleaned"
            ;;
        install-deps)
            log_info "Installing coverage dependencies..."
            cargo install cargo-tarpaulin
            log_success "Dependencies installed"
            ;;
        *)
            echo "Usage: $0 [workspace|per-crate|both|analyze|clean|install-deps]"
            echo
            echo "Commands:"
            echo "  workspace    - Run workspace-wide coverage (default)"
            echo "  per-crate    - Run coverage for each crate individually"
            echo "  both         - Run both workspace and per-crate coverage"
            echo "  analyze      - Analyze existing coverage results"
            echo "  clean        - Clean coverage directory"
            echo "  install-deps - Install required coverage tools"
            echo
            echo "Examples:"
            echo "  $0                    # Run workspace coverage"
            echo "  $0 per-crate         # Run per-crate coverage"
            echo "  $0 both              # Run comprehensive coverage"
            exit 1
            ;;
    esac
    
    if [ -f "$REPORT_FILE" ]; then
        echo
        log_success "Coverage reports generated in: $COVERAGE_DIR"
        echo -e "  HTML Report: ${BLUE}$REPORT_FILE${NC}"
        echo -e "  JSON Data:   ${BLUE}$JSON_FILE${NC}"
        echo -e "  XML Report:  ${BLUE}$XML_FILE${NC}"
        
        if command -v xdg-open &> /dev/null; then
            echo
            echo -e "${YELLOW}Open in browser:${NC} xdg-open $REPORT_FILE"
        elif command -v open &> /dev/null; then
            echo
            echo -e "${YELLOW}Open in browser:${NC} open $REPORT_FILE"
        fi
    fi
}

# Run main function
main "$@"
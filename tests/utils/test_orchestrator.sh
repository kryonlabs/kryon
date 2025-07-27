#!/bin/bash
# Test Orchestrator - Central coordination for all testing utilities
# Manages and coordinates all testing tools and workflows

set -e

# Configuration
SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
UTILS_DIR="$SCRIPT_DIR"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Available tools
declare -A TOOLS=(
    ["analyzer"]="$UTILS_DIR/test_analyzer.py"
    ["visual"]="$UTILS_DIR/visual_regression.sh"
    ["generator"]="$UTILS_DIR/test_data_generator.py"
    ["platform"]="$UTILS_DIR/cross_platform_test.sh"
    ["coverage"]="$UTILS_DIR/coverage_reporter.sh"
    ["debug"]="$UTILS_DIR/debug_helper.py"
    ["suite"]="$SCRIPT_DIR/../run_test_suite.sh"
)

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_title() {
    echo -e "${PURPLE}$1${NC}"
}

# Check tool availability
check_tool() {
    local tool_name="$1"
    local tool_path="${TOOLS[$tool_name]}"
    
    if [ -z "$tool_path" ]; then
        return 1
    fi
    
    if [ -f "$tool_path" ] && [ -x "$tool_path" ]; then
        return 0
    else
        return 1
    fi
}

# Display available tools
show_tools() {
    log_title "📋 Available Testing Tools"
    echo
    
    for tool in "${!TOOLS[@]}"; do
        local path="${TOOLS[$tool]}"
        local status="❌"
        local description=""
        
        if check_tool "$tool"; then
            status="✅"
        fi
        
        case "$tool" in
            analyzer)
                description="Test result analysis and reporting"
                ;;
            visual)
                description="Visual regression testing"
                ;;
            generator)
                description="Test data and stress test generation"
                ;;
            platform)
                description="Cross-platform compatibility testing"
                ;;
            coverage)
                description="Code coverage analysis and reporting"
                ;;
            debug)
                description="Interactive debugging for test failures"
                ;;
            suite)
                description="Main test suite runner"
                ;;
        esac
        
        printf "  %s %-12s %s\n" "$status" "$tool" "$description"
    done
    echo
}

# Run comprehensive testing workflow
run_comprehensive_workflow() {
    local workflow_dir="$PROJECT_ROOT/tests/workflow_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$workflow_dir"
    
    log_title "🚀 Running Comprehensive Testing Workflow"
    echo -e "Results will be saved to: ${BLUE}$workflow_dir${NC}"
    echo
    
    local start_time=$(date +%s)
    local failed_steps=0
    
    # Step 1: Cross-platform validation
    log_info "Step 1: Cross-platform validation"
    if check_tool "platform" && "${TOOLS[platform]}" system > "$workflow_dir/platform_info.json" 2>&1; then
        log_success "Platform validation completed"
    else
        log_error "Platform validation failed"
        ((failed_steps++))
    fi
    
    # Step 2: Run main test suite
    log_info "Step 2: Running main test suite"
    if check_tool "suite" && "${TOOLS[suite]}" > "$workflow_dir/test_suite.log" 2>&1; then
        log_success "Test suite completed"
    else
        log_error "Test suite failed"
        ((failed_steps++))
    fi
    
    # Step 3: Coverage analysis
    log_info "Step 3: Coverage analysis"
    if check_tool "coverage" && "${TOOLS[coverage]}" workspace > "$workflow_dir/coverage.log" 2>&1; then
        log_success "Coverage analysis completed"
        # Copy coverage results
        if [ -d "$PROJECT_ROOT/tests/coverage" ]; then
            cp -r "$PROJECT_ROOT/tests/coverage" "$workflow_dir/"
        fi
    else
        log_warning "Coverage analysis failed or skipped"
    fi
    
    # Step 4: Visual regression testing
    log_info "Step 4: Visual regression testing"
    if check_tool "visual" && "${TOOLS[visual]}" test > "$workflow_dir/visual_regression.log" 2>&1; then
        log_success "Visual regression testing completed"
    else
        log_warning "Visual regression testing failed or skipped"
    fi
    
    # Step 5: Test result analysis
    log_info "Step 5: Analyzing test results"
    if check_tool "analyzer" && python3 "${TOOLS[analyzer]}" --results-dir "$PROJECT_ROOT/tests/results" --output-dir "$workflow_dir/analysis" --charts > "$workflow_dir/analysis.log" 2>&1; then
        log_success "Test analysis completed"
    else
        log_warning "Test analysis failed or skipped"
    fi
    
    # Generate workflow summary
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    cat > "$workflow_dir/workflow_summary.json" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "duration_seconds": $duration,
    "failed_steps": $failed_steps,
    "workflow_directory": "$workflow_dir",
    "steps": [
        {"name": "platform_validation", "status": "$([ $failed_steps -eq 0 ] && echo "success" || echo "partial")"},
        {"name": "test_suite", "status": "$([ -f "$workflow_dir/test_suite.log" ] && echo "completed" || echo "failed")"},
        {"name": "coverage_analysis", "status": "$([ -f "$workflow_dir/coverage.log" ] && echo "completed" || echo "skipped")"},
        {"name": "visual_regression", "status": "$([ -f "$workflow_dir/visual_regression.log" ] && echo "completed" || echo "skipped")"},
        {"name": "result_analysis", "status": "$([ -f "$workflow_dir/analysis.log" ] && echo "completed" || echo "skipped")"}
    ]
}
EOF
    
    # Generate HTML summary report
    generate_workflow_report "$workflow_dir"
    
    echo
    log_title "📊 Workflow Summary"
    echo -e "Duration: ${duration}s"
    echo -e "Failed steps: $failed_steps"
    echo -e "Results: ${BLUE}$workflow_dir${NC}"
    
    if [ $failed_steps -eq 0 ]; then
        log_success "Comprehensive testing workflow completed successfully"
        return 0
    else
        log_warning "Workflow completed with $failed_steps failed steps"
        return 1
    fi
}

# Generate workflow HTML report
generate_workflow_report() {
    local workflow_dir="$1"
    local report_file="$workflow_dir/workflow_report.html"
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Kryon Testing Workflow Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .header { background-color: #3498db; color: white; padding: 20px; border-radius: 8px; }
        .section { background-color: white; margin: 20px 0; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .step { margin: 10px 0; padding: 15px; border-radius: 8px; }
        .success { background-color: #d5f4e6; border-left: 4px solid #27ae60; }
        .warning { background-color: #fef9e7; border-left: 4px solid #f39c12; }
        .error { background-color: #fdf2f2; border-left: 4px solid #e74c3c; }
        .file-list { background-color: #f8f9fa; padding: 10px; border-radius: 4px; font-family: monospace; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🧪 Kryon Testing Workflow Report</h1>
        <p>Generated: $(date)</p>
        <p>Workflow Directory: $(basename "$workflow_dir")</p>
    </div>
    
    <div class="section">
        <h2>📊 Workflow Steps</h2>
EOF

    # Add workflow steps
    local steps=(
        "platform_validation:Cross-Platform Validation"
        "test_suite:Main Test Suite"
        "coverage_analysis:Coverage Analysis" 
        "visual_regression:Visual Regression Testing"
        "result_analysis:Result Analysis"
    )
    
    for step_info in "${steps[@]}"; do
        IFS=':' read -r step_id step_name <<< "$step_info"
        local log_file="$workflow_dir/${step_id}.log"
        local status_class="error"
        local status_text="❌ Failed"
        
        if [ -f "$log_file" ]; then
            status_class="success"
            status_text="✅ Completed"
        elif [[ "$step_id" == "coverage_analysis" || "$step_id" == "visual_regression" || "$step_id" == "result_analysis" ]]; then
            status_class="warning"
            status_text="⚠️ Skipped"
        fi
        
        echo "<div class='step $status_class'>" >> "$report_file"
        echo "<h3>$status_text $step_name</h3>" >> "$report_file"
        
        if [ -f "$log_file" ]; then
            echo "<p>Log file: <a href='$(basename "$log_file")'>$(basename "$log_file")</a></p>" >> "$report_file"
        fi
        
        echo "</div>" >> "$report_file"
    done
    
    cat >> "$report_file" << EOF
    </div>
    
    <div class="section">
        <h2>📁 Generated Files</h2>
        <div class="file-list">
EOF

    # List all generated files
    find "$workflow_dir" -type f | sort | while read -r file; do
        local rel_path=$(basename "$file")
        echo "<a href='$rel_path'>$rel_path</a><br>" >> "$report_file"
    done
    
    cat >> "$report_file" << EOF
        </div>
    </div>
    
    <div class="section">
        <h2>🔧 Tools Used</h2>
        <table>
            <tr><th>Tool</th><th>Description</th><th>Status</th></tr>
EOF

    for tool in "${!TOOLS[@]}"; do
        local status="✅ Available"
        if ! check_tool "$tool"; then
            status="❌ Not Available"
        fi
        
        local description=""
        case "$tool" in
            analyzer) description="Test result analysis and reporting" ;;
            visual) description="Visual regression testing" ;;
            generator) description="Test data and stress test generation" ;;
            platform) description="Cross-platform compatibility testing" ;;
            coverage) description="Code coverage analysis and reporting" ;;
            debug) description="Interactive debugging for test failures" ;;
            suite) description="Main test suite runner" ;;
        esac
        
        echo "<tr><td>$tool</td><td>$description</td><td>$status</td></tr>" >> "$report_file"
    done
    
    cat >> "$report_file" << EOF
        </table>
    </div>
    
    <footer style="text-align: center; margin-top: 40px; color: #7f8c8d;">
        <p>Generated by Kryon Test Orchestrator</p>
    </footer>
</body>
</html>
EOF

    log_success "Workflow report generated: $report_file"
}

# Interactive tool selection
interactive_mode() {
    log_title "🎮 Interactive Testing Mode"
    echo
    
    while true; do
        echo "Available tools:"
        local i=1
        local tool_list=()
        
        for tool in "${!TOOLS[@]}"; do
            local status="❌"
            if check_tool "$tool"; then
                status="✅"
            fi
            echo "  $i) $status $tool"
            tool_list+=("$tool")
            ((i++))
        done
        
        echo "  $i) Run comprehensive workflow"
        ((i++))
        echo "  $i) Show tool status"
        ((i++))
        echo "  $i) Exit"
        echo
        
        read -p "Select option (1-$i): " choice
        
        if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le $((i-1)) ]; then
            if [ "$choice" -eq $((i-3)) ]; then
                # Comprehensive workflow
                run_comprehensive_workflow
            elif [ "$choice" -eq $((i-2)) ]; then
                # Show tool status
                show_tools
            elif [ "$choice" -eq $((i-1)) ]; then
                # Exit
                log_info "Exiting interactive mode"
                break
            else
                # Run specific tool
                local selected_tool="${tool_list[$((choice-1))]}"
                local tool_path="${TOOLS[$selected_tool]}"
                
                if check_tool "$selected_tool"; then
                    log_info "Running $selected_tool..."
                    echo
                    
                    case "$selected_tool" in
                        suite)
                            "$tool_path"
                            ;;
                        analyzer)
                            python3 "$tool_path" --charts
                            ;;
                        coverage)
                            "$tool_path" workspace
                            ;;
                        platform)
                            "$tool_path" full
                            ;;
                        visual)
                            "$tool_path" test
                            ;;
                        generator)
                            echo "Usage examples:"
                            echo "  python3 $tool_path --type stress --count 100 --output test_stress.kry"
                            echo "  python3 $tool_path --type edge --edge-case unicode --output test_unicode.kry"
                            ;;
                        debug)
                            echo "Usage examples:"
                            echo "  python3 $tool_path your_test.kry --mode interactive"
                            echo "  python3 $tool_path your_test.kry --mode compile"
                            ;;
                    esac
                else
                    log_error "Tool $selected_tool is not available"
                fi
            fi
        else
            log_error "Invalid selection"
        fi
        
        echo
    done
}

# Main function
main() {
    local command="${1:-interactive}"
    
    log_title "🎯 Kryon Test Orchestrator"
    log_title "=========================="
    echo
    
    # Change to project root
    cd "$PROJECT_ROOT"
    
    case "$command" in
        interactive|i)
            interactive_mode
            ;;
        workflow|w)
            run_comprehensive_workflow
            ;;
        tools|t)
            show_tools
            ;;
        check|c)
            log_info "Checking tool availability..."
            local all_available=true
            
            for tool in "${!TOOLS[@]}"; do
                if check_tool "$tool"; then
                    log_success "$tool is available"
                else
                    log_error "$tool is not available"
                    all_available=false
                fi
            done
            
            if [ "$all_available" = true ]; then
                log_success "All tools are available"
                exit 0
            else
                log_error "Some tools are missing"
                exit 1
            fi
            ;;
        help|h)
            echo "Usage: $0 [command]"
            echo
            echo "Commands:"
            echo "  interactive, i  - Interactive tool selection (default)"
            echo "  workflow, w     - Run comprehensive testing workflow"
            echo "  tools, t        - Show available tools"
            echo "  check, c        - Check tool availability"
            echo "  help, h         - Show this help"
            echo
            echo "Tool-specific commands:"
            echo "  analyzer        - Run test result analyzer"
            echo "  visual          - Run visual regression tests"
            echo "  generator       - Run test data generator"
            echo "  platform        - Run cross-platform tests"
            echo "  coverage        - Run coverage analysis"
            echo "  debug           - Run debug helper"
            echo "  suite           - Run main test suite"
            ;;
        analyzer|visual|generator|platform|coverage|debug|suite)
            local tool_path="${TOOLS[$command]}"
            
            if check_tool "$command"; then
                log_info "Running $command..."
                shift  # Remove command from arguments
                
                case "$command" in
                    analyzer)
                        python3 "$tool_path" "$@"
                        ;;
                    generator|debug)
                        python3 "$tool_path" "$@"
                        ;;
                    *)
                        "$tool_path" "$@"
                        ;;
                esac
            else
                log_error "Tool $command is not available"
                exit 1
            fi
            ;;
        *)
            log_error "Unknown command: $command"
            echo "Use '$0 help' for usage information"
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
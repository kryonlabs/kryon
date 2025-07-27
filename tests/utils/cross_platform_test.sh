#!/bin/bash
# Cross-Platform Test Validation for Kryon
# Tests behavior consistency across different platforms and environments

set -e

# Configuration
SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
RESULTS_DIR="$SCRIPT_DIR/../cross_platform_results"
PLATFORMS_FILE="$SCRIPT_DIR/platform_config.json"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m'

# Create results directory
mkdir -p "$RESULTS_DIR"

# Platform detection
detect_platform() {
    local os_name=""
    local arch=""
    local display=""
    
    # Detect OS
    case "$(uname -s)" in
        Linux*)     os_name="Linux";;
        Darwin*)    os_name="macOS";;
        CYGWIN*)    os_name="Windows";;
        MINGW*)     os_name="Windows";;
        MSYS*)      os_name="Windows";;
        *)          os_name="Unknown";;
    esac
    
    # Detect architecture
    case "$(uname -m)" in
        x86_64)     arch="x64";;
        aarch64)    arch="arm64";;
        arm64)      arch="arm64";;
        armv7l)     arch="arm32";;
        i386)       arch="x86";;
        i686)       arch="x86";;
        *)          arch="unknown";;
    esac
    
    # Detect display environment
    if [ -n "$DISPLAY" ]; then
        display="X11"
    elif [ -n "$WAYLAND_DISPLAY" ]; then
        display="Wayland"
    elif [ "$os_name" = "macOS" ]; then
        display="Quartz"
    elif [ "$os_name" = "Windows" ]; then
        display="Win32"
    else
        display="None"
    fi
    
    echo "$os_name|$arch|$display"
}

# System information gathering
gather_system_info() {
    local info_file="$RESULTS_DIR/system_info.json"
    local platform_info=$(detect_platform)
    IFS='|' read -r os_name arch display <<< "$platform_info"
    
    # Get system details
    local total_mem=""
    local cpu_info=""
    local gpu_info=""
    
    case "$os_name" in
        Linux)
            total_mem=$(free -h | awk '/^Mem:/ {print $2}')
            cpu_info=$(lscpu | grep "Model name" | cut -d':' -f2 | xargs)
            gpu_info=$(lspci | grep -i vga | cut -d':' -f3 | xargs)
            ;;
        macOS)
            total_mem=$(sysctl -n hw.memsize | awk '{print int($1/1024/1024/1024) " GB"}')
            cpu_info=$(sysctl -n machdep.cpu.brand_string)
            gpu_info=$(system_profiler SPDisplaysDataType | grep "Chipset Model" | cut -d':' -f2 | xargs)
            ;;
        Windows)
            total_mem=$(wmic computersystem get TotalPhysicalMemory /value | grep = | cut -d'=' -f2)
            cpu_info=$(wmic cpu get name /value | grep = | cut -d'=' -f2)
            gpu_info=$(wmic path win32_VideoController get name /value | grep = | cut -d'=' -f2)
            ;;
    esac
    
    # Check for required tools
    local has_cargo=$(command -v cargo &> /dev/null && echo "true" || echo "false")
    local has_git=$(command -v git &> /dev/null && echo "true" || echo "false")
    local rust_version=""
    
    if [ "$has_cargo" = "true" ]; then
        rust_version=$(rustc --version 2>/dev/null || echo "unknown")
    fi
    
    # Generate JSON
    cat > "$info_file" << EOF
{
    "timestamp": "$(date -Iseconds)",
    "platform": {
        "os": "$os_name",
        "architecture": "$arch",
        "display_system": "$display"
    },
    "hardware": {
        "total_memory": "$total_mem",
        "cpu": "$cpu_info",
        "gpu": "$gpu_info"
    },
    "environment": {
        "has_cargo": $has_cargo,
        "has_git": $has_git,
        "rust_version": "$rust_version",
        "display_env": "${DISPLAY:-${WAYLAND_DISPLAY:-none}}",
        "user": "${USER:-unknown}",
        "shell": "${SHELL:-unknown}"
    }
}
EOF
    
    echo "$info_file"
}

# Test compilation across targets
test_compilation() {
    local results_file="$RESULTS_DIR/compilation_results.json"
    
    echo -e "${BLUE}Testing compilation across targets...${NC}"
    
    # Get available targets
    local targets=()
    if command -v rustup &> /dev/null; then
        # Get installed targets
        while IFS= read -r line; do
            if [[ $line == *"(installed)"* ]]; then
                local target=$(echo "$line" | awk '{print $1}')
                targets+=("$target")
            fi
        done <<< "$(rustup target list)"
    else
        # Default to current target
        targets=("$(rustc -vV | grep host | cut -d' ' -f2)")
    fi
    
    echo "Found ${#targets[@]} installed targets: ${targets[*]}"
    
    # Test compilation for each target
    local results="{"
    local first=true
    
    for target in "${targets[@]}"; do
        echo -e "${YELLOW}Testing compilation for target: $target${NC}"
        
        local start_time=$(date +%s.%N)
        local success="false"
        local error=""
        
        if cargo check --target "$target" --workspace &>/dev/null; then
            success="true"
            echo -e "${GREEN}✓ Compilation successful for $target${NC}"
        else
            error=$(cargo check --target "$target" --workspace 2>&1 | head -n 5 | tr '\n' ' ')
            echo -e "${RED}❌ Compilation failed for $target${NC}"
        fi
        
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc -l)
        
        if [ "$first" = true ]; then
            first=false
        else
            results="$results,"
        fi
        
        results="$results\"$target\":{\"success\":$success,\"duration\":$duration,\"error\":\"$error\"}"
    done
    
    results="$results}"
    echo "$results" > "$results_file"
    echo "Compilation results saved to: $results_file"
}

# Test renderer availability
test_renderer_availability() {
    local results_file="$RESULTS_DIR/renderer_availability.json"
    
    echo -e "${BLUE}Testing renderer availability...${NC}"
    
    local renderers=(
        "kryon-renderer-sdl2:SDL2"
        "kryon-renderer-wgpu:WGPU"  
        "kryon-renderer-ratatui:Ratatui"
        "kryon-debug-interactive:Debug"
    )
    
    local results="{"
    local first=true
    
    for renderer_info in "${renderers[@]}"; do
        IFS=':' read -r binary_name renderer_name <<< "$renderer_info"
        
        echo -e "${YELLOW}Testing $renderer_name renderer...${NC}"
        
        local available="false"
        local version=""
        local error=""
        
        # Check if binary exists
        if [ -f "./target/debug/$binary_name" ] || [ -f "./target/release/$binary_name" ] || command -v "$binary_name" &> /dev/null; then
            available="true"
            
            # Try to get version info
            local binary_path=""
            if [ -f "./target/debug/$binary_name" ]; then
                binary_path="./target/debug/$binary_name"
            elif [ -f "./target/release/$binary_name" ]; then
                binary_path="./target/release/$binary_name"
            else
                binary_path="$binary_name"
            fi
            
            version=$("$binary_path" --version 2>/dev/null || echo "unknown")
            echo -e "${GREEN}✓ $renderer_name available${NC}"
        else
            error="Binary not found"
            echo -e "${RED}❌ $renderer_name not available${NC}"
        fi
        
        if [ "$first" = true ]; then
            first=false
        else
            results="$results,"
        fi
        
        results="$results\"$renderer_name\":{\"available\":$available,\"version\":\"$version\",\"error\":\"$error\"}"
    done
    
    results="$results}"
    echo "$results" > "$results_file"
    echo "Renderer availability results saved to: $results_file"
}

# Test dependencies
test_dependencies() {
    local results_file="$RESULTS_DIR/dependencies.json"
    
    echo -e "${BLUE}Testing system dependencies...${NC}"
    
    local deps=(
        "cargo:Rust toolchain"
        "git:Version control"
        "pkg-config:Build configuration"
        "cmake:Build system"
        "make:Build tool"
        "gcc:C compiler"
        "clang:Alternative C compiler"
        "python3:Python runtime"
        "node:Node.js runtime"
    )
    
    local results="{"
    local first=true
    
    for dep_info in "${deps[@]}"; do
        IFS=':' read -r command description <<< "$dep_info"
        
        local available="false"
        local version=""
        
        if command -v "$command" &> /dev/null; then
            available="true"
            case "$command" in
                cargo)
                    version=$(cargo --version | cut -d' ' -f2)
                    ;;
                git)
                    version=$(git --version | cut -d' ' -f3)
                    ;;
                python3)
                    version=$(python3 --version | cut -d' ' -f2)
                    ;;
                node)
                    version=$(node --version | tr -d 'v')
                    ;;
                *)
                    version=$("$command" --version 2>/dev/null | head -n1 | awk '{print $NF}' || echo "unknown")
                    ;;
            esac
            echo -e "${GREEN}✓ $description ($command): $version${NC}"
        else
            echo -e "${YELLOW}⚠ $description ($command): not found${NC}"
        fi
        
        if [ "$first" = true ]; then
            first=false
        else
            results="$results,"
        fi
        
        results="$results\"$command\":{\"available\":$available,\"version\":\"$version\",\"description\":\"$description\"}"
    done
    
    results="$results}"
    echo "$results" > "$results_file"
    echo "Dependencies results saved to: $results_file"
}

# Test platform-specific features
test_platform_features() {
    local results_file="$RESULTS_DIR/platform_features.json"
    local platform_info=$(detect_platform)
    IFS='|' read -r os_name arch display <<< "$platform_info"
    
    echo -e "${BLUE}Testing platform-specific features...${NC}"
    
    local results="{"
    
    # Test display system
    local display_available="false"
    if [ "$display" != "None" ]; then
        case "$display" in
            X11)
                if xset q &>/dev/null; then
                    display_available="true"
                fi
                ;;
            Wayland)
                if [ -n "$WAYLAND_DISPLAY" ]; then
                    display_available="true"
                fi
                ;;
            Quartz|Win32)
                display_available="true"  # Assume available on macOS/Windows
                ;;
        esac
    fi
    
    # Test OpenGL availability (for WGPU)
    local opengl_available="false"
    case "$os_name" in
        Linux)
            if command -v glxinfo &> /dev/null && glxinfo | grep -q "OpenGL"; then
                opengl_available="true"
            fi
            ;;
        macOS)
            # OpenGL is available on macOS by default
            opengl_available="true"
            ;;
        Windows)
            # Assume OpenGL is available on Windows
            opengl_available="true"
            ;;
    esac
    
    # Test SDL2 availability
    local sdl2_available="false"
    if pkg-config --exists sdl2 2>/dev/null; then
        sdl2_available="true"
    fi
    
    # Test terminal capabilities
    local terminal_colors="false"
    local terminal_unicode="false"
    
    if [ -n "$TERM" ]; then
        if tput colors &>/dev/null && [ "$(tput colors)" -ge 256 ]; then
            terminal_colors="true"
        fi
        
        if locale | grep -q "UTF-8"; then
            terminal_unicode="true"
        fi
    fi
    
    results="$results\"display_system\":{\"type\":\"$display\",\"available\":$display_available}"
    results="$results,\"graphics\":{\"opengl_available\":$opengl_available,\"sdl2_available\":$sdl2_available}"
    results="$results,\"terminal\":{\"colors_available\":$terminal_colors,\"unicode_available\":$terminal_unicode}"
    results="$results}"
    
    echo "$results" > "$results_file"
    echo "Platform features results saved to: $results_file"
    
    # Print summary
    echo -e "${BLUE}Platform Feature Summary:${NC}"
    echo -e "  Display: $display (Available: $display_available)"
    echo -e "  OpenGL: $opengl_available"
    echo -e "  SDL2: $sdl2_available"
    echo -e "  Terminal Colors: $terminal_colors"
    echo -e "  Terminal Unicode: $terminal_unicode"
}

# Run basic tests on current platform
run_basic_tests() {
    local results_file="$RESULTS_DIR/basic_tests.json"
    
    echo -e "${BLUE}Running basic tests on current platform...${NC}"
    
    # Try to run a simple test
    local test_file="$PROJECT_ROOT/tests/test_suites/01_core_components.kry"
    local krb_file="$PROJECT_ROOT/tests/test_suites/01_core_components.krb"
    
    local compile_success="false"
    local compile_time=""
    local compile_error=""
    
    if [ -f "$test_file" ]; then
        local start_time=$(date +%s.%N)
        
        if cargo run --bin kryon-compiler -- compile "$test_file" "$krb_file" &>/dev/null; then
            compile_success="true"
            echo -e "${GREEN}✓ Test compilation successful${NC}"
        else
            compile_error=$(cargo run --bin kryon-compiler -- compile "$test_file" "$krb_file" 2>&1 | head -n 3)
            echo -e "${RED}❌ Test compilation failed${NC}"
        fi
        
        local end_time=$(date +%s.%N)
        compile_time=$(echo "$end_time - $start_time" | bc -l)
    else
        compile_error="Test file not found"
        echo -e "${RED}❌ Test file not found: $test_file${NC}"
    fi
    
    # Test renderer execution (if available)
    local renderer_tests="{"
    local first=true
    
    if [ "$compile_success" = "true" ] && [ -f "$krb_file" ]; then
        local renderers=("kryon-debug-interactive" "kryon-renderer-ratatui")
        
        for renderer in "${renderers[@]}"; do
            local success="false"
            local error=""
            
            if [ -f "./target/debug/$renderer" ] || [ -f "./target/release/$renderer" ]; then
                # Try to run for 2 seconds
                if timeout 2s cargo run --bin "$renderer" -- "$krb_file" &>/dev/null; then
                    success="true"
                    echo -e "${GREEN}✓ $renderer execution successful${NC}"
                else
                    error="Execution timeout or error"
                    echo -e "${YELLOW}⚠ $renderer execution timeout/error${NC}"
                fi
            else
                error="Renderer not available"
                echo -e "${YELLOW}⚠ $renderer not available${NC}"
            fi
            
            if [ "$first" = true ]; then
                first=false
            else
                renderer_tests="$renderer_tests,"
            fi
            
            renderer_tests="$renderer_tests\"$renderer\":{\"success\":$success,\"error\":\"$error\"}"
        done
    fi
    
    renderer_tests="$renderer_tests}"
    
    # Generate results
    local results="{"
    results="$results\"compilation\":{\"success\":$compile_success,\"time\":\"$compile_time\",\"error\":\"$compile_error\"}"
    results="$results,\"renderers\":$renderer_tests"
    results="$results}"
    
    echo "$results" > "$results_file"
    echo "Basic tests results saved to: $results_file"
}

# Generate comprehensive report
generate_report() {
    local report_file="$RESULTS_DIR/cross_platform_report.html"
    local timestamp=$(date)
    local platform_info=$(detect_platform)
    IFS='|' read -r os_name arch display <<< "$platform_info"
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Kryon Cross-Platform Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }
        .header { background-color: #2c3e50; color: white; padding: 20px; border-radius: 8px; }
        .section { background-color: white; margin: 20px 0; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .success { color: #27ae60; }
        .warning { color: #f39c12; }
        .error { color: #e74c3c; }
        .info { color: #3498db; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .json-display { background-color: #f8f9fa; padding: 10px; border-radius: 4px; font-family: monospace; font-size: 12px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🔍 Kryon Cross-Platform Test Report</h1>
        <p>Generated: $timestamp</p>
        <p>Platform: $os_name ($arch) - $display</p>
    </div>
    
    <div class="section">
        <h2>📊 System Information</h2>
EOF

    # Include system info JSON if available
    if [ -f "$RESULTS_DIR/system_info.json" ]; then
        echo "<div class='json-display'>" >> "$report_file"
        cat "$RESULTS_DIR/system_info.json" | python3 -m json.tool >> "$report_file" 2>/dev/null || cat "$RESULTS_DIR/system_info.json" >> "$report_file"
        echo "</div>" >> "$report_file"
    fi
    
    # Add other sections similarly
    for section in "compilation_results" "renderer_availability" "dependencies" "platform_features" "basic_tests"; do
        local file="$RESULTS_DIR/${section}.json"
        if [ -f "$file" ]; then
            echo "<div class='section'>" >> "$report_file"
            echo "<h2>📋 ${section//_/ }</h2>" >> "$report_file"
            echo "<div class='json-display'>" >> "$report_file"
            cat "$file" | python3 -m json.tool >> "$report_file" 2>/dev/null || cat "$file" >> "$report_file"
            echo "</div></div>" >> "$report_file"
        fi
    done
    
    cat >> "$report_file" << EOF
    
    <div class="section">
        <h2>📝 Summary</h2>
        <p>This report provides comprehensive information about Kryon's compatibility and performance on the current platform.</p>
        <p>All test results and system information have been saved to: <code>$RESULTS_DIR</code></p>
    </div>
    
    <footer style="text-align: center; margin-top: 40px; color: #7f8c8d;">
        <p>Generated by Kryon Cross-Platform Test Suite</p>
    </footer>
</body>
</html>
EOF
    
    echo -e "${GREEN}✓ HTML report generated: $report_file${NC}"
}

# Main function
main() {
    local mode="${1:-full}"
    
    echo -e "${PURPLE}Kryon Cross-Platform Test Suite${NC}"
    echo -e "${PURPLE}================================${NC}"
    echo
    
    case "$mode" in
        full)
            echo -e "${BLUE}Running comprehensive cross-platform tests...${NC}"
            gather_system_info
            test_compilation
            test_renderer_availability
            test_dependencies
            test_platform_features
            run_basic_tests
            generate_report
            ;;
        system)
            gather_system_info
            ;;
        compile)
            test_compilation
            ;;
        renderers)
            test_renderer_availability
            ;;
        deps)
            test_dependencies
            ;;
        features)
            test_platform_features
            ;;
        basic)
            run_basic_tests
            ;;
        report)
            generate_report
            ;;
        *)
            echo "Usage: $0 [full|system|compile|renderers|deps|features|basic|report]"
            echo
            echo "Commands:"
            echo "  full      - Run all tests (default)"
            echo "  system    - Gather system information only"
            echo "  compile   - Test compilation across targets"
            echo "  renderers - Test renderer availability"
            echo "  deps      - Test system dependencies"
            echo "  features  - Test platform-specific features"
            echo "  basic     - Run basic functionality tests"
            echo "  report    - Generate HTML report from existing results"
            exit 1
            ;;
    esac
    
    echo
    echo -e "${GREEN}✅ Cross-platform testing completed${NC}"
    echo -e "${BLUE}Results saved to: $RESULTS_DIR${NC}"
}

# Change to project root
cd "$PROJECT_ROOT"

# Run main function
main "$@"
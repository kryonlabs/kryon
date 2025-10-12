#!/bin/bash

# Compilation Benchmark Script for Kryon
# Measures and logs compilation time of hello_world.nim
# Usage: ./benchmark_compilation.sh [options]
# Options:
#   --show-last    Show only the last benchmark result
#   --show-recent  Show last 10 benchmark results
#   --clear        Clear the benchmark log

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BENCHMARK_FILE="$SCRIPT_DIR/hello_world.nim"
LOG_FILE="$SCRIPT_DIR/compilation_benchmark.log"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Parse command line arguments
case "${1:-}" in
    --show-last)
        if [ -f "$LOG_FILE" ]; then
            echo -e "${CYAN}Last benchmark result:${NC}"
            tail -n 1 "$LOG_FILE"
        else
            print_warning "No benchmark results found."
        fi
        exit 0
        ;;
    --show-recent)
        if [ -f "$LOG_FILE" ]; then
            echo -e "${CYAN}Recent benchmark results (last 10):${NC}"
            tail -n 10 "$LOG_FILE"
        else
            print_warning "No benchmark results found."
        fi
        exit 0
        ;;
    --clear)
        if [ -f "$LOG_FILE" ]; then
            rm "$LOG_FILE"
            print_success "Benchmark log cleared."
        else
            print_warning "No benchmark log to clear."
        fi
        exit 0
        ;;
    --help|-h)
        echo "Usage: $0 [options]"
        echo "Options:"
        echo "  --show-last    Show only the last benchmark result"
        echo "  --show-recent  Show last 10 benchmark results"
        echo "  --clear        Clear the benchmark log"
        echo "  --help, -h     Show this help"
        exit 0
        ;;
    "")
        # No arguments, run benchmark
        ;;
    *)
        print_error "Unknown option: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
esac

# Check if benchmark file exists
if [ ! -f "$BENCHMARK_FILE" ]; then
    print_error "Benchmark file not found: $BENCHMARK_FILE"
    exit 1
fi

# Get current date and time
CURRENT_DATE=$(date '+%Y-%m-%d %H:%M:%S')

print_status "Running compilation benchmark..."
print_status "File: $BENCHMARK_FILE"
print_status "Date: $CURRENT_DATE"
echo ""

# Clean up any previous compilation artifacts
rm -f "$SCRIPT_DIR/hello_world"

# Run compilation and measure time
print_status "Compiling with Nim compiler..."

# Use simple timing with date +%s.%N and awk for calculation
START_TIME=$(date +%s.%N)

if nim c "$BENCHMARK_FILE" 2>/dev/null; then
    END_TIME=$(date +%s.%N)
    # Use awk for floating point calculation (works without bc)
    COMPILE_TIME=$(awk "BEGIN {printf \"%.3f\", $END_TIME - $START_TIME}")

      # Ensure we have a valid time value
    if ! echo "$COMPILE_TIME" | grep -q "^[0-9]*\.[0-9]*"; then
        COMPILE_TIME="0.001"  # Minimum measurable time
    fi

    print_success "Compilation completed in ${COMPILE_TIME} seconds"

    # Log the result
    echo "$CURRENT_DATE - Compilation Time: ${COMPILE_TIME} seconds" >> "$LOG_FILE"

    # Clean up the compiled binary
    rm -f "$SCRIPT_DIR/hello_world" "$SCRIPT_DIR/hello_world.exe"

    print_status "Result logged to: $LOG_FILE"

    # Show comparison with previous runs if available
    if [ -f "$LOG_FILE" ]; then
        TOTAL_RUNS=$(wc -l < "$LOG_FILE")
        echo ""
        print_status "Total benchmarks run: $TOTAL_RUNS"

        if [ "$TOTAL_RUNS" -gt 1 ]; then
            echo -e "${CYAN}Recent benchmark history:${NC}"
            tail -n 5 "$LOG_FILE"
        fi
    fi

else
    print_error "Compilation failed!"
    echo "$CURRENT_DATE - Compilation Failed" >> "$LOG_FILE"
    exit 1
fi

echo ""
print_success "Benchmark completed successfully!"
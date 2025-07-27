#!/bin/bash
set -e

# Kryon Modular Renderer Build Script
# Build individual renderer binaries as needed

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Available renderers and their features
declare -A RENDERERS
RENDERERS[wgpu]="wgpu"
RENDERERS[raylib]="raylib"
RENDERERS[sdl2]="sdl2"
RENDERERS[ratatui]="ratatui"
RENDERERS[html]="html-server"
RENDERERS[debug]=""  # Debug renderer has no special features

# Usage
usage() {
    echo "Usage: $0 [OPTIONS] [RENDERER...]"
    echo ""
    echo "Build Kryon renderer binaries selectively for faster builds."
    echo ""
    echo "Available renderers:"
    for renderer in "${!RENDERERS[@]}"; do
        echo "  $renderer"
    done
    echo ""
    echo "Options:"
    echo "  --all           Build all renderers"
    echo "  --main          Build main kryon CLI only"
    echo "  --release       Build in release mode"
    echo "  --clean         Clean build artifacts first"
    echo "  --help, -h      Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 wgpu raylib                 # Build WGPU and Raylib renderers"
    echo "  $0 --all                      # Build all renderers"
    echo "  $0 --main                     # Build main CLI only"
    echo "  $0 --release wgpu             # Build WGPU renderer in release mode"
    echo "  $0 --clean --all              # Clean and build all"
}

# Parse command line arguments
BUILD_MODE="debug"
CLEAN=false
BUILD_MAIN=false
BUILD_ALL=false
SELECTED_RENDERERS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            BUILD_ALL=true
            shift
            ;;
        --main)
            BUILD_MAIN=true
            shift
            ;;
        --release)
            BUILD_MODE="release"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        --*)
            print_error "Unknown option: $1"
            usage
            exit 1
            ;;
        *)
            # Check if it's a valid renderer
            if [[ -n "${RENDERERS[$1]}" ]]; then
                SELECTED_RENDERERS+=("$1")
            else
                print_error "Unknown renderer: $1"
                echo "Available renderers: ${!RENDERERS[*]}"
                exit 1
            fi
            shift
            ;;
    esac
done

# If no specific renderers selected and not --all or --main, show usage
if [[ ${#SELECTED_RENDERERS[@]} -eq 0 && "$BUILD_ALL" = false && "$BUILD_MAIN" = false ]]; then
    print_warning "No renderers specified."
    usage
    exit 1
fi

# Clean if requested
if [[ "$CLEAN" = true ]]; then
    print_info "Cleaning build artifacts..."
    cd "$PROJECT_ROOT"
    cargo clean
    print_success "Clean complete"
fi

# Build main CLI if requested
if [[ "$BUILD_MAIN" = true ]]; then
    print_info "Building main kryon CLI..."
    cd "$PROJECT_ROOT"
    
    if [[ "$BUILD_MODE" = "release" ]]; then
        cargo build --bin kryon --release
    else
        cargo build --bin kryon
    fi
    
    print_success "Main kryon CLI built successfully"
fi

# Determine which renderers to build
RENDERERS_TO_BUILD=()
if [[ "$BUILD_ALL" = true ]]; then
    RENDERERS_TO_BUILD=("${!RENDERERS[@]}")
else
    RENDERERS_TO_BUILD=("${SELECTED_RENDERERS[@]}")
fi

# Build selected renderers
for renderer in "${RENDERERS_TO_BUILD[@]}"; do
    print_info "Building $renderer renderer..."
    
    cd "$PROJECT_ROOT"
    
    # Build command
    CMD="cargo build --bin kryon-renderer-$renderer"
    
    # Add features if needed
    feature="${RENDERERS[$renderer]}"
    if [[ -n "$feature" ]]; then
        CMD="$CMD --features $feature"
    fi
    
    # Add release flag if needed
    if [[ "$BUILD_MODE" = "release" ]]; then
        CMD="$CMD --release"
    fi
    
    print_info "Running: $CMD"
    
    if $CMD; then
        print_success "$renderer renderer built successfully"
    else
        print_error "Failed to build $renderer renderer"
        exit 1
    fi
done

print_success "Build completed!"

# Show what was built
echo ""
print_info "Built binaries:"
TARGET_DIR="$PROJECT_ROOT/target/$BUILD_MODE"

if [[ "$BUILD_MAIN" = true ]]; then
    if [[ -f "$TARGET_DIR/kryon" ]]; then
        echo "  ✓ kryon (main CLI)"
    fi
fi

for renderer in "${RENDERERS_TO_BUILD[@]}"; do
    binary_name="kryon-renderer-$renderer"
    if [[ -f "$TARGET_DIR/$binary_name" ]]; then
        echo "  ✓ $binary_name"
    fi
done

echo ""
print_info "To use these renderers, make sure they're in your PATH or in the same directory as the main kryon binary."
print_info "Test with: kryon --list-renderers"
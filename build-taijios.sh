#!/bin/bash
# Build script for TaijiOS native kryon
#
# This script builds kryon as a native TaijiOS/Inferno binary that:
# - Links against libinterp.a (Inferno interpreter runtime)
# - Uses full kernel syscalls (kopen, kread, kmount, kbind, etc.)
# - Runs under TaijiOS emu (user-space OS emulator)
# - Has access to all TaijiOS devices and namespaces
#
# Usage:
#   ./build-taijios.sh        - Build TaijiOS native version
#   ./build-taijios.sh clean  - Clean build artifacts

set -e

# =============================================================================
# CONFIGURATION
# =============================================================================

TAIJIOS="${TAIJIOS:-/home/wao/Projects/TaijiOS}"
KRYON_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# =============================================================================
# HELPER FUNCTIONS
# =============================================================================

info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# =============================================================================
# VALIDATION
# =============================================================================

validate_environment() {
    info "Validating TaijiOS environment..."

    # Check TaijiOS directory exists
    if [ ! -d "$TAIJIOS" ]; then
        error "TaijiOS not found at $TAIJIOS"
        echo ""
        echo "Please set TAIJIOS environment variable or edit this script:"
        echo "  export TAIJIOS=/path/to/TaijiOS"
        echo "  ./build-taijios.sh"
        exit 1
    fi

    success "TaijiOS directory found: $TAIJIOS"

    # Check libinterp.a exists
    local LIBINTERP="$TAIJIOS/Linux/amd64/lib/libinterp.a"
    if [ ! -f "$LIBINTERP" ]; then
        error "libinterp.a not found at $LIBINTERP"
        echo ""
        echo "Please build TaijiOS first:"
        echo "  cd $TAIJIOS"
        echo "  mk"
        exit 1
    fi

    success "libinterp.a found"

    # Check lib9.a exists
    local LIB9="$TAIJIOS/Linux/amd64/lib/lib9.a"
    if [ ! -f "$LIB9" ]; then
        error "lib9.a not found at $LIB9"
        echo ""
        echo "Please build TaijiOS first:"
        echo "  cd $TAIJIOS"
        echo "  mk"
        exit 1
    fi

    success "lib9.a found"

    # Check emu binary exists
    local EMU="$TAIJIOS/emu/Linux/emu"
    if [ ! -f "$EMU" ]; then
        warning "emu binary not found at $EMU"
        warning "You may need to build TaijiOS emu: cd $TAIJIOS && mk"
    else
        success "emu binary found"
    fi

    info "Environment validation complete"
    echo ""
}

# =============================================================================
# BUILD
# =============================================================================

build_kryon() {
    info "Building kryon for TaijiOS/Inferno..."
    echo ""
    echo "  TAIJIOS: $TAIJIOS"
    echo "  KRYON:   $KRYON_ROOT"
    echo ""

    cd "$KRYON_ROOT"

    # Build using mk (Plan 9 make)
    local MK="$TAIJIOS/Linux/amd64/bin/mk"
    if [ ! -f "$MK" ]; then
        error "mk not found at $MK"
        error "Please build TaijiOS first: cd $TAIJIOS && mk"
        exit 1
    fi

    if "$MK" "$@"; then
        echo ""
        success "Build successful!"
        echo ""
        show_usage_instructions
    else
        echo ""
        error "Build failed!"
        exit 1
    fi
}

# =============================================================================
# USAGE INSTRUCTIONS
# =============================================================================

show_usage_instructions() {
    local KRYON_BIN="$TAIJIOS/Linux/amd64/bin/kryon"

    echo "=============================================="
    echo "  TaijiOS Native Kryon - Usage Instructions"
    echo "=============================================="
    echo ""
    echo "Binary location:"
    echo "  $KRYON_BIN"
    echo ""
    echo "Test kryon:"
    echo "  $KRYON_BIN --version"
    echo "  $KRYON_BIN --help"
    echo ""
    echo "Compile a Kryon file:"
    echo "  $KRYON_BIN compile examples/hello-world.kry -o /tmp/hello.krb"
    echo ""
    echo "Run a compiled application:"
    echo "  $KRYON_BIN run /tmp/hello.krb"
    echo ""
    echo "  4. Access TaijiOS devices:"
    echo "     # Kryon can now use device files like:"
    echo "     #   #c/cons    - console device"
    echo "     #   #p/PID/ctl - process control"
    echo "     #   /prog/*    - process filesystem"
    echo ""
    echo "Features available in TaijiOS native build:"
    echo "  ✓ Extended file I/O (device files)"
    echo "  ✓ Namespace operations (bind, mount, unmount)"
    echo "  ✓ Process control via /prog device"
    echo "  ✓ 9P filesystem access"
    echo "  ✓ Full kernel syscalls (kopen, kread, kmount, etc.)"
    echo ""
}

# =============================================================================
# CLEAN
# =============================================================================

clean_build() {
    info "Cleaning build artifacts..."
    cd "$KRYON_ROOT"
    local MK="$TAIJIOS/Linux/amd64/bin/mk"
    if [ -f "$MK" ]; then
        "$MK" clean
    else
        error "mk not found at $MK"
        exit 1
    fi
    success "Clean complete"
}

# =============================================================================
# MAIN
# =============================================================================

main() {
    echo "=============================================="
    echo "  TaijiOS Native Kryon Build Script"
    echo "=============================================="
    echo ""

    # Parse command
    local COMMAND="${1:-build}"

    case "$COMMAND" in
        clean)
            clean_build
            ;;
        help|--help|-h)
            echo "Usage: $0 [command]"
            echo ""
            echo "Commands:"
            echo "  build (default)  - Build TaijiOS native kryon"
            echo "  clean            - Clean build artifacts"
            echo "  help             - Show this help message"
            echo ""
            echo "Environment variables:"
            echo "  TAIJIOS - Path to TaijiOS installation (default: $TAIJIOS)"
            echo ""
            echo "Example:"
            echo "  ./build-taijios.sh"
            echo "  TAIJIOS=/path/to/TaijiOS ./build-taijios.sh"
            echo ""
            ;;
        build|*)
            validate_environment
            build_kryon "${@:2}"
            ;;
    esac
}

main "$@"

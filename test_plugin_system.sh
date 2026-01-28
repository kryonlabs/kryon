#!/bin/bash
# Test script for kryon platform services plugin system

set -e  # Exit on error

KRYON_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$KRYON_DIR"

echo "================================"
echo "Kryon Plugin System Test Script"
echo "================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

success() {
    echo -e "${GREEN}✓${NC} $1"
}

error() {
    echo -e "${RED}✗${NC} $1"
}

info() {
    echo -e "${YELLOW}ℹ${NC} $1"
}

# Test 1: Standard build (no plugins)
echo "Test 1: Standard build (no plugins)"
echo "-----------------------------------"
info "Cleaning build directory..."
make clean > /dev/null 2>&1

info "Building without plugins..."
if make > /dev/null 2>&1; then
    success "Standard build successful"
else
    error "Standard build failed"
    exit 1
fi

info "Checking binary exists..."
if [ -f "build/bin/kryon" ]; then
    success "Binary created"
else
    error "Binary not found"
    exit 1
fi

echo ""

# Test 2: Check for Inferno environment
echo "Test 2: Inferno environment detection"
echo "-------------------------------------"
if [ -n "$INFERNO" ]; then
    success "INFERNO environment variable set: $INFERNO"
    INFERNO_AVAILABLE=1
elif [ -d "/opt/inferno" ]; then
    info "Found Inferno at /opt/inferno"
    export INFERNO=/opt/inferno
    INFERNO_AVAILABLE=1
elif [ -d "$HOME/inferno" ]; then
    info "Found Inferno at $HOME/inferno"
    export INFERNO=$HOME/inferno
    INFERNO_AVAILABLE=1
else
    error "Inferno not found (set INFERNO environment variable)"
    INFERNO_AVAILABLE=0
fi

echo ""

# Test 3: Inferno build (if available)
if [ "$INFERNO_AVAILABLE" -eq 1 ]; then
    echo "Test 3: Inferno plugin build"
    echo "----------------------------"
    info "Cleaning build directory..."
    make clean > /dev/null 2>&1

    info "Building with Inferno plugin..."
    if make -f Makefile.inferno > build.log 2>&1; then
        success "Inferno plugin build successful"
    else
        error "Inferno plugin build failed"
        echo "Check build.log for details"
        exit 1
    fi

    info "Checking binary exists..."
    if [ -f "build/bin/kryon" ]; then
        success "Binary created"
    else
        error "Binary not found"
        exit 1
    fi

    info "Checking for plugin symbols..."
    if nm build/bin/kryon | grep -q "inferno_plugin"; then
        success "Plugin symbols found in binary"
    else
        error "Plugin symbols not found"
        exit 1
    fi

    echo ""
else
    echo "Test 3: Inferno plugin build"
    echo "----------------------------"
    info "Skipping (Inferno not available)"
    echo ""
fi

# Test 4: Source structure verification
echo "Test 4: Source structure verification"
echo "-------------------------------------"

check_file() {
    if [ -f "$1" ]; then
        success "$1 exists"
    else
        error "$1 missing"
        return 1
    fi
}

check_file "include/platform_services.h"
check_file "include/services/extended_file_io.h"
check_file "include/services/namespace.h"
check_file "include/services/process_control.h"
check_file "include/services/ipc.h"
check_file "src/services/services_registry.c"
check_file "src/services/services_null.c"
check_file "src/plugins/inferno/plugin.c"
check_file "src/plugins/inferno/extended_file_io.c"
check_file "src/plugins/inferno/namespace.c"
check_file "src/plugins/inferno/process_control.c"
check_file "src/plugins/inferno/Makefile"
check_file "Makefile.inferno"
check_file "docs/plugin_development.md"
check_file "src/plugins/README.md"
check_file "examples/inferno_features.c"

echo ""

# Summary
echo "================================"
echo "Test Summary"
echo "================================"
success "Standard build works"
if [ "$INFERNO_AVAILABLE" -eq 1 ]; then
    success "Inferno plugin build works"
    success "Plugin symbols present"
else
    info "Inferno tests skipped (not available)"
fi
success "All required files present"

echo ""
echo "Plugin system implementation complete!"
echo ""
echo "Next steps:"
echo "  1. Test standard build: make && ./build/bin/kryon --version"
if [ "$INFERNO_AVAILABLE" -eq 1 ]; then
    echo "  2. Test Inferno build: make -f Makefile.inferno"
    echo "  3. Compile and run demo: gcc -o demo examples/inferno_features.c -Iinclude -Lbuild/lib -lkryon"
else
    echo "  2. Install Inferno to test Inferno plugin"
    echo "  3. Set INFERNO environment variable"
fi
echo ""

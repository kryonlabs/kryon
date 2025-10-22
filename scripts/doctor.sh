#!/usr/bin/env bash

# Kryon System Doctor Script
# Comprehensive system health check and dependency verification

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
PREFIX="${HOME}/.local"
BINDIR="${PREFIX}/bin"
LIBDIR="${PREFIX}/lib"
INCDIR="${PREFIX}/include/kryon"
PKGCONFIGDIR="${PREFIX}/lib/pkgconfig"
CONFIGDIR="${HOME}/.config/kryon"

# Helper functions
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_header() {
    echo -e "${CYAN}$1${NC}"
}

# Check basic tools
check_basic_tools() {
    print_header "Basic Tools"
    echo "=================="

    # Check for make
    if command -v make &> /dev/null; then
        print_success "Make: $(make --version 2>/dev/null | head -n1 || echo 'Available')"
    else
        print_error "Make: not found (install build-essential on Ubuntu/Debian)"
    fi

    # Check for git
    if command -v git &> /dev/null; then
        print_success "Git: $(git --version)"
    else
        print_warning "Git: not found (optional, for cloning repositories)"
    fi

    # Check for curl/wget
    if command -v curl &> /dev/null; then
        print_success "curl: Available"
    elif command -v wget &> /dev/null; then
        print_success "wget: Available"
    else
        print_warning "Neither curl nor wget found (optional, for downloads)"
    fi

    echo ""
}

# Check Nim installation
check_nim() {
    print_header "Nim Installation"
    echo "==================="

    if command -v nim &> /dev/null; then
        local nim_version=$(nim --version 2>/dev/null | head -n1 || echo "Unknown")
        print_success "Nim: $nim_version"

        # Check for nimble
        if command -v nimble &> /dev/null; then
            print_success "Nimble: Available"
        else
            print_error "Nimble: not found (should come with Nim)"
        fi

        # Check Nim configuration
        local nim_cache="${HOME}/.nimcache"
        if [ -d "$nim_cache" ]; then
            print_success "Nim cache: $nim_cache"
        else
            print_warning "Nim cache not found (will be created on first compile)"
        fi

    else
        print_error "Nim: not found"
        echo "  Install from: https://nim-lang.org/install.html"
        echo "  Or use package manager:"
        echo "    Ubuntu/Debian: sudo apt install nim"
        echo "    macOS: brew install nim"
        echo "    Nix: nix-shell -p nim"
        return 1
    fi

    echo ""
}

# Check backend libraries
check_backends() {
    print_header "Rendering Backends"
    echo "==================="

    local found_backends=0

    # Check Raylib
    if nimble list raylib 2>/dev/null | grep -q raylib; then
        print_success "Raylib: Available via nimble"
        ((found_backends++))
    elif pkg-config --exists raylib 2>/dev/null; then
        print_success "Raylib: Available via system package"
        ((found_backends++))
    else
        print_warning "Raylib: Not found"
        echo "  Install with: nimble install raylib"
    fi

    # Check SDL2
    if pkg-config --exists sdl2 2>/dev/null; then
        local sdl_version=$(pkg-config --modversion sdl2 2>/dev/null || echo "Unknown")
        print_success "SDL2: Available ($sdl_version)"
        ((found_backends++))
    else
        print_warning "SDL2: Not found"
        echo "  Install with:"
        echo "    Ubuntu/Debian: sudo apt install libsdl2-dev"
        echo "    macOS: brew install sdl2"
        echo "    Nix: nix-shell -p SDL2"
    fi

    # Check SDL2_ttf
    if pkg-config --exists sdl2_ttf 2>/dev/null; then
        local ttf_version=$(pkg-config --modversion sdl2_ttf 2>/dev/null || echo "Unknown")
        print_success "SDL2_ttf: Available ($ttf_version)"
    else
        print_warning "SDL2_ttf: Not found (required for text rendering with SDL2)"
        echo "  Install with:"
        echo "    Ubuntu/Debian: sudo apt install libsdl2-ttf-dev"
        echo "    macOS: brew install sdl2_ttf"
    fi

    # Check Skia
    if [ -f "/usr/include/skia" ] || [ -f "/usr/local/include/skia" ] || [ -d "/nix/store" ] && find /nix/store -name "skia" -type d 2>/dev/null | grep -q .; then
        print_success "Skia: Available (for SDL2_skia backend)"
    else
        print_warning "Skia: Not found (optional, advanced backend)"
    fi

    echo ""

    if [ $found_backends -eq 0 ]; then
        print_error "No rendering backends available!"
        echo "Install at least one backend to use Kryon."
        return 1
    elif [ $found_backends -eq 1 ]; then
        print_status "Found $found_backends rendering backend"
    else
        print_status "Found $found_backends rendering backends"
    fi

    echo ""
}

# Check Kryon installation
check_kryon_installation() {
    print_header "Kryon Installation"
    echo "======================"

    local installation_status="Not installed"
    local components_found=0

    # Check CLI tool
    if command -v "${BINDIR}/kryon" &> /dev/null; then
        print_success "CLI tool: ${BINDIR}/kryon"
        ((components_found++))

        # Get version
        local version=$("${BINDIR}/kryon" version 2>/dev/null | head -n1 | grep -o 'v[0-9.]*' || echo "Unknown")
        echo "  Version: $version"
        installation_status="Partially installed"
    else
        print_warning "CLI tool: Not found at ${BINDIR}/kryon"
    fi

    # Check library
    if [ -f "${LIBDIR}/libkryon.a" ]; then
        print_success "Library: ${LIBDIR}/libkryon.a"
        ((components_found++))
        installation_status="Partially installed"
    else
        print_warning "Library: Not found at ${LIBDIR}/libkryon.a"
    fi

    # Check headers
    if [ -d "${INCDIR}" ]; then
        local header_count=$(find "$INCDIR" -name "*.nim" 2>/dev/null | wc -l)
        print_success "Headers: ${INCDIR} ($header_count files)"
        ((components_found++))
        installation_status="Partially installed"
    else
        print_warning "Headers: Not found at ${INCDIR}"
    fi

    # Check pkg-config
    if [ -f "${PKGCONFIGDIR}/kryon.pc" ]; then
        print_success "pkg-config: ${PKGCONFIGDIR}/kryon.pc"
        ((components_found++))
        installation_status="Partially installed"
    else
        print_warning "pkg-config: Not found at ${PKGCONFIGDIR}/kryon.pc"
    fi

    # Check configuration
    if [ -f "${CONFIGDIR}/config.yaml" ]; then
        print_success "Config: ${CONFIGDIR}/config.yaml"
        ((components_found++))

        # Show config content
        echo "  Contents:"
        grep -v "installed_at" "${CONFIGDIR}/config.yaml" 2>/dev/null | sed 's/^/    /' || true
    else
        print_warning "Config: Not found at ${CONFIGDIR}/config.yaml"
    fi

    echo ""

    if [ $components_found -eq 5 ]; then
        installation_status="Fully installed"
        print_success "Installation status: $installation_status ($components_found/5 components)"
    elif [ $components_found -gt 0 ]; then
        print_warning "Installation status: $installation_status ($components_found/5 components)"
    else
        print_error "Installation status: $installation_status"
    fi

    echo ""
}

# Check environment
check_environment() {
    print_header "Environment"
    echo "============"

    # Check PATH
    if [[ ":$PATH:" == *":${BINDIR}:"* ]]; then
        print_success "PATH includes ${BINDIR}"
    else
        print_warning "PATH does not include ${BINDIR}"
        echo "  Add to your shell profile:"
        echo "    export PATH=\"${BINDIR}:\$PATH\""
    fi

    # Check PKG_CONFIG_PATH
    if [ -n "${PKG_CONFIG_PATH}" ]; then
        if [[ ":${PKG_CONFIG_PATH}:" == *":${PKGCONFIGDIR}:"* ]]; then
            print_success "PKG_CONFIG_PATH includes ${PKGCONFIGDIR}"
        else
            print_warning "PKG_CONFIG_PATH does not include ${PKGCONFIGDIR}"
        fi
    else
        print_status "PKG_CONFIG_PATH not set (usually not needed)"
    fi

    # Check for environment variables
    local kryon_vars=("KRYON_ROOT" "KRYON_HOME" "KRYON_PATH" "KRYON_SRC")
    local found_vars=0

    for var in "${kryon_vars[@]}"; do
        if [ -n "${!var}" ]; then
            print_success "$var: ${!var}"
            ((found_vars++))
        fi
    done

    if [ $found_vars -eq 0 ]; then
        print_status "No Kryon environment variables set (normal for installed version)"
    fi

    echo ""
}

# Check system information
check_system() {
    print_header "System Information"
    echo "==================="

    # OS and architecture
    if command -v uname &> /dev/null; then
        print_success "OS: $(uname -s)"
        print_success "Architecture: $(uname -m)"
    fi

    # Check if running in NixOS
    if [ -f /etc/nixos ]; then
        print_success "NixOS: Yes"
        if command -v nix-shell &> /dev/null; then
            print_success "Nix: Available"
        fi
    else
        print_status "NixOS: No"
    fi

    # Check shell
    if [ -n "$SHELL" ]; then
        print_success "Shell: $SHELL"
    fi

    echo ""
}

# Provide recommendations
provide_recommendations() {
    print_header "Recommendations"
    echo "==============="

    # Check for critical issues
    if ! command -v nim &> /dev/null; then
        print_error "CRITICAL: Install Nim compiler"
        echo "  https://nim-lang.org/install.html"
        return 1
    fi

    # Check for backends
    if ! (nimble list raylib 2>/dev/null | grep -q raylib) && ! pkg-config --exists sdl2 2>/dev/null; then
        print_error "RECOMMENDED: Install at least one rendering backend"
        echo "  Raylib: nimble install raylib"
        echo "  SDL2: sudo apt install libsdl2-dev"
        return 1
    fi

    # Installation recommendations
    if [ ! -f "${BINDIR}/kryon" ]; then
        print_status "RECOMMENDED: Install Kryon using the installation script"
        echo "  ./scripts/install.sh"
    fi

    print_success "Your system appears ready for Kryon development!"

    echo ""
    echo "Quick start commands:"
    echo "  kryon doctor              # Run this check"
    echo "  kryon init basic myapp    # Create a new project"
    echo "  kryon run main.kry        # Run a Kryon application"
    echo ""

    return 0
}

# Main function
main() {
    echo "Kryon System Doctor"
    echo "==================="
    echo ""

    check_system
    check_basic_tools
    check_nim
    check_backends
    check_kryon_installation
    check_environment

    # Provide recommendations and exit with appropriate code
    if provide_recommendations; then
        exit 0
    else
        exit 1
    fi
}

# Handle command line arguments
case "${1:-}" in
    --help|-h)
        echo "Kryon System Doctor"
        echo "Usage: $0"
        echo ""
        echo "This script performs a comprehensive health check of your system"
        echo "to ensure all dependencies for Kryon development are properly"
        echo "installed and configured."
        echo ""
        exit 0
        ;;
    "")
        # No arguments, run full check
        ;;
    *)
        print_error "Unknown argument: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
esac

# Run main check
main
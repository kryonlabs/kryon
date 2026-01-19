#!/usr/bin/env bash

# Kryon Installation Script
# This script installs Kryon CLI tool and library system-wide

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PREFIX="${HOME}/.local"
BINDIR="${PREFIX}/bin"
LIBDIR="${PREFIX}/lib"
INCDIR="${PREFIX}/include/kryon"
CONFIGDIR="${HOME}/.config/kryon"

# Helper functions
print_status() {
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

# Check prerequisites
check_prerequisites() {
    print_status "Checking prerequisites..."

    # Check for make
    if ! command -v make &> /dev/null; then
        print_error "Make not found!"
        echo "Please install make (build-essential on Ubuntu/Debian)"
        exit 1
    fi
    print_success "Make found"

    # Check for raylib
    if ! pkg-config --exists raylib 2>/dev/null; then
        print_warning "Raylib not found via pkg-config"
        print_status "Raylib is recommended for desktop rendering"
    else
        print_success "Raylib available"
    fi
}

# Install using Makefile
install_with_make() {
    print_status "Building and installing Kryon..."

    # Clean previous builds
    make clean > /dev/null 2>&1 || true

    # Build and install
    if make install; then
        print_success "Installation completed successfully!"
    else
        print_error "Installation failed!"
        exit 1
    fi
}

# Verify installation
verify_installation() {
    print_status "Verifying installation..."

    # Check CLI
    if command -v "${BINDIR}/kryon" &> /dev/null; then
        print_success "CLI tool installed: ${BINDIR}/kryon"
    else
        print_error "CLI tool not found!"
        return 1
    fi

    # Check library
    if [ -f "${LIBDIR}/libkryon.a" ]; then
        print_success "Library installed: ${LIBDIR}/libkryon.a"
    else
        print_warning "Library not found at ${LIBDIR}/libkryon.a"
    fi

    # Check headers
    if [ -d "${INCDIR}" ]; then
        print_success "Headers installed: ${INCDIR}"
    else
        print_warning "Headers not found at ${INCDIR}"
    fi

    # Check configuration
    if [ -f "${CONFIGDIR}/config.yaml" ]; then
        print_success "Configuration created: ${CONFIGDIR}/config.yaml"
    else
        print_warning "Configuration not found at ${CONFIGDIR}/config.yaml"
    fi

    # Test CLI
    print_status "Testing CLI tool..."
    if "${BINDIR}/kryon" version > /dev/null 2>&1; then
        print_success "CLI tool is working!"
    else
        print_error "CLI tool test failed!"
        return 1
    fi
}

# Show usage information
show_usage() {
    echo ""
    echo "Installation completed! Here's how to use Kryon:"
    echo ""
    echo "  # Check system status"
    echo "  kryon doctor"
    echo ""
    echo "  # Create a new project"
    echo "  kryon init basic myapp"
    echo "  cd myapp"
    echo "  kryon run main.kry"
    echo ""
    echo "  # Available templates:"
    echo "  kryon init basic      # Simple app with button"
    echo "  kryon init todo       # Todo list application"
    echo "  kryon init game       # Simple game template"
    echo "  kryon init calculator # Calculator app"
    echo ""
    echo "  # Use different renderers"
    echo "  kryon run main.kry --renderer sdl2"
    echo "  kryon run main.kry --renderer skia"
    echo ""
    echo "Installation paths:"
    echo "  CLI: ${BINDIR}/kryon"
    echo "  Library: ${LIBDIR}/libkryon.a"
    echo "  Headers: ${INCDIR}"
    echo "  Config: ${CONFIGDIR}"
    echo ""
}

# Main installation flow
main() {
    echo "Kryon Installation Script"
    echo "========================"
    echo ""
    echo "This will install Kryon to ${PREFIX}"
    echo ""

    # Ask for confirmation
    read -p "Continue? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_status "Installation cancelled."
        exit 0
    fi

    check_prerequisites
    install_with_make
    verify_installation
    show_usage

    print_success "Kryon has been installed successfully! 🎉"
}

# Handle command line arguments
case "${1:-}" in
    --prefix)
        if [ -n "$2" ]; then
            PREFIX="$2"
            BINDIR="${PREFIX}/bin"
            LIBDIR="${PREFIX}/lib"
            INCDIR="${PREFIX}/include/kryon"
            CONFIGDIR="${HOME}/.config/kryon"
        fi
        ;;
    --help|-h)
        echo "Kryon Installation Script"
        echo "Usage: $0 [--prefix PREFIX]"
        echo ""
        echo "Options:"
        echo "  --prefix PREFIX    Install to custom prefix (default: ~/.local)"
        echo "  --help, -h         Show this help"
        echo ""
        exit 0
        ;;
    "")
        # No arguments, use defaults
        ;;
    *)
        print_error "Unknown argument: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
esac

# Run main installation
main
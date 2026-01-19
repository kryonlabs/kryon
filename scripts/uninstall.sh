#!/usr/bin/env bash

# Kryon Uninstallation Script
# This script removes Kryon CLI tool and library from the system

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
PKGCONFIGDIR="${PREFIX}/lib/pkgconfig"
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

# Check if Kryon is installed
check_installation() {
    local installed=false

    if [ -f "${BINDIR}/kryon" ]; then
        print_status "Found CLI tool: ${BINDIR}/kryon"
        installed=true
    fi

    if [ -f "${LIBDIR}/libkryon.a" ]; then
        print_status "Found library: ${LIBDIR}/libkryon.a"
        installed=true
    fi

    if [ -d "${INCDIR}" ]; then
        print_status "Found headers: ${INCDIR}"
        installed=true
    fi

    if [ -f "${PKGCONFIGDIR}/kryon.pc" ]; then
        print_status "Found pkg-config file: ${PKGCONFIGDIR}/kryon.pc"
        installed=true
    fi

    if [ -d "${CONFIGDIR}" ]; then
        print_status "Found configuration: ${CONFIGDIR}"
        installed=true
    fi

    if [ "$installed" = false ]; then
        print_warning "Kryon installation not found!"
        return 1
    fi

    return 0
}

# Remove CLI tool
remove_cli() {
    if [ -f "${BINDIR}/kryon" ]; then
        print_status "Removing CLI tool..."
        rm -f "${BINDIR}/kryon"
        print_success "CLI tool removed"
    fi
}

# Remove library
remove_library() {
    if [ -f "${LIBDIR}/libkryon.a" ]; then
        print_status "Removing library..."
        rm -f "${LIBDIR}/libkryon.a"
        print_success "Library removed"
    fi
}

# Remove headers
remove_headers() {
    if [ -d "${INCDIR}" ]; then
        print_status "Removing headers..."
        rm -rf "${INCDIR}"
        print_success "Headers removed"
    fi
}

# Remove pkg-config file
remove_pkgconfig() {
    if [ -f "${PKGCONFIGDIR}/kryon.pc" ]; then
        print_status "Removing pkg-config file..."
        rm -f "${PKGCONFIGDIR}/kryon.pc"
        print_success "pkg-config file removed"
    fi
}

# Remove configuration
remove_config() {
    if [ -d "${CONFIGDIR}" ]; then
        print_status "Removing configuration..."
        rm -rf "${CONFIGDIR}"
        print_success "Configuration removed"
    fi
}

# Clean up any remaining artifacts
cleanup_artifacts() {
    print_status "Cleaning up any remaining artifacts..."

    # Remove any temporary files
    rm -f /tmp/kryon_* 2>/dev/null || true

    # Remove any build artifacts in source directory if present
    if [ -f "Makefile" ]; then
        make clean > /dev/null 2>&1 || true
        print_status "Cleaned build artifacts"
    fi
}

# Show completion message
show_completion() {
    echo ""
    print_success "Kryon has been uninstalled successfully!"
    echo ""
    echo "Note: This script only removes files installed by the Kryon"
    echo "installation script. Any manually installed dependencies (like"
    echo "raylib, SDL2, etc.) will need to be removed separately."
    echo ""
    echo "To remove system packages (Ubuntu/Debian):"
    echo "  sudo apt remove libsdl2-dev libsdl2-ttf-dev libraylib-dev"
    echo ""
}

# Main uninstallation flow
main() {
    echo "Kryon Uninstallation Script"
    echo "==========================="
    echo ""

    # Check if running from correct directory
    if [ ! -f "Makefile" ]; then
        print_error "Please run this script from the Kryon source directory!"
        exit 1
    fi

    # Check if Kryon is installed
    if ! check_installation; then
        print_error "No Kryon installation found to uninstall."
        exit 1
    fi

    echo ""
    print_warning "This will remove Kryon from your system."
    echo ""

    # Ask for confirmation
    read -p "Continue with uninstallation? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_status "Uninstallation cancelled."
        exit 0
    fi

    echo ""
    print_status "Starting uninstallation..."

    remove_cli
    remove_library
    remove_headers
    remove_pkgconfig
    remove_config
    cleanup_artifacts

    show_completion
}

# Handle command line arguments
case "${1:-}" in
    --help|-h)
        echo "Kryon Uninstallation Script"
        echo "Usage: $0"
        echo ""
        echo "This script removes all Kryon files installed by the"
        echo "installation script, including:"
        echo "  - CLI tool"
        echo "  - Library files"
        echo "  - Header files"
        echo "  - Configuration files"
        echo "  - pkg-config files"
        echo ""
        echo "Note: This must be run from the Kryon source directory."
        echo ""
        exit 0
        ;;
    "")
        # No arguments
        ;;
    *)
        print_error "Unknown argument: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
esac

# Run main uninstallation
main
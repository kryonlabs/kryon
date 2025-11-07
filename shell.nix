# shell.nix (Simple Kryon development environment)
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Core toolchain
    nim
    nimble
    gcc
    gnumake
    pkg-config

    # SDL3 for rendering backend
    sdl3
    sdl3-ttf
    sdl3-image

    # Terminal rendering backend
    libtickit

    # Lua frontend support
    lua
    lua54Packages.lua

    # System libraries
    libGL
    libglvnd
    xorg.libX11
    xorg.libXrandr
    xorg.libXi
    xorg.libXcursor
    libxkbcommon

    # Development tools
    git
    gdb
    which
    tree
  ];

  shellHook = ''
    echo "Kryon Development Environment"
    echo "============================="
    echo "Nim version: $(nim --version | head -1)"
    echo ""
    echo "Quick start:"
    echo "  Build C Core: cd core && make"
    echo "  Build SDL3 renderer: cd renderers/sdl3 && make"
    echo "  Build Terminal renderer: cd renderers/terminal && make"
    echo "  Run examples: ./run_example.sh hello_world nim sdl3"
    echo "  Run terminal examples: ./run_example.sh hello_world nim terminal"
    echo ""
    echo "Available renderers: framebuffer, sdl3, terminal"
    echo "SDL3 Nim wrapper: https://github.com/dinau/sdl3_nim"
    echo "Terminal rendering: libtickit-based TUI support"
    echo ""
  '';
}

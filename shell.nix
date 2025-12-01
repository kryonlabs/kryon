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

    # Text layout and shaping
    harfbuzz
    fribidi

    # Terminal rendering backend
    libtickit

    # Lua frontend support (LuaJIT for FFI)
    luajit

    # TypeScript/JavaScript frontend support
    bun
    nodejs  # For npm compatibility if needed

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
    echo "LuaJIT version: $(luajit -v 2>/dev/null || echo 'not available')"
    echo "Bun version: $(bun --version 2>/dev/null || echo 'not available')"
    echo ""
    echo "Quick start:"
    echo "  Build C Core: cd core && make"
    echo "  Build SDL3 renderer: cd renderers/sdl3 && make"
    echo "  Build Terminal renderer: cd renderers/terminal && make"
    echo "  Run Nim examples: ./run_example.sh hello_world nim sdl3"
    echo "  Run TypeScript examples: ./run_example.sh hello_world ts"
    echo "  Run terminal examples: ./run_example.sh hello_world nim terminal"
    echo "  Run web examples: ./run_example.sh hello_world ts web"
    echo ""
    echo "Available frontends: nim, typescript (ts), lua, c"
    echo "Available renderers: sdl3, terminal, web, framebuffer"
    echo ""
  '';
}

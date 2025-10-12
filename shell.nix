{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    nim
    nimble
    gcc
    raylib
    # SDL2 dependencies
    SDL2
    SDL2_ttf
    pkg-config
  ];

  shellHook = ''
    echo "Kryon-Nim Development Environment"
    echo "=================================="
    echo "Nim version: $(nim --version | head -1)"
    echo "Nimble version: $(nimble --version | head -1)"
    echo "Available backends: Raylib, SDL2"
    echo ""
    echo "Quick start:"
    echo "  Build CLI: nimble build"
    echo ""
    echo "Run examples (default: Raylib):"
    echo "  ./bin/cli/kryon run examples/hello_world.nim"
    echo "  ./bin/cli/kryon run examples/button_demo.nim"
    echo "  ./bin/cli/kryon run examples/dropdown.nim"
    echo ""
    echo "Run with SDL2 backend:"
    echo "  ./bin/cli/kryon run examples/hello_world.nim --renderer sdl2"
    echo ""
    echo ""
  '';
}

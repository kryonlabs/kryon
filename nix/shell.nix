{ pkgs ? import <nixpkgs> {} }:

let
  kryon = import ./default.nix { inherit pkgs; };

  # Development dependencies
  devInputs = with pkgs; [
    nim2
    pkg-config

    # Backend libraries for development
    raylib
    SDL2
    SDL2_ttf

    # Additional tools
    git
    gdb
    valgrind

    # Documentation tools
    nimble

    # Build tools
    cmake
    gcc
  ];

in pkgs.mkShell {
  buildInputs = devInputs;

  # Environment variables for development
  KRYON_ROOT = toString ./.;
  KRYON_SRC = toString ../src;

  # Shell hook for convenience
  shellHook = ''
    echo "Welcome to Kryon Development Environment"
    echo "========================================="
    echo ""
    echo "Available commands:"
    echo "  make help        - Show build options"
    echo "  make doctor      - Check dependencies"
    echo "  make install     - Install to ~/.local"
    echo "  make dev         - Development build"
    echo ""
    echo "Environment variables:"
    echo "  KRYON_ROOT=$KRYON_ROOT"
    echo "  KRYON_SRC=$KRYON_SRC"
    echo ""
    echo "Backend libraries available:"
    echo "  ✓ Raylib: ${pkgs.raylib}"
    echo "  ✓ SDL2: ${pkgs.SDL2}"
    echo "  ✓ SDL2_ttf: ${pkgs.SDL2_ttf}"
    echo ""
    echo "Quick start:"
    echo "  ./bin/cli/kryon run examples/hello.nim"
    echo "  nim c -r src/cli/kryon.nim doctor"
    echo ""
  '';

  # Enable development mode
  KRYON_DEV = "1";
}
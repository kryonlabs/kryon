{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    nim
    nimble
    gcc
    raylib
  ];

  shellHook = ''
    echo "Kryon-Nim Development Environment"
    echo "=================================="
    echo "Nim version: $(nim --version | head -1)"
    echo "Nimble version: $(nimble --version | head -1)"
    echo ""
    echo "Quick start:"
    echo "  Build CLI: nimble build"
    echo "  Run examples with Kryon CLI:"
    echo "    ./bin/kryon run -f examples/hello_world.nim"
    echo "    ./bin/kryon run -f examples/button_demo.nim"
    echo "    ./bin/kryon run -f examples/dropdown.nim"
    echo "  Or compile manually:"
    echo "    nim c -r examples/hello_world.nim"
    echo ""
  '';
}

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
    echo "  mkdir -p build"
    echo "  nim c -o:build/hello_world -r examples/hello_world.nim"
    echo "  nim c -o:build/button_demo -r examples/button_demo.nim"
    echo "  nim c -o:build/counter -r examples/counter.nim"
    echo ""
  '';
}

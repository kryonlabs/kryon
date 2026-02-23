{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # The Compiler
    tinycc

    # Build Tools
    gnumake

    # Testing Tools (provides 9mount, 9p, etc.)
    plan9port

    # Network testing
    netcat
  ];

  shellHook = ''
    echo "--- Kryon Labs Dev Environment ---"
    echo "Compiler: $(tcc -v)"
    echo "Standards: C89/C90 via tcc"
    echo "Testing tools: plan9port loaded"
    echo "----------------------------------"
  '';
}

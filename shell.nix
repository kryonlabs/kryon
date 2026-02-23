{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # The Compiler
    tinycc
    gcc  # Alternative compiler for better warnings

    # Build Tools
    gnumake
    pkg-config

    # SDL2 for display client (required)
    SDL2
    SDL2.dev

    # Math library
    glibc.dev

    # Testing Tools (provides 9mount, 9p, etc.)
    plan9port

    # Network testing
    netcat
  ];

  shellHook = ''
    echo "--- Kryon Labs Dev Environment ---"
    echo "Compiler: tcc (C89/C90)"
    echo "SDL2: Loaded for display client"
    echo "Testing tools: plan9port loaded"
    echo "----------------------------------"
    echo ""
    echo "Build: make"
    echo "Run server: ./bin/kryon-server --port 17019"
    echo "Run display: ./bin/kryon-display"
    echo "Note: Binaries have RPATH set and work outside nix-shell"
  '';
}

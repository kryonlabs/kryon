{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    raylib
  ];

  shellHook = ''
    export RAYLIB_DIR=${pkgs.raylib}/include
  '';
}

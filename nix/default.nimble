{ pkgs ? import <nixpkgs> {}, static ? false }:

let
  inherit (pkgs) lib stdenv nim2 fetchFromGitHub pkg-config raylib SDL2 SDL2_ttf;

  # Backend dependencies
  backends = {
    raylib = [ raylib ];
    sdl2 = [ SDL2 SDL2_ttf ];
    skia = [ SDL2 SDL2_ttf ];  # Skia via SDL2 backend
  };

  # Choose appropriate backend libraries based on build type
  backendLibs = if static then
    lib.concatLists (attrValues backends)
  else
    [];  # Dynamic linking uses system libraries

  # Common build inputs
  buildInputs = [ nim2 pkg-config ] ++ backendLibs;

  # Native build inputs
  nativeBuildInputs = [ pkg-config ];

  # Compiler flags based on build type
  nimFlags = [
    "--define:kryonVersion=0.2.0"
    "-d:release"
    "--opt:speed"
  ] ++ lib.optionals static [
    "-d:staticBackend"
    "--passL:-static"
  ];

in stdenv.mkDerivation rec {
  pname = "kryon";
  version = "0.2.0";

  src = lib.cleanSource ../.;

  inherit buildInputs nativeBuildInputs;

  patches = [
    # This would be for any Nix-specific patches if needed
  ];

  postPatch = ''
    # Patch paths for Nix compatibility
    substituteInPlace src/cli/kryon.nim \
      --replace 'getHomeDir() / ".local" / "bin"' '"${placeholder "out"}/bin"' \
      --replace 'getHomeDir() / ".local" / "lib"' '"${placeholder "out"}/lib"' \
      --replace 'getHomeDir() / ".config" / "kryon"' '"${placeholder "out"}/etc/kryon"'
  '';

  buildPhase = ''
    runHook preBuild

    # Create build directory
    mkdir -p build

    # Build CLI tool
    nim c ${lib.concatStringsSep " " nimFlags} \
      -o:build/kryon \
      src/cli/kryon.nim

    # Build library
    nim c --app:staticlib \
      ${lib.concatStringsSep " " nimFlags} \
      --out:build/libkryon.a \
      src/kryon

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    # Install binaries
    mkdir -p $out/bin
    install -m 755 build/kryon $out/bin/

    # Install library
    mkdir -p $out/lib
    install -m 644 build/libkryon.a $out/lib/

    # Install headers
    mkdir -p $out/include/kryon
    cp -r src/kryon/*.nim $out/include/kryon/

    # Install pkg-config file
    mkdir -p $out/lib/pkgconfig
    substitute kryon.pc $out/lib/pkgconfig/kryon.pc \
      --replace '@PREFIX@' $out \
      --replace '@VERSION@' ${version}

    # Install configuration and templates
    mkdir -p $out/etc/kryon
    mkdir -p $out/etc/kryon/templates
    echo "version: ${version}" > $out/etc/kryon/config.yaml
    echo "installed_at: $(date)" >> $out/etc/kryon/config.yaml
    cp -r templates/* $out/etc/kryon/templates/ 2>/dev/null || true

    runHook postInstall
  '';

  # Enable static linking if requested
  dontDisableStatic = static;

  meta = with lib; {
    description = "Declarative UI framework for multiple platforms";
    homepage = "https://github.com/kryonlabs/kryon";
    license = licenses.bsd0;
    maintainers = with maintainers; [ ];  # Add maintainers
    platforms = platforms.all;

    # Backend dependencies for runtime
    # For static builds, these are compiled in
    # For dynamic builds, users need to install them separately
    hydraPlatforms = platforms.linux ++ platforms.darwin;

    # Long description
    longDescription = ''
      Kryon is a declarative UI framework for Nim that supports multiple
      rendering backends including Raylib, SDL2, Skia, and HTML. It provides
      a simple, expressive DSL for building user interfaces that can run
      on desktop, web, and mobile platforms.
    '';
  };
}
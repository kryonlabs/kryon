# Package information
version       = "0.2.0"
author        = "Kryon Labs"
description   = "Kryon UI Framework - Declarative UI for multiple platforms"
license       = "BSD-Zero-Clause"
srcDir        = "src"
bin           = @["cli/kryon"]
binDir        = "bin"

# Dependencies
requires "nim >= 2.0.0"
requires "cligen >= 1.6.0"  # For CLI tool

# Backend dependencies (optional - install what you need)
# SDL2 backend dependencies (requires system SDL2 and SDL_ttf libraries)
# Note: These are system dependencies that need to be installed separately:
# - Ubuntu/Debian: sudo apt install libsdl2-dev libsdl2-ttf-dev
# - macOS: brew install sdl2 sdl2_ttf
# - Windows: Download SDL2 and SDL2_ttf development libraries
# Add passL flags manually when compiling: --passL:"-lSDL2 -lSDL2_ttf"

# Raylib backend dependencies (if using Raylib backend)
# Install with: nimble install raylib

# Tasks
task test, "Run tests":
  exec "nim c -r tests/test_all.nim"

task docs, "Generate documentation":
  exec "nim doc --project --index:on --outdir:docs src/kryon.nim"

task examples, "Build all examples":
  for example in listFiles("examples"):
    if example.endsWith(".nim"):
      exec "nim c -r " & example

task examples_sdl2, "Build all examples with SDL2 backend":
  for example in listFiles("examples"):
    if example.endsWith(".nim"):
      exec "nim c -r --passL:\"-lSDL2 -lSDL2_ttf\" " & example

task install_sdl2_deps, "Show SDL2 dependency installation instructions":
  echo "SDL2 Backend Dependencies Installation:"
  echo ""
  echo "Ubuntu/Debian:"
  echo "  sudo apt install libsdl2-dev libsdl2-ttf-dev"
  echo ""
  echo "macOS:"
  echo "  brew install sdl2 sdl2_ttf"
  echo ""
  echo "Windows:"
  echo "  1. Download SDL2 development libraries from https://www.libsdl.org/"
  echo "  2. Download SDL2_ttf from https://www.libsdl.org/projects/SDL_ttf/"
  echo "  3. Extract both to accessible locations"
  echo "  4. Add to PATH or use --passL flags when compiling"
  echo ""
  echo "Compilation with SDL2:"
  echo "  nim c --passL:\"-lSDL2 -lSDL2_ttf\" your_program.nim"

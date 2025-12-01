# Package information
version       = "0.2.0"
author        = "Kryon Labs"
description   = "Kryon UI Framework - Declarative UI for multiple platforms"
license       = "BSD-Zero-Clause"
srcDir        = "bindings/nim"
bin           = @["cli/main"]
binDir        = "bin"

# Dependencies
requires "nim >= 2.0.0"
requires "cligen >= 1.6.0"  # For CLI tool
requires "parsetoml >= 0.7.0"  # For TOML configuration support
import os
# Note: naylib and other rendering backends are optional system dependencies

# Lua frontend dependencies (optional - install for Lua support)
# Requires system Lua library installation
# Note: These are system dependencies that need to be installed separately:
# - Ubuntu/Debian: sudo apt install liblua5.4-dev
# - macOS: brew install lua
# - Windows: Download Lua development libraries from lua.org
# Add passL flags manually when compiling: --passL:"-llua5.4"


# Development tasks
task dev_setup, "Setup development environment for IDE support":
  exec "nimble install --depsOnly"
  exec "echo 'Development environment setup complete!'"
  exec "echo 'VSCode should now recognize kryon_dsl imports.'"

task dev_install, "Install package locally for IDE development":
  exec "nimble develop -y"
  exec "echo 'Package installed in development mode. VSCode should now work correctly.'"

task check_syntax, "Check syntax of examples (for IDE validation)":
  exec "nim check --path:bindings/nim examples/nim/button_demo.nim"
  exec "nim check --path:bindings/nim examples/nim/hello_world.nim"
  exec "nim check --path:bindings/nim examples/nim/simple_styled_app.nim"

task test, "Run tests":
  exec "nim c -r tests/test_all.nim"

task docs, "Generate documentation":
  exec "nim doc --project --index:on --outdir:docs src/kryon.nim"

task examples, "Build all examples":
  for example in listFiles("examples/nim"):
    if example.endsWith(".nim"):
      exec "nim c -r " & example


task examples_lua, "Build all Lua examples":
  for example in listFiles("examples/lua"):
    if example.endsWith(".lua"):
      echo "Building Lua example: " & example
      let name = example.splitFile.name
      exec "gcc -o bin/" & name & " bindings/lua/kryon_lua.c -Icore/include -Lbuild -lkryon_core -llua5.4 -lm"

task build_lua_bindings, "Build Lua C bindings":
  exec "gcc -shared -fPIC -o libkryon_lua.so bindings/lua/kryon_lua.c -Icore/include -Lbuild -lkryon_core -llua5.4 -lm"

task install_lua_deps, "Show Lua dependency installation instructions":
  echo "Lua Frontend Dependencies Installation:"
  echo ""
  echo "Ubuntu/Debian:"
  echo "  sudo apt install liblua5.4-dev lua5.4"
  echo ""
  echo "macOS:"
  echo "  brew install lua"
  echo ""
  echo "Windows:"
  echo "  1. Download Lua development libraries from https://lua.org/"
  echo "  2. Extract to accessible location"
  echo "  3. Add to PATH or use --passL flags when compiling"
  echo ""
  echo "Compilation with Lua:"
  echo "  gcc -o program bindings/lua/kryon_lua.c -Icore/include -Lbuild -lkryon_core -llua5.4 -lm"


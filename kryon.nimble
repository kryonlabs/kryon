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

# Tasks
task test, "Run tests":
  exec "nim c -r tests/test_all.nim"

task docs, "Generate documentation":
  exec "nim doc --project --index:on --outdir:docs src/kryon.nim"

task examples, "Build all examples":
  for example in listFiles("examples"):
    if example.endsWith(".nim"):
      exec "nim c -r " & example

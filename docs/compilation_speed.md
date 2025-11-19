# Speeding Up Kryon Compilation

## Quick Wins

### 1. Use Incremental Compilation (nim.cfg)
Add to `nim.cfg`:
```
--incremental:on
--nimcache:build/nimcache
```

### 2. Disable Optimizations During Development
Add to `nim.cfg` for faster debug builds:
```
--opt:none
--debugger:native
```

### 3. Reduce Hints and Warnings
```
--hints:off
--warnings:off
```

### 4. Use Parallel Compilation
```bash
nim c --threads:on --parallelBuild:4 your_file.nim
```

## Long-term Optimizations

### 1. Split Large Files
The 2800-line `kryon_dsl.nim` causes slow macro expansion. Consider:
- Keep macros in separate files
- Use `{.push.}` and `{.pop.}` pragmas for sections

### 2. Reduce Macro Complexity
- Simplify macro nesting
- Use templates where possible instead of macros
- Cache macro results when possible

### 3. Precompile Common Modules
```bash
nim c --compileOnly runtime.nim
nim c --compileOnly core_kryon.nim
```

## Development Workflow

### Fast Iteration Mode
Create a `nim.dev.cfg`:
```
--opt:none
--assertions:on
--lineDir:on
--stackTrace:on
--hints:off
--warnings:off
```

Use: `nim c --nimconf:nim.dev.cfg your_file.nim`

### Release Build
Use: `nim c -d:release your_file.nim`

## Benchmarking
Time your builds:
```bash
time nim c your_file.nim
```

## Current Stats
- First compile: ~30-60s (cold cache)  
- Incremental: ~5-15s (warm cache)
- Goal: <5s incremental builds

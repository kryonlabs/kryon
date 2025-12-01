# Kryon Multi-Frontend CLI

The Kryon CLI now automatically detects and runs applications in multiple frontend languages!

## Auto-Detection

The CLI now intelligently detects file extensions and uses the appropriate runtime:

```bash
# Detects .lua file and runs with LuaJIT
kryon run myapp
# → Finds myapp.lua and runs with luajit

# Detects .nim file and compiles with Nim
kryon run myapp
# → Finds myapp.nim and compiles

# Detects .ts/.js file and runs with Bun/Node
kryon run myapp
# → Finds myapp.ts and runs with bun
```

## Supported Frontends

| Extension | Runtime | Auto-Detected | Hot Reload |
|-----------|---------|---------------|------------|
| `.lua` | LuaJIT | ✅ | ✅ |
| `.nim` | Nim compiler | ✅ | ✅ |
| `.ts` | Bun/Node | ✅ | ⏳ |
| `.js` | Bun/Node | ✅ | ⏳ |
| `.c` | GCC/Clang | ✅ | ❌ |

## Example Usage

### Running a Lua Application

```bash
# In your project directory with waozi-website.lua
$ kryon run waozi-website

🔍 Detected .lua file: waozi-website.lua
🚀 Running Kryon application...
📁 File: waozi-website.lua
🎨 Frontend: .lua
🎯 Target: native
🌙 Running Lua application with LuaJIT...
```

### Running a Nim Application

```bash
$ kryon run hello_world

🔍 Detected .nim file: hello_world.nim
🚀 Running Kryon application...
📁 File: hello_world.nim
🎨 Frontend: .nim
🎯 Target: native
🔨 Building...
✅ Build successful!
🏃 Running application...
```

### Explicit File Extension

You can also specify the full filename:

```bash
kryon run myapp.lua    # Explicitly run Lua version
kryon run myapp.nim    # Explicitly run Nim version
kryon run myapp.ts     # Explicitly run TypeScript version
```

## Detection Order

When you run `kryon run myapp`, it searches for files in this order:

1. `.lua` - Lua/LuaJIT frontend
2. `.nim` - Nim frontend
3. `.ts` - TypeScript frontend
4. `.js` - JavaScript frontend
5. `.c` - C frontend

## Hot Reload Mode

The `kryon dev` command also supports auto-detection:

```bash
# Lua hot reload
$ kryon dev waozi-website

🔥 Starting Hot Reload Development Mode...
📁 File: waozi-website.lua
🎨 Frontend: .lua
🎯 Target: native
🌙 Lua hot reload mode...
🏃 Running waozi-website.lua...

# Edit and save the file - it auto-reloads!
🔄 File changed, restarting...
```

## Error Handling

The CLI provides helpful error messages when dependencies are missing:

```bash
$ kryon run myapp.lua
❌ LuaJIT not found!
   Install LuaJIT or run in nix-shell:
   nix-shell -p luajit --run 'kryon run myapp.lua'
```

## Environment Setup

### For Lua Development

```bash
# Using Nix
nix-shell

# Or install LuaJIT system-wide
# Ubuntu/Debian
sudo apt install luajit

# macOS
brew install luajit

# Arch
sudo pacman -S luajit
```

### For TypeScript Development

```bash
# Install Bun (recommended)
curl -fsSL https://bun.sh/install | bash

# Or use Node.js
nix-shell -p nodejs
```

## Benefits

✅ **Zero Configuration** - Just run `kryon run myapp` and it figures out the rest
✅ **Multi-Language Support** - Write frontends in Lua, Nim, TypeScript, JavaScript, or C
✅ **Consistent Experience** - Same CLI commands across all languages
✅ **Hot Reload** - Fast development iteration for Lua and Nim
✅ **Shared IR Core** - All frontends use the same C IR core

## Implementation Details

The CLI uses a simple but robust detection algorithm:

```nim
proc detectFileWithExtensions(baseName: string): tuple[found: bool, path: string, ext: string] =
  const extensions = [".lua", ".nim", ".ts", ".js", ".c"]

  # If file has extension, use it directly
  if baseName.contains('.'):
    if fileExists(baseName):
      return (true, baseName, ext)

  # Try each extension
  for ext in extensions:
    let testPath = baseName & ext
    if fileExists(testPath):
      return (true, testPath, ext)

  return (false, "", "")
```

Then dispatch to the appropriate runtime:

```nim
case frontend:
of ".lua":
  # Run with LuaJIT
  execShellCmd("luajit " & file)

of ".nim":
  # Compile with Nim
  execShellCmd("nim c -r " & file)

of ".ts", ".js":
  # Run with Bun or Node
  execShellCmd("bun run " & file)
```

## Future Enhancements

- 🔄 TypeScript hot reload via Bun's `--watch` flag
- 🐍 Python frontend support
- 🦀 Rust frontend support
- 📦 Auto-install missing runtimes
- 🔧 Custom frontend configurations in `kryon.toml`

## Example Projects

### Lua Project

```
myapp/
├── main.lua          # Auto-detected and run with LuaJIT
└── components/
    ├── header.lua
    └── footer.lua
```

### Multi-Language Project

```
myapp/
├── main.nim          # Nim version (detected first by default)
├── main.lua          # Lua version
└── main.ts           # TypeScript version

# Run specific version
$ kryon run main.nim   # Nim
$ kryon run main.lua   # Lua
$ kryon run main.ts    # TypeScript
```

## Troubleshooting

### LuaJIT not found

**Problem**: `luajit: command not found`

**Solution**:
```bash
# Enter nix-shell (LuaJIT is pre-installed)
nix-shell

# Or install globally
sudo apt install luajit  # Ubuntu/Debian
brew install luajit      # macOS
```

### Multiple files found

**Problem**: Both `myapp.lua` and `myapp.nim` exist

**Solution**: Specify the full filename:
```bash
kryon run myapp.lua   # Run Lua version
kryon run myapp.nim   # Run Nim version
```

### File not found

**Problem**: `❌ Could not find file: myapp`

**Solution**: Ensure the file exists with a supported extension:
```bash
ls myapp.*            # Check what files exist
kryon run myapp.lua   # Be explicit about which one to run
```

## Summary

The Kryon CLI is now a **universal frontend launcher** that:

1. ✅ Auto-detects file extensions
2. ✅ Runs the appropriate runtime (LuaJIT, Nim, Bun, Node)
3. ✅ Provides helpful error messages
4. ✅ Supports hot reload for rapid development
5. ✅ Works seamlessly with all Kryon frontends

**One CLI, Multiple Languages, Same IR Core!**

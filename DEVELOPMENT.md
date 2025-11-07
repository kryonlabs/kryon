# Development Setup for Kryon UI Framework

This guide helps you set up your development environment for proper VSCode syntax highlighting and IntelliSense support when working with Kryon DSL.

## Prerequisites

1. **Install Nim**: Follow the official Nim installation guide at https://nim-lang.org/install.html
2. **Install VSCode**: Download from https://code.visualstudio.com/
3. **Install Nim VSCode Extension**:
   - Open VSCode
   - Go to Extensions (Ctrl+Shift+X)
   - Search for "nim" and install the extension by "kosz78"

## Quick Setup (Recommended)

### Method 1: Using Nimble Commands

```bash
# Clone and navigate to the project
git clone <repository-url>
cd kryon

# Install package in development mode (recommended for IDE support)
nimble dev_install

# OR setup dependencies only
nimble dev_setup
```

### Method 2: Manual Setup

1. **Install dependencies**:
   ```bash
   nimble install --depsOnly
   ```

2. **Build the C core**:
   ```bash
   make -C core
   ```

3. **Verify VSCode Configuration**:
   - Open the project in VSCode
   - The `.vscode/settings.json` file should automatically configure the Nim language server
   - Try opening an example file like `examples/nim/button_demo.nim`
   - The `import kryon_dsl` line should no longer show an error

## Configuration Files

### VSCode Settings (`.vscode/settings.json`)
The project includes VSCode configuration that:
- Adds `bindings/nim` to Nim module search paths
- Configures the Nim language server for proper IntelliSense
- Sets up proper compiler options for development

### Nim Configuration (`kryon.nim.cfg`)
The main configuration file includes:
- Module search paths: `--path:bindings/nim`
- C core integration settings
- Compiler optimization flags

### Nimble Configuration (`kryon.nimble`)
Development tasks available:
- `nimble dev_setup` - Setup development environment
- `nimble dev_install` - Install package locally
- `nimble check_syntax` - Validate syntax of all examples

## Troubleshooting

### VSCode Still Shows "Cannot open file: kryon_dsl"

1. **Reload VSCode**: Press `Ctrl+Shift+P` and type "Developer: Reload Window"
2. **Check Nim Extension**: Ensure the Nim extension is installed and enabled
3. **Verify Path Configuration**:
   - Open `examples/nim/button_demo.nim`
   - Check if the import line still shows errors
   - Run `nim check examples/nim/button_demo.nim` in terminal to verify it works
4. **Install Package Locally**:
   ```bash
   nimble develop -y
   ```

### IntelliSense Not Working

1. **Restart Nim Language Server**: Press `Ctrl+Shift+P` and type "Nim: Restart nimsuggest"
2. **Check VSCode Settings**: Ensure `.vscode/settings.json` is properly configured
3. **Verify Nim Installation**: Run `nim --version` to ensure Nim is properly installed

### Compilation Issues

1. **Build C Core**: `make -C core`
2. **Check Dependencies**: `nimble install --depsOnly`
3. **Use Build Scripts**: `./run_example.sh button_demo`

## Project Structure

```
kryon/
├── bindings/nim/           # Nim DSL bindings (main source)
│   ├── kryon_dsl.nim      # Main DSL module
│   ├── runtime.nim        # Runtime support
│   └── core_kryon.nim     # C core bindings
├── core/                   # C core implementation
├── examples/nim/          # Example applications
├── renderers/             # Rendering backends
├── .vscode/               # VSCode configuration
├── kryon.nimble          # Package configuration
└── kryon.nim.cfg         # Nim compiler settings
```

## Development Workflow

1. **Make Changes**: Edit files in `bindings/nim/` or `core/`
2. **Build Core**: `make -C core` (if C core changes)
3. **Test Examples**: `./run_example.sh button_demo`
4. **Check Syntax**: `nimble check_syntax`
5. **Commit Changes**: Use git as usual

## Getting Help

If you encounter issues with VSCode setup:
1. Check that all prerequisites are installed
2. Try the quick setup commands above
3. Verify the configuration files are present
4. Check the terminal output for specific error messages

The Kryon project should now provide proper syntax highlighting and IntelliSense in VSCode!
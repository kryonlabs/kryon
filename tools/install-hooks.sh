#!/bin/bash

# Kryon-C Development Environment Setup Script
# Installs all necessary development tools and pre-commit hooks

set -e

echo "🚀 Setting up Kryon-C development environment..."

# =============================================================================
# DETECT PLATFORM
# =============================================================================

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    PLATFORM="windows"
else
    echo "❌ Unsupported platform: $OSTYPE"
    exit 1
fi

echo "📱 Detected platform: $PLATFORM"

# =============================================================================
# INSTALL DEPENDENCIES
# =============================================================================

install_linux_deps() {
    echo "📦 Installing Linux dependencies..."
    
    # Update package list
    sudo apt-get update
    
    # Essential build tools
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        pkg-config
    
    # Code quality tools
    sudo apt-get install -y \
        clang \
        clang-format \
        clang-tidy \
        cppcheck \
        valgrind
    
    # Development libraries
    sudo apt-get install -y \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libasound2-dev \
        libpulse-dev \
        libudev-dev \
        libncurses5-dev \
        libxcursor-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxi-dev \
        libxext-dev \
        libwayland-dev \
        libxkbcommon-dev
    
    # Python for pre-commit
    sudo apt-get install -y \
        python3 \
        python3-pip
}

install_macos_deps() {
    echo "📦 Installing macOS dependencies..."
    
    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    # Essential build tools
    brew install \
        cmake \
        ninja \
        pkg-config
    
    # Code quality tools
    brew install \
        llvm \
        cppcheck
    
    # Python for pre-commit
    brew install python3
}

install_windows_deps() {
    echo "📦 Installing Windows dependencies..."
    echo "Please install the following manually:"
    echo "1. Visual Studio 2019 or later with C++ support"
    echo "2. CMake (https://cmake.org/download/)"
    echo "3. Git (https://git-scm.com/download/win)"
    echo "4. Python 3.8+ (https://python.org/downloads/)"
    echo "5. LLVM/Clang (https://releases.llvm.org/download.html)"
    echo ""
    echo "Then run this script again."
    
    # Check if tools are available
    if ! command -v cmake &> /dev/null; then
        echo "❌ CMake not found. Please install CMake and add it to PATH."
        exit 1
    fi
    
    if ! command -v python &> /dev/null; then
        echo "❌ Python not found. Please install Python and add it to PATH."
        exit 1
    fi
}

# Install platform-specific dependencies
case $PLATFORM in
    "linux")
        install_linux_deps
        ;;
    "macos")
        install_macos_deps
        ;;
    "windows")
        install_windows_deps
        ;;
esac

# =============================================================================
# INSTALL PYTHON TOOLS
# =============================================================================

echo "🐍 Installing Python development tools..."

# Upgrade pip
python3 -m pip install --upgrade pip

# Install pre-commit
python3 -m pip install pre-commit

# Install additional development tools
python3 -m pip install \
    cmake-format \
    mdformat \
    mdformat-gfm \
    mdformat-black

# =============================================================================
# SETUP PRE-COMMIT HOOKS
# =============================================================================

echo "🪝 Setting up pre-commit hooks..."

# Install pre-commit hooks
pre-commit install

# Install pre-push hooks
pre-commit install --hook-type pre-push

# Install commit message hooks
pre-commit install --hook-type commit-msg

# Run hooks on all files to test
echo "🧪 Testing pre-commit hooks on all files..."
pre-commit run --all-files || {
    echo "⚠️  Some pre-commit checks failed. This is normal for initial setup."
    echo "   Run 'pre-commit run --all-files' again after fixing any issues."
}

# =============================================================================
# SETUP GIT CONFIGURATION
# =============================================================================

echo "⚙️  Configuring Git settings..."

# Set up Git to use the hooks
git config core.hooksPath .git/hooks

# Configure Git to use clang-format for C files
git config diff.cpp.textconv "clang-format"

# Set up useful Git aliases
git config alias.st status
git config alias.co checkout
git config alias.br branch
git config alias.ci commit
git config alias.unstage 'reset HEAD --'
git config alias.last 'log -1 HEAD'
git config alias.visual '!gitk'

# =============================================================================
# CREATE DEVELOPMENT SCRIPTS
# =============================================================================

echo "📝 Creating development scripts..."

# Create format script
cat > tools/format.sh << 'EOF'
#!/bin/bash
echo "🎨 Formatting all C source files..."
find src include -name "*.c" -o -name "*.h" | xargs clang-format -i
echo "✅ Code formatting complete!"
EOF
chmod +x tools/format.sh

# Create lint script
cat > tools/lint.sh << 'EOF'
#!/bin/bash
echo "🔍 Running static analysis..."
find src -name "*.c" | xargs clang-tidy --config-file=.clang-tidy
cppcheck --enable=all --std=c99 --suppress=missingIncludeSystem src/ include/
echo "✅ Static analysis complete!"
EOF
chmod +x tools/lint.sh

# Create test script
cat > tools/test.sh << 'EOF'
#!/bin/bash
echo "🧪 Running all tests..."
if [ ! -d "build" ]; then
    mkdir build
    cd build
    cmake .. -DKRYON_BUILD_TESTS=ON
    cd ..
fi
cd build
cmake --build .
ctest --output-on-failure --verbose
echo "✅ Tests complete!"
EOF
chmod +x tools/test.sh

# Create clean script
cat > tools/clean.sh << 'EOF'
#!/bin/bash
echo "🧹 Cleaning build artifacts..."
rm -rf build/
rm -rf .pytest_cache/
rm -rf __pycache__/
find . -name "*.o" -delete
find . -name "*.so" -delete
find . -name "*.a" -delete
find . -name "core.*" -delete
echo "✅ Clean complete!"
EOF
chmod +x tools/clean.sh

# =============================================================================
# SETUP DEVELOPMENT ENVIRONMENT
# =============================================================================

echo "🔧 Setting up development environment..."

# Create initial build directory
mkdir -p build

# Create .env file for development
cat > .env << 'EOF'
# Kryon-C Development Environment Variables

# Build configuration
CMAKE_BUILD_TYPE=Debug
KRYON_BUILD_TESTS=ON
KRYON_BUILD_EXAMPLES=ON
KRYON_BUILD_TOOLS=ON

# Feature flags
KRYON_ENABLE_PROFILER=ON
KRYON_ENABLE_DEBUGGER=ON
KRYON_ENABLE_NETWORK=ON
KRYON_ENABLE_AUDIO=ON

# Renderer backends
KRYON_RENDERER_SOFTWARE=ON
KRYON_RENDERER_TERMINAL=ON
KRYON_RENDERER_WEBGL=OFF
KRYON_RENDERER_WGPU=OFF
KRYON_RENDERER_RAYLIB=ON

# Script engines
KRYON_SCRIPT_LUA=ON
KRYON_SCRIPT_JAVASCRIPT=ON
KRYON_SCRIPT_PYTHON=OFF
KRYON_SCRIPT_KRYON_LISP=ON

# Development options
KRYON_ENABLE_SANITIZERS=ON
KRYON_ENABLE_COVERAGE=OFF
EOF

# =============================================================================
# FINAL SETUP
# =============================================================================

echo "📋 Creating development quick reference..."

cat > DEVELOPMENT.md << 'EOF'
# Kryon-C Development Quick Reference

## Essential Commands

```bash
# Format all code
./tools/format.sh

# Run static analysis
./tools/lint.sh  

# Run tests
./tools/test.sh

# Clean build artifacts
./tools/clean.sh

# Run pre-commit checks manually
pre-commit run --all-files

# Build project
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## Git Workflow

```bash
# Create feature branch
git checkout -b feature/my-feature

# Make changes and commit (pre-commit hooks will run)
git add .
git commit -m "Add my feature"

# Push changes
git push origin feature/my-feature
```

## Debugging

```bash
# Debug build with AddressSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug -DKRYON_ENABLE_SANITIZERS=ON ..

# Run with Valgrind
valgrind --tool=memcheck --leak-check=full ./build/bin/kryon

# Generate coverage report
cmake -DKRYON_ENABLE_COVERAGE=ON ..
make && make test
gcov src/*.c
```

## IDE Setup

### VS Code
1. Install C/C++ extension
2. Install CMake Tools extension
3. Open workspace: `code kryon-c.code-workspace`

### CLion
1. Open CMakeLists.txt as project
2. Configure with Debug profile
3. Enable clang-tidy inspections
EOF

echo ""
echo "✅ Development environment setup complete!"
echo ""
echo "📚 Next steps:"
echo "   1. Run './tools/test.sh' to verify everything works"
echo "   2. Read DEVELOPMENT.md for development workflow"
echo "   3. Start coding! Pre-commit hooks will keep code quality high"
echo ""
echo "🎉 Happy coding!"
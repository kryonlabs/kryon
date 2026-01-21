#!/usr/bin/env python3
"""
Build script for Kryon Python bindings
Compiles cffi definitions and prepares the package
"""

import os
import sys
import subprocess
from pathlib import Path

def run_command(cmd, description=""):
    """Run a command and exit on failure"""
    print(f"{' ' * 4}{description or cmd}")
    result = subprocess.run(cmd, shell=True)
    if result.returncode != 0:
        print(f"✗ Failed: {description or cmd}")
        sys.exit(1)
    return result

def main():
    print("Building Kryon Python bindings...")

    # Check if cffi is available
    try:
        import cffi
    except ImportError:
        print("✗ cffi not found. Install with: pip install cffi")
        sys.exit(1)

    # Build the FFI module
    script_dir = Path(__file__).parent
    ffi_module = script_dir / "kryon" / "core" / "ffi_build.py"

    if ffi_module.exists():
        print("\n1. Compiling cffi bindings...")
        os.chdir(script_dir / "kryon" / "core")
        run_command(f"{sys.executable} ffi_build.py", "Compiling FFI")
        os.chdir(script_dir)

    # Run tests (non-blocking - warn but don't fail build)
    print("\n2. Running tests...")
    result = subprocess.run(f"{sys.executable} -m pytest tests/ -v", shell=True)
    if result.returncode != 0:
        print("⚠ Tests failed - see errors above")
    else:
        print("✓ Tests passed")

    print("\n✓ Build complete!")
    print("\nTo install in development mode:")
    print("  pip install -e .")

if __name__ == "__main__":
    main()

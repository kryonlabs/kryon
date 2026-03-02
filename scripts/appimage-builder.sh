#!/bin/bash
# Kryon AppImage Builder
# Creates .AppImage packages for Linux distribution

set -e

BINARY_PATH="$1"
OUTPUT_DIR="$2"

if [ -z "$BINARY_PATH" ] || [ -z "$OUTPUT_DIR" ]; then
    echo "Usage: $0 <binary_path> <output_dir>"
    exit 1
fi

if [ ! -f "$BINARY_PATH" ]; then
    echo "Error: Binary not found: $BINARY_PATH"
    exit 1
fi

BINARY_NAME=$(basename "$BINARY_PATH")
APPDIR="$OUTPUT_DIR/AppDir"

echo "Creating AppImage for $BINARY_NAME..."

# Create AppDir structure
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "$BINARY_PATH" "$APPDIR/usr/bin/"

# Create .desktop file
cat > "$APPDIR/usr/share/applications/${BINARY_NAME}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Kryon Window Manager
Exec=${BINARY_NAME}
Icon=${BINARY_NAME}
Categories=System; WindowManager;
EOF

# Create AppRun script
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "${HERE}/usr/bin/$(basename "$SELF" .AppImage)" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# TODO: Copy dependencies using linuxdeploy or appimagetool
# For now, just create a simple AppImage structure

echo "AppDir created at $APPDIR"
echo "To create final AppImage, run: appimagetool $APPDIR"

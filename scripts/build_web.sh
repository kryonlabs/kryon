#!/bin/bash
#
# build_web.sh - Build Kryon examples for web target
#
# Usage:
#   ./scripts/build_web.sh <example_name>     # Build single example
#   ./scripts/build_web.sh all                # Build all examples
#   ./scripts/build_web.sh list               # List available examples
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
EXAMPLES_DIR="$PROJECT_ROOT/examples"
BUILD_DIR="$PROJECT_ROOT/build"
WEB_OUTPUT_DIR="$PROJECT_ROOT/web_output"

# Ensure build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}❌ Build directory not found. Please run ./scripts/build.sh first${NC}"
    exit 1
fi

# Ensure web output directory exists
mkdir -p "$WEB_OUTPUT_DIR"

# Function to print colored output
print_status() {
    echo -e "${BLUE}$1${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# Function to list all available examples
list_examples() {
    print_status "📋 Available examples:"
    echo ""

    if [ ! -d "$EXAMPLES_DIR" ]; then
        print_error "Examples directory not found: $EXAMPLES_DIR"
        exit 1
    fi

    for file in "$EXAMPLES_DIR"/*.kry; do
        if [ -f "$file" ]; then
            basename=$(basename "$file" .kry)
            echo "  • $basename"
        fi
    done
    echo ""
}

# Function to build a single example for web
build_web_example() {
    local example_name="$1"
    local kry_file="$EXAMPLES_DIR/${example_name}.kry"
    local krb_file="$WEB_OUTPUT_DIR/${example_name}/${example_name}.krb"
    local output_dir="$WEB_OUTPUT_DIR/${example_name}"

    if [ ! -f "$kry_file" ]; then
        print_error "Example not found: $kry_file"
        return 1
    fi

    print_status "🔨 Building web target for: $example_name"

    # Create output directory
    mkdir -p "$output_dir"

    # Step 1: Compile .kry to .krb
    print_status "  1️⃣  Compiling $example_name.kry → $example_name.krb"
    if ! "$BUILD_DIR/bin/kryon" compile "$kry_file" -o "$krb_file"; then
        print_error "Failed to compile $example_name.kry"
        return 1
    fi
    print_success "  Compiled successfully"

    # Step 2: Scripting currently disabled for web builds
    print_status "  2️⃣  Scripting support disabled for web targets"

    # Step 3: Generate HTML/CSS/JS from KRB (using web renderer)
    # NOTE: This requires web renderer integration in the runtime
    # For now, we'll create a simple HTML template

    print_status "  3️⃣  Generating web files..."

    # Copy runtime JavaScript
    cp "$PROJECT_ROOT/web/runtime/kryon-runtime.js" "$output_dir/runtime.js"

    # Create index.html with conditional placeholder scripting loading
    cat > "$output_dir/index.html" << EOF
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Kryon - $example_name</title>
    <link rel="stylesheet" href="styles.css">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
            background: #f5f5f5;
            padding: 20px;
        }
        #app-container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
            padding: 20px;
        }
        .kryon-info {
            background: #e3f2fd;
            border-left: 4px solid #2196f3;
            padding: 15px;
            margin-bottom: 20px;
            border-radius: 4px;
        }
    </style>
</head>
<body>
    <div class="kryon-info">
        <h2>🚀 Kryon Web Demo: $example_name</h2>
        <p>This page was generated from <code>$example_name.kry</code></p>
    </div>

    <div id="app-container">
        <p>⚠️ Web renderer integration in progress...</p>
        <p>To fully render this example, the Kryon web renderer needs to be integrated into the build pipeline.</p>
        <p>KRB file available at: <code>$example_name.krb</code></p>
    </div>

    <!-- Kryon Runtime -->
    <script src="runtime.js"></script>
</body>
</html>
EOF

    # Create empty styles.css (will be populated by web renderer)
    cat > "$output_dir/styles.css" << EOF
/* Kryon Generated Styles for $example_name */

.kryon-container {
    position: relative;
}

.kryon-button {
    cursor: pointer;
    transition: all 0.2s;
}

.kryon-button:hover {
    filter: brightness(0.9);
}

.kryon-input {
    outline: none;
    transition: border-color 0.2s;
}

.kryon-input:focus {
    border-color: #2196f3;
}
EOF

    print_success "  Web files generated"

    # Step 3: Show output
    print_success "🎉 Build complete!"
    echo ""
    print_status "📁 Output directory: $output_dir"
    print_status "🌐 Open in browser: file://$output_dir/index.html"
    echo ""

    return 0
}

# Function to build all examples
build_all_examples() {
    print_status "🏗️  Building all examples for web..."
    echo ""

    local success_count=0
    local fail_count=0

    for file in "$EXAMPLES_DIR"/*.kry; do
        if [ -f "$file" ]; then
            example_name=$(basename "$file" .kry)
            if build_web_example "$example_name"; then
                ((success_count++))
            else
                ((fail_count++))
            fi
            echo ""
        fi
    done

    echo ""
    print_status "📊 Build Summary:"
    print_success "  Successful: $success_count"
    if [ $fail_count -gt 0 ]; then
        print_error "  Failed: $fail_count"
    fi
    echo ""
}

# Main script logic
main() {
    if [ $# -eq 0 ]; then
        echo "Usage: $0 <example_name|all|list>"
        echo ""
        echo "Examples:"
        echo "  $0 hello-world    # Build hello-world example"
        echo "  $0 all            # Build all examples"
        echo "  $0 list           # List available examples"
        exit 1
    fi

    case "$1" in
        list)
            list_examples
            ;;
        all)
            build_all_examples
            ;;
        *)
            build_web_example "$1"
            ;;
    esac
}

main "$@"

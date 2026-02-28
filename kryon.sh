#!/bin/bash
# Kryon convenience wrapper - launches WM and display viewer together

# Default values
MARROW_HOST="127.0.0.1"
MARROW_PORT="17010"
WM_ARGS=""
VIEW_ARGS=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --marrow-host)
            MARROW_HOST="$2"
            shift 2
            ;;
        --marrow-port)
            MARROW_PORT="$2"
            shift 2
            ;;
        --run|--blank|--list-examples|--help|--dump-screen)
            # Pass to WM
            WM_ARGS="$WM_ARGS $1"
            if [[ "$1" == "--run" ]]; then
                WM_ARGS="$WM_ARGS $2"
                shift 2
            else
                shift
            fi
            ;;
        --verbose|--stay-open|--dump-screen)
            # Pass to viewer
            VIEW_ARGS="$VIEW_ARGS $1"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --marrow-host HOST    Marrow host (default: 127.0.0.1)"
            echo "  --marrow-port PORT    Marrow port (default: 17010)"
            echo "  --run FILE.kry        Load and run the specified .kry file"
            echo "  --list-examples       List available example files"
            echo "  --blank               Clear screen to black and wait"
            echo "  --verbose             Enable verbose output in viewer"
            echo "  --help                Show help message"
            echo ""
            echo "Examples:"
            echo "  $0 --run examples/minimal.kry"
            echo "  $0 --run examples/widgets/button.kry"
            echo "  $0 --list-examples"
            exit 1
            ;;
    esac
done

# Find script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Launch viewer in background (connects to Marrow)
echo "Starting display viewer (connecting to Marrow at $MARROW_HOST:$MARROW_PORT)..."
"$SCRIPT_DIR/kryon-view" --host "$MARROW_HOST" --port "$MARROW_PORT" $VIEW_ARGS &
VIEW_PID=$!

# Give viewer time to start
sleep 0.5

# Launch WM in foreground (also connects to Marrow)
echo "Starting window manager..."
"$SCRIPT_DIR/kryon-wm" $WM_ARGS

# WM exited - kill viewer
echo "Window manager exited, stopping display viewer..."
kill $VIEW_PID 2>/dev/null
wait $VIEW_PID 2>/dev/null

echo "Kryon stopped"

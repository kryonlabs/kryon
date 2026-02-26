#!/bin/bash
# Launch Marrow + Kryon Window Manager + Display Client

# Kill any existing instances
echo "=== Cleaning up existing instances ==="
pkill -9 marrow 2>/dev/null || true
pkill -9 kryon-wm 2>/dev/null || true
pkill -9 kryon-display 2>/dev/null || true
sleep 1
echo ""

# Start Marrow
echo "=== Starting Marrow Kernel (port 17010) ==="
cd /mnt/storage/Projects/KryonLabs/marrow
nix-shell --run "./bin/marrow > /tmp/marrow.log 2>&1" &
MARROW_PID=$!
echo "Marrow PID: $MARROW_PID"
sleep 2

# Start Kryon Window Manager
echo ""
echo "=== Starting Kryon Window Manager ==="
cd /mnt/storage/Projects/KryonLabs/kryon
nix-shell --run "./bin/kryon-wm" &
KRYON_WM_PID=$!
echo "Kryon WM PID: $KRYON_WM_PID"
sleep 2

# Start Display Client
echo ""
echo "=== Starting Kryon Display Client ==="
cd /mnt/storage/Projects/KryonLabs/kryon
nix-shell --run "./bin/kryon-display --port 17010" &
KRYON_DISPLAY_PID=$!
echo "Kryon Display PID: $KRYON_DISPLAY_PID"

echo ""
echo "=== All Services Running ==="
echo "Marrow kernel:  PID $MARROW_PID (logs: /tmp/marrow.log)"
echo "Kryon WM:       PID $KRYON_WM_PID"
echo "Kryon Display:  PID $KRYON_DISPLAY_PID"
echo ""
echo "You should see an SDL2 window with the rendered content!"
echo "Press Ctrl-C to stop all services"

# Handle Ctrl-C
trap "echo ''; echo 'Stopping all services...'; kill $MARROW_PID $KRYON_WM_PID $KRYON_DISPLAY_PID 2>/dev/null; exit" INT TERM

wait

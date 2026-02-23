#!/usr/bin/env bash
#
# Kryon Test Script
# Launches server and display client for easy testing
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Port configuration
PORT=${KRYON_PORT:-17019}
HOST="127.0.0.1"

# Binary paths
SERVER_BIN="./bin/kryon-server"
CLIENT_BIN="./bin/kryon-display"

# PIDs for cleanup
SERVER_PID=""
CLIENT_PID=""

# Cleanup function
cleanup() {
    echo ""
    echo -e "${YELLOW}Shutting down server...${NC}"

    if [ -n "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi

    # Final cleanup in case anything is still running
    pkill -9 -f "kryon-server" 2>/dev/null || true

    echo "Done."
    exit 0
}

# Set trap for cleanup on interrupt
trap cleanup SIGINT SIGTERM

# Check if binaries exist and build if needed
if [ ! -f "$SERVER_BIN" ] || [ ! -f "$CLIENT_BIN" ]; then
    echo -e "${YELLOW}Binaries not found, building...${NC}"
    nix-shell --run 'make'
    if [ $? -ne 0 ]; then
        echo -e "${RED}Build failed${NC}"
        exit 1
    fi
    echo -e "${GREEN}Build complete${NC}"
fi

# Kill any existing processes
echo -e "${YELLOW}Cleaning up any existing processes...${NC}"
pkill -9 -f "kryon-server" 2>/dev/null || true
pkill -9 -f "kryon-display" 2>/dev/null || true
sleep 1

# Start server
echo -e "${GREEN}Starting Kryon Server on port $PORT...${NC}"
$SERVER_BIN --port $PORT &
SERVER_PID=$!

# Wait for server to start
echo "Waiting for server to initialize..."
sleep 2

# Check if server is still running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}Error: Server failed to start${NC}"
    exit 1
fi

echo -e "${GREEN}Server started (PID: $SERVER_PID)${NC}"

# Start client
echo -e "${GREEN}Starting Display Client...${NC}"
$CLIENT_BIN --host $HOST --port $PORT &
CLIENT_PID=$!

echo -e "${GREEN}Client started (PID: $CLIENT_PID)${NC}"
echo ""
echo "=========================================="
echo "  Kryon is running!"
echo "  Server: $HOST:$PORT (PID: $SERVER_PID)"
echo "=========================================="
echo ""
echo "Press Ctrl+C to stop the server"
echo "(client will exit automatically when window closes)"
echo ""

# Wait for client to finish (display closes), then cleanup
wait $CLIENT_PID
echo "Client exited."
cleanup

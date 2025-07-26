#!/bin/bash
# Kryon Test Runner

set -e

echo "🧪 Running Kryon Test Suite"
echo "=========================="

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Run compiler tests
echo -e "\n${YELLOW}Running Compiler Tests...${NC}"
cargo test -p kryon-compiler --lib

# Run renderer tests  
echo -e "\n${YELLOW}Running Renderer Tests...${NC}"
cargo test -p kryon-core --lib
cargo test -p kryon-layout --lib
cargo test -p kryon-render --lib
cargo test -p kryon-runtime --lib

# Run integration tests
echo -e "\n${YELLOW}Running Integration Tests...${NC}"
cargo test -p kryon-tests

# Run snapshot tests
echo -e "\n${YELLOW}Running Snapshot Tests...${NC}"
cargo test -p kryon-ratatui
if [ $? -ne 0 ]; then
    echo -e "${RED}Snapshot tests failed. Run 'cargo insta review' to review changes.${NC}"
fi

# Run benchmarks (optional)
if [ "$1" == "--bench" ]; then
    echo -e "\n${YELLOW}Running Benchmarks...${NC}"
    cargo bench -p kryon-tests
fi

# Check for warnings
echo -e "\n${YELLOW}Checking for Warnings...${NC}"
cargo check --workspace 2>&1 | grep -i warning || echo -e "${GREEN}No warnings found!${NC}"

echo -e "\n${GREEN}✅ Test suite completed!${NC}"
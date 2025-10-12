#!/bin/bash

# Quick test script for HTML backend
set -e

echo "🚀 Testing Kryon HTML Backend..."
echo "=================================="

# Build Kryon CLI first
echo "📦 Building Kryon CLI..."
nimble build

# Build hello world for HTML
echo "🌐 Building Hello World for web..."
./bin/cli/kryon build --filename examples/hello_world.nim --renderer html

# Check if HTML file was generated
if [ -f "examples/hello_world_html/index.html" ]; then
    echo "✅ HTML generated successfully!"
    echo "📁 Location: examples/hello_world_html/index.html"
else
    echo "❌ HTML generation failed!"
    exit 1
fi

echo ""
echo "🌍 Starting local web server..."
echo "📱 Open http://localhost:8000 in your browser"
echo "⏹️  Press Ctrl+C to stop the server"
echo ""

# Change to HTML directory and start simple Python web server
cd examples/hello_world_html

# Try Python 3, then Python 2, then Python as fallback
if command -v python3 &> /dev/null; then
    echo "🐍 Using Python 3 server..."
    python3 -m http.server 8000
elif command -v python2 &> /dev/null; then
    echo "🐍 Using Python 2 server..."
    python2 -m SimpleHTTPServer 8000
elif command -v python &> /dev/null; then
    echo "🐍 Using Python server..."
    python -m http.server 8000 2>/dev/null || python -m SimpleHTTPServer 8000
else
    echo "❌ Python not found! Please install Python or use your own web server."
    echo "💡 You can open examples/hello_world_html/index.html directly in your browser."
    exit 1
fi
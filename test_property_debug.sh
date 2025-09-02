#!/bin/bash
# Quick test to check property types
env KRYON_RENDERER=raylib timeout 8s ./build/bin/kryon run examples/index.krb 2>&1 | tee /tmp/kryon_debug.log
echo "=== PROPERTY CREATION ==="
grep -E "(POST_CLONE.*to.*ptr)" /tmp/kryon_debug.log | head -3
echo "=== PROPERTY ACCESS ==="
grep -E "(NAV PROPERTY.*type=)" /tmp/kryon_debug.log | head -3
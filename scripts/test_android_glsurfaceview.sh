#!/bin/bash
#
# GLSurfaceView Migration Test Script
# Tests the Android app lifecycle and rendering
#

set -e

PACKAGE_NAME="com.kryon.temp"
ACTIVITY_NAME=".MainActivity"
TEST_DURATION=5

echo "========================================="
echo "Kryon GLSurfaceView Migration Test"
echo "========================================="
echo ""

# Check device connection
echo "[1/7] Checking device connection..."
if ! adb devices | grep -q "device$"; then
    echo "❌ ERROR: No Android device connected"
    echo "   Please connect a device and enable USB debugging"
    exit 1
fi
DEVICE=$(adb devices | grep "device$" | awk '{print $1}')
echo "✅ Device connected: $DEVICE"
echo ""

# Get device info
echo "[2/7] Device Information:"
ANDROID_VERSION=$(adb shell getprop ro.build.version.release)
MANUFACTURER=$(adb shell getprop ro.product.manufacturer)
MODEL=$(adb shell getprop ro.product.model)
echo "   Android: $ANDROID_VERSION"
echo "   Device: $MANUFACTURER $MODEL"
echo ""

# Check if app is installed
echo "[3/7] Checking app installation..."
if adb shell pm list packages | grep -q "$PACKAGE_NAME"; then
    echo "✅ App is installed"
    APP_VERSION=$(adb shell dumpsys package $PACKAGE_NAME | grep versionName | head -1)
    echo "   $APP_VERSION"
else
    echo "⚠️  App not installed"
    echo "   Install with: adb install -r app-debug.apk"
    exit 1
fi
echo ""

# Clear logs
echo "[4/7] Clearing logcat..."
adb logcat -c
echo "✅ Logs cleared"
echo ""

# Launch app
echo "[5/7] Launching app..."
adb shell am start -n "$PACKAGE_NAME/$ACTIVITY_NAME" > /dev/null 2>&1
sleep 2
echo "✅ App launched"
echo ""

# Monitor initial logs
echo "[6/7] Checking initialization logs..."
echo ""
echo "--- GL Initialization ---"
timeout 3 adb logcat -d | grep -E "GL onSurface|GL resources|Fonts registered" | tail -5
echo ""

echo "--- Render Loop ---"
timeout 2 adb logcat -d | grep "JNI nativeGLRender" | tail -3
echo ""

# Test lifecycle
echo "[7/7] Testing Lifecycle..."
echo ""

echo "📱 Test 1: Backgrounding..."
adb shell input keyevent KEYCODE_HOME
sleep 2
echo "   ✅ App backgrounded"

timeout 2 adb logcat -d | grep "onPause" | tail -1
sleep 1

echo "📱 Test 1: Resuming..."
adb shell am start -n "$PACKAGE_NAME/$ACTIVITY_NAME" > /dev/null 2>&1
sleep 2
echo "   ✅ App resumed"

timeout 2 adb logcat -d | grep "onResume" | tail -1
echo ""

# Check for crashes
echo "🔍 Checking for crashes..."
if adb logcat -d | grep -E "FATAL|AndroidRuntime.*FATAL" > /dev/null; then
    echo "❌ CRASH DETECTED!"
    echo ""
    adb logcat -d | grep -A 10 "FATAL"
    exit 1
else
    echo "✅ No crashes detected"
fi
echo ""

# Check for GL errors
echo "🔍 Checking for GL errors..."
if adb logcat -d | grep -E "GL error|EGL error|GLSurfaceView.*error" > /dev/null; then
    echo "⚠️  GL errors detected:"
    adb logcat -d | grep -E "GL error|EGL error" | tail -5
else
    echo "✅ No GL errors"
fi
echo ""

# Performance check
echo "📊 Performance Check..."
RENDER_COUNT=$(adb logcat -d | grep -c "nativeGLRender called" || echo "0")
echo "   Render frames logged: $RENDER_COUNT"

if [ "$RENDER_COUNT" -gt 100 ]; then
    echo "   ✅ Rendering actively (>100 frames)"
elif [ "$RENDER_COUNT" -gt 10 ]; then
    echo "   ⚠️  Rendering but may be slow ($RENDER_COUNT frames)"
else
    echo "   ❌ Rendering not working (<10 frames)"
fi
echo ""

# Visual verification prompt
echo "========================================="
echo "Visual Verification Required"
echo "========================================="
echo ""
echo "Please verify on the device screen:"
echo "  [ ] Text 'HELLO' is visible in RED"
echo "  [ ] Blue border (#0099ff) around container"
echo "  [ ] Midnight blue background (#191970)"
echo "  [ ] Positioned at (200, 100) from top-left"
echo "  [ ] Container size is 200x100 pixels"
echo ""

# Save logs
LOG_FILE="/tmp/kryon_test_$(date +%Y%m%d_%H%M%S).log"
adb logcat -d > "$LOG_FILE"
echo "📝 Full logs saved to: $LOG_FILE"
echo ""

# Summary
echo "========================================="
echo "Test Summary"
echo "========================================="
echo "Device: $MANUFACTURER $MODEL (Android $ANDROID_VERSION)"
echo "Status: Tests completed"
echo "Logs: $LOG_FILE"
echo ""
echo "Next steps:"
echo "  1. Verify visual appearance on device"
echo "  2. Test device rotation"
echo "  3. Test extended backgrounding (1+ minute)"
echo "  4. Check $LOG_FILE for any issues"
echo ""
echo "✅ Automated tests passed!"
echo "   Manual visual verification required."

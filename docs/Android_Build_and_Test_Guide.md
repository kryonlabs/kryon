# Android Build and Test Guide

## Quick Start: Building the Android App

### Prerequisites

```bash
export ANDROID_HOME=/home/wao/Android/Sdk
export ANDROID_NDK_HOME=/home/wao/Android/Sdk/ndk/29.0.14206865
export PATH=$PATH:$ANDROID_HOME/platform-tools
```

### Option 1: Using Gradle (Recommended)

If you have pre-built libraries:

```bash
cd bindings/kotlin
./gradlew assembleDebug

# Install to device
adb install -r build/outputs/apk/debug/app-debug.apk
```

### Option 2: Manual Build Steps

For a clean build from scratch:

1. **Build Platform Library**
```bash
cd platforms/android
mkdir -p build
cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21
make
```

2. **Build Renderer Library**
```bash
cd ../../../renderers/android
mkdir -p build
cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21
make
```

3. **Build Backend Library**
```bash
cd ../../../backends/android
mkdir -p build
cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21
make
```

4. **Build Full App**
```bash
cd ../../../bindings/kotlin
./gradlew assembleDebug
```

### Option 3: Using Test Script

A simplified test script is available:

```bash
# Create a simple test app
./scripts/build_android_test.sh
```

---

## Testing the GLSurfaceView Changes

### 1. Basic Deployment Test

```bash
# Connect Android device
adb devices

# Install app
adb install -r app-debug.apk

# Start app
adb shell am start -n com.kryon.temp/.MainActivity

# Monitor logs
adb logcat -c
adb logcat | grep -E "(Kryon|GL|AndroidRenderer)"
```

### 2. Visual Verification Checklist

Launch the app and verify:

- [ ] **Text Visible**: "HELLO" text appears in RED color (#ff0000)
- [ ] **Font Size**: Text is clearly 32.0f size (large)
- [ ] **Container**: Blue border (#0099ff) around midnight blue background (#191970)
- [ ] **Position**: Container positioned at (200, 100)
- [ ] **Size**: Container is 200x100 pixels

### 3. Lifecycle Test Scenarios

#### Test 1: App Backgrounding
```
1. Launch app
2. Verify text renders correctly
3. Press HOME button
4. Wait 5 seconds
5. Return to app (Recent Apps button)
✅ PASS: Text still visible, no black screen
❌ FAIL: Black screen or crash
```

#### Test 2: Device Rotation
```
1. Launch app
2. Verify text renders correctly
3. Rotate device 90 degrees
4. Wait 2 seconds
✅ PASS: Text re-renders at correct position
❌ FAIL: Crash or black screen
```

#### Test 3: Memory Pressure
```
1. Launch app
2. Press HOME button
3. Open several heavy apps (Chrome, Maps, etc.)
4. Wait 1 minute
5. Return to app
✅ PASS: Content restored (may call onSurfaceCreated)
❌ FAIL: Crash on restore
```

#### Test 4: Multi-Window Mode (Android 7+)
```
1. Launch app
2. Enter split-screen mode
3. Resize window
4. Exit split-screen
✅ PASS: Renders correctly in all modes
❌ FAIL: Layout issues or crash
```

### 4. Logcat Verification

Expected log output on successful startup:

```
D/Kryon: MainActivity.onKryonCreate()
I/KryonJNI: nativeInit called
I/KryonJNI: GL onSurfaceCreated
I/KryonJNI: IR Renderer created successfully
I/KryonJNI: GL resources initialized successfully (GL-only mode)
I/KryonJNI: Fonts registered with renderer
I/KryonJNI: GL onSurfaceChanged: 1080x2400
I/KryonJNI: JNI nativeGLRender called, count=60
I/KryonJNI: JNI nativeGLRender called, count=120
```

Expected log on pause/resume:

```
# On HOME button press:
I/KryonActivity: onPause
I/KryonJNI: nativeOnPause called

# On return to app:
I/KryonActivity: onResume
I/KryonJNI: nativeOnResume called
I/KryonJNI: JNI nativeGLRender called, count=180
```

### 5. Performance Monitoring

Check FPS and frame times:

```bash
# Monitor frame rate
adb shell dumpsys gfxinfo com.kryon.temp

# Expected: Consistent 60 FPS, <16ms frame time
```

### 6. Memory Leak Detection

```bash
# Before test
adb shell dumpsys meminfo com.kryon.temp | grep TOTAL

# After 10 pause/resume cycles
adb shell dumpsys meminfo com.kryon.temp | grep TOTAL

# Memory should not grow significantly
```

---

## Troubleshooting Common Issues

### Issue: Text Not Rendering

**Symptoms**: Black screen, no text visible

**Check**:
```bash
adb logcat | grep -E "GL|Shader|Font"
```

**Likely Causes**:
1. Font file not found: Check `/system/fonts/Roboto-Regular.ttf` exists
2. Shader compilation failed: Check for GL errors
3. VAO not bound: Check `glBindVertexArray` calls

**Fix**: Check logs for specific GL errors

### Issue: App Crashes on Pause/Resume

**Symptoms**: Crash when pressing HOME or returning

**Check**:
```bash
adb logcat | grep -E "FATAL|AndroidRuntime|SIGSEGV"
```

**Likely Causes**:
1. GL calls on invalid context
2. EGL context not released properly
3. Memory corruption

**Fix**: Verify `glSurfaceView.onPause()/onResume()` are called

### Issue: Build Fails with CMake Errors

**Symptoms**: "Cannot find source file" errors

**Check**: Verify file paths in CMakeLists.txt

**Fix**:
```bash
# Verify files exist
ls -la renderers/android/android_renderer.c
ls -la backends/android/ir_android_renderer.c

# Check CMake configuration
cd bindings/kotlin
cat src/main/cpp/CMakeLists.txt
```

### Issue: Device Not Detected

**Check**:
```bash
adb devices
# If empty, check USB debugging enabled
```

**Fix**:
1. Enable Developer Options on device
2. Enable USB Debugging
3. Accept RSA key prompt
4. Try different USB cable/port

---

## Advanced Testing

### OpenGL ES Debugging

Enable GL debug logging:

```bash
adb shell setprop debug.egl.trace error
adb logcat | grep -E "EGL|GLES"
```

### Render Doc Integration

For detailed frame analysis:

```bash
# Install RenderDoc Android app
# Enable GPU debugging in Developer Options
# Capture frames during rendering
```

### Profiling with Android Studio

```bash
# Open Android Studio
# Tools > Profiler
# Select app process
# Monitor CPU, Memory, GPU
```

---

## Success Criteria Checklist

Before marking the migration as complete, verify ALL of the following:

- [ ] Text renders correctly with Roboto font
- [ ] Red color (#ff0000) is visible
- [ ] Container has blue border (#0099ff)
- [ ] Background is midnight blue (#191970)
- [ ] Position is (200, 100) from top-left
- [ ] Size is exactly 200x100 pixels
- [ ] App survives HOME button press
- [ ] Content persists after backgrounding
- [ ] No crashes on device rotation
- [ ] No crashes after 10+ pause/resume cycles
- [ ] Memory usage remains stable
- [ ] Consistent 60 FPS rendering
- [ ] No GL errors in logcat
- [ ] Works on multiple devices (if available)

---

## Reporting Issues

If tests fail, collect the following information:

```bash
# Full logcat during failure
adb logcat -d > /tmp/kryon_failure.log

# Device information
adb shell getprop ro.build.version.release
adb shell getprop ro.product.manufacturer
adb shell getprop ro.product.model

# GL capabilities
adb shell dumpsys SurfaceFlinger | grep GLES

# App state
adb shell dumpsys activity com.kryon.temp
```

Include:
1. Device model and Android version
2. Exact steps to reproduce
3. Full logcat output
4. Screenshots/screen recording if applicable

---

## Next Steps After Successful Testing

Once all tests pass:

1. **Document Results**: Update this file with test results
2. **Performance Baseline**: Record FPS and memory usage
3. **Clean Up**: Remove `android_egl.c` (no longer needed)
4. **Update CLAUDE.md**: Document GLSurfaceView approach
5. **Create Examples**: Add more complex test cases

---

**Last Updated**: December 28, 2025
**GLSurfaceView Migration**: Phase 1 Complete, Phase 2 Testing Pending

# Plan 06: Android Platform Support

## Executive Summary

Implement complete native Android support for Kryon, adding Android as a first-class platform target alongside Linux, Windows, macOS, STM32, and RP2040. This enables Kryon applications to run on Android devices with full access to platform capabilities (touch input, lifecycle management, storage, permissions) while maintaining the KIR-centric architecture.

## Architecture Overview

### Platform Integration Model

```
┌─────────────────────────────────────────────────────────────┐
│                    Kryon Application                         │
│                  (.kry, .tsx, .md, .c)                      │
└─────────────────────────────────────────────────────────────┘
                            ↓
                    ┌──────────────┐
                    │  KIR     │
                    └──────────────┘
                            ↓
                ┌───────────┴───────────┐
                ↓                       ↓
          ┌──────────┐            ┌──────────┐
          │    VM    │            │  NATIVE  │
          │  (.krb)  │            │  (AOT)   │
          └──────────┘            └──────────┘
                ↓                       ↓
                └───────────┬───────────┘
                            ↓
┌───────────────────────────────────────────────────────────┐
│            ANDROID PLATFORM LAYER                          │
│                                                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │  Lifecycle   │  │   Storage    │  │    Input     │   │
│  │  (Activity)  │  │ (SharedPref) │  │   (Touch)    │   │
│  └──────────────┘  └──────────────┘  └──────────────┘   │
│                                                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │ Permissions  │  │   Display    │  │   System     │   │
│  │  (Runtime)   │  │  (Window)    │  │  (Vibrate)   │   │
│  └──────────────┘  └──────────────┘  └──────────────┘   │
└───────────────────────────────────────────────────────────┘
                            ↓
┌───────────────────────────────────────────────────────────┐
│            ANDROID RENDERER (OpenGL ES 3.0)                │
│                                                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │   Layout     │  │     Text     │  │    Image     │   │
│  │  (Flexbox)   │  │  (FreeType)  │  │  (Texture)   │   │
│  └──────────────┘  └──────────────┘  └──────────────┘   │
│                                                            │
│  ┌──────────────────────────────────────────────────┐    │
│  │        EGL + OpenGL ES 3.0 Backend               │    │
│  └──────────────────────────────────────────────────┘    │
└───────────────────────────────────────────────────────────┘
                            ↓
                    Android OS (API 24+)
```

### Dual Execution Modes

**VM Mode (Bytecode):**
```
KIR → .krb → Android VM Runtime → OpenGL ES Renderer
```
- Single .krb file bundled in APK assets
- Platform-independent execution
- Hot reload support via ADB
- Ideal for development and prototyping

**Native Mode (AOT Compilation):**
```
KIR → C Source → Android NDK → ARM64/ARMv7 Binary → APK
```
- Ahead-of-time compiled native code
- Maximum performance
- Static linking (no external dependencies)
- Production deployment

## Current State

### Existing Platform Infrastructure

**Platform Abstraction Pattern Established:**
- **Linux:** `platforms/linux/linux_platform.h`
- **STM32:** `platforms/stm32/stm32_platform.h`
- **RP2040:** `platforms/rp2040/rp2040_platform.h`

Each platform implements:
```c
bool platform_init(void);
void platform_shutdown(void);
bool platform_storage_set/get/has(const char* key, ...);
uint32_t platform_get_timestamp(void);
void platform_get_display_dimensions(uint16_t* w, uint16_t* h);
```

**Renderer Abstraction:**
- **SDL3 Desktop:** `backends/desktop/ir_desktop_renderer.h`
- **Terminal:** `renderers/terminal/`
- **Framebuffer:** `renderers/framebuffer/`

**Bindings Ecosystem:**
- C, Nim, Lua, TypeScript, Rust, JavaScript bindings exist
- Pattern for new bindings well-established

**Build System:**
- Makefile-based with platform-specific targets
- Cross-compilation support for ARM (STM32, RP2040)
- Static library generation (`libkryon_core.a`, `libkryon_renderer.a`)

### What's Missing for Android

1. **No Android platform abstraction layer**
   - Missing: `platforms/android/android_platform.h/c`
   - Missing: Android-specific lifecycle, storage, input, permissions

2. **No Android renderer backend**
   - SDL3 doesn't provide robust Android support
   - Need OpenGL ES 3.0 renderer (not OpenGL 3.x)
   - Need EGL context management

3. **No JNI/Kotlin bindings**
   - Missing: `bindings/kotlin/` for Android integration
   - Missing: JNI bridge to C core

4. **No Android NDK build integration**
   - Missing: CMakeLists.txt for Android
   - Missing: Gradle integration
   - Missing: APK packaging support

5. **No mobile-specific features**
   - Touch input (single, multi-touch, gestures)
   - Soft keyboard (IME) integration
   - Activity lifecycle management
   - Runtime permissions

6. **No CLI support for Android**
   - Missing: `kryon build --target=android`
   - Missing: `kryon run android app.krb`
   - Missing: `kryon package android app.kry -o app.apk`

## Complete Android Platform Implementation

### 1. Platform Abstraction Layer

**Location:** `platforms/android/`

```
platforms/android/
├── android_platform.h           # Public platform API
├── android_platform.c           # Core implementation
├── android_lifecycle.c          # Activity lifecycle
├── android_storage.c            # Storage implementation
├── android_input.c              # Touch + IME input
├── android_permissions.c        # Runtime permissions
├── android_jni_bridge.c         # JNI helper functions
└── ndk/
    ├── CMakeLists.txt
    ├── build_android.sh
    └── Application.mk
```

**Platform API (android_platform.h):**

```c
#ifndef KRYON_ANDROID_PLATFORM_H
#define KRYON_ANDROID_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <jni.h>
#include <android/native_activity.h>

// ============================================================================
// Lifecycle Management
// ============================================================================

typedef enum {
    KRYON_ANDROID_STATE_CREATED,
    KRYON_ANDROID_STATE_STARTED,
    KRYON_ANDROID_STATE_RESUMED,
    KRYON_ANDROID_STATE_PAUSED,
    KRYON_ANDROID_STATE_STOPPED,
    KRYON_ANDROID_STATE_DESTROYED
} kryon_android_lifecycle_state_t;

// Initialize platform with ANativeActivity
bool kryon_android_platform_init(ANativeActivity* activity);
void kryon_android_platform_shutdown(void);

// Lifecycle callbacks (called by Java Activity)
void kryon_android_on_create(void);
void kryon_android_on_start(void);
void kryon_android_on_resume(void);
void kryon_android_on_pause(void);
void kryon_android_on_stop(void);
void kryon_android_on_destroy(void);

kryon_android_lifecycle_state_t kryon_android_get_lifecycle_state(void);

// ============================================================================
// Storage (SharedPreferences + Internal Storage)
// ============================================================================

// Key-value storage (via SharedPreferences JNI)
bool kryon_android_storage_set(const char* key, const void* data, size_t size);
bool kryon_android_storage_get(const char* key, void* buffer, size_t* size);
bool kryon_android_storage_has(const char* key);
bool kryon_android_storage_remove(const char* key);

// File storage (Internal storage: /data/data/<package>/files/)
bool kryon_android_file_write(const char* filename, const void* data, size_t size);
bool kryon_android_file_read(const char* filename, void** data, size_t* size);
bool kryon_android_file_exists(const char* filename);
bool kryon_android_file_delete(const char* filename);

const char* kryon_android_get_internal_storage_path(void);
const char* kryon_android_get_cache_path(void);

// ============================================================================
// Input Handling (Touch + Multi-touch + IME)
// ============================================================================

typedef enum {
    KRYON_ANDROID_TOUCH_DOWN,
    KRYON_ANDROID_TOUCH_MOVE,
    KRYON_ANDROID_TOUCH_UP,
    KRYON_ANDROID_TOUCH_CANCEL
} kryon_android_touch_action_t;

typedef struct {
    int32_t pointer_id;
    float x, y;
    float pressure;
    kryon_android_touch_action_t action;
    uint64_t timestamp;
} kryon_android_touch_event_t;

// Process Android MotionEvents → Kryon events
bool kryon_android_handle_input_events(kryon_event_t* events, size_t* count);

// Soft keyboard (IME) control
void kryon_android_show_keyboard(void);
void kryon_android_hide_keyboard(void);
bool kryon_android_is_keyboard_visible(void);

// ============================================================================
// Display & Window Management
// ============================================================================

typedef struct {
    int32_t width;
    int32_t height;
    int32_t density_dpi;
    float density_scale;
    int32_t orientation;  // 0=portrait, 1=landscape, 2=reverse_portrait, 3=reverse_landscape
} kryon_android_display_info_t;

kryon_android_display_info_t kryon_android_get_display_info(void);
ANativeWindow* kryon_android_get_native_window(void);

// Orientation control
void kryon_android_set_orientation(int orientation);

// ============================================================================
// Permissions Management
// ============================================================================

typedef enum {
    KRYON_ANDROID_PERM_STORAGE,
    KRYON_ANDROID_PERM_CAMERA,
    KRYON_ANDROID_PERM_LOCATION,
    KRYON_ANDROID_PERM_NETWORK,
    KRYON_ANDROID_PERM_MICROPHONE
} kryon_android_permission_t;

bool kryon_android_request_permission(kryon_android_permission_t permission);
bool kryon_android_has_permission(kryon_android_permission_t permission);

// ============================================================================
// System Utilities
// ============================================================================

uint64_t kryon_android_get_timestamp(void);
void kryon_android_vibrate(uint32_t duration_ms);
const char* kryon_android_get_package_name(void);
const char* kryon_android_get_app_data_dir(void);
const char* kryon_android_get_device_model(void);
const char* kryon_android_get_android_version(void);
int kryon_android_get_api_level(void);

// Open URL in browser
bool kryon_android_open_url(const char* url);

// JNI Environment access (for advanced use)
JNIEnv* kryon_android_get_jni_env(void);
jobject kryon_android_get_activity_object(void);

// ============================================================================
// Platform Capabilities
// ============================================================================

typedef struct {
    bool has_opengl_es3;
    bool has_vulkan;
    bool has_multitouch;
    bool has_accelerometer;
    bool has_gyroscope;
    bool has_gps;
    int max_touch_points;
    const char* device_model;
    const char* android_version;
    int api_level;
} kryon_android_capabilities_t;

kryon_android_capabilities_t kryon_android_get_capabilities(void);
void kryon_android_print_info(void);

#endif // KRYON_ANDROID_PLATFORM_H
```

**Key Implementation Notes:**

1. **Lifecycle Management:**
   - Save component tree state on `onPause()` (app backgrounded)
   - Release renderer resources but preserve component tree
   - Restore renderer surface on `onResume()`
   - Full cleanup on `onDestroy()`

2. **Storage Implementation:**
   - Use JNI to call `SharedPreferences.Editor.putString()` for key-value
   - Use POSIX file APIs for internal storage (`/data/data/<package>/files/`)
   - Maintain same API as Linux platform for consistency

3. **Touch Input:**
   - Convert Android `MotionEvent` to Kryon events
   - Support up to 10 simultaneous touch points
   - Map touch coordinates to component tree (hit testing)
   - Handle ACTION_DOWN, ACTION_MOVE, ACTION_UP, ACTION_CANCEL

4. **Permissions:**
   - JNI calls to `ActivityCompat.requestPermissions()`
   - Callback mechanism for permission results
   - Graceful degradation when permissions denied

### 2. Android Renderer (OpenGL ES 3.0)

**Location:** `renderers/android/`

```
renderers/android/
├── android_renderer.h           # Public renderer API
├── android_renderer.c           # Core renderer implementation
├── android_gles3_backend.c      # OpenGL ES 3.0 backend
├── android_text_rendering.c     # FreeType text rendering
├── android_image_loader.c       # PNG/JPEG decoding
├── android_shaders.h            # GLSL ES shader sources
└── README.md
```

**Why OpenGL ES 3.0:**
- Available on 95%+ of Android devices (API 24+)
- Mature, well-documented, stable
- Compatible with existing rendering patterns from desktop
- Easy to port from SDL3 renderer
- Future: Add Vulkan backend for high-end devices

**Rendering Features:**

1. **Core Rendering:**
   - Rectangle rendering (solid, gradient, rounded corners)
   - Flexbox layout computation
   - Shadow and blur effects (via multi-pass rendering)
   - Clipping and masking

2. **Text Rendering:**
   - FreeType integration for font rasterization
   - HarfBuzz integration for text shaping
   - Glyph texture atlas with dynamic packing
   - Multi-line text with word wrapping
   - BiDi support via FriBidi

3. **Image Rendering:**
   - PNG/JPEG decoding
   - Texture caching (LRU eviction)
   - Mipmapping for scaled images
   - Texture compression (ETC2/ASTC on supported devices)

4. **Performance Optimizations:**
   - Batch rendering (minimize draw calls)
   - Dirty rectangle tracking (partial redraws)
   - VSync synchronization (60fps target)
   - Texture atlas for small images and glyphs
   - GPU-accelerated effects (shadows, blurs)

### 3. Kotlin/JNI Bindings

**Location:** `bindings/kotlin/`

```
bindings/kotlin/
├── src/main/kotlin/com/kryon/
│   ├── KryonActivity.kt         # Base activity
│   ├── KryonView.kt             # Custom view
│   ├── KryonRuntime.kt          # Runtime management
│   ├── KryonLifecycleObserver.kt
│   └── KryonPermissionManager.kt
├── src/main/java/com/kryon/
│   └── KryonNativeLib.java      # JNI declarations
├── src/main/cpp/
│   ├── kryon_jni.c              # JNI implementation
│   └── CMakeLists.txt
├── build.gradle.kts
└── README.md
```

### 4. Build System Integration

**Android NDK CMakeLists.txt:** `platforms/android/ndk/CMakeLists.txt`

**Gradle Build:** `platforms/android/gradle/build.gradle.kts`

**Makefile Targets:** (add to root `Makefile`)
```makefile
build-android:    # Build native libraries for Android
package-android:  # Package APK
install-android:  # Install to device via ADB
clean-android:    # Clean Android build artifacts
```

### 5. CLI Integration

**New Commands:**
```bash
kryon build app.kry --target=android-vm -o app.apk
kryon build app.kry --target=android-native -o app.apk --arch=arm64-v8a
kryon run android app.krb
kryon package android app.kry -o myapp.apk --app-name "My App"
kryon deploy android myapp.apk
kryon dev android app.kry  # Hot reload
```

## Implementation Phases

### Phase 1: Foundation (Weeks 1-2)
**Goal:** Basic Android platform + minimal renderer

**Deliverable:** APK that displays a solid color screen

### Phase 2: Input & Rendering (Weeks 3-4)
**Goal:** Interactive UI with touch input

**Deliverable:** APK running counter.kry with working buttons

### Phase 3: VM Target (Weeks 5-6)
**Goal:** .krb bytecode execution on Android

**Deliverable:** Deploy and run .krb files on Android

### Phase 4: Native Compilation (Weeks 7-8)
**Goal:** AOT compilation to native Android binary

**Deliverable:** Standalone APK compiled from .kry source

### Phase 5: Platform Features (Weeks 9-10)
**Goal:** Android-specific integrations

**Deliverable:** Full-featured Android platform (permissions, storage, IME, etc.)

### Phase 6: Optimization & Documentation (Weeks 11-12)
**Goal:** Production-ready Android support

**Deliverable:** Production-ready platform with comprehensive docs

## Dependencies

### Required Android Components
- Android NDK 26.1+ (LTS)
- Android SDK Platform API 34
- Build Tools 34.0.0
- CMake 3.22.1+
- Gradle 8.2+
- Java JDK 17+

### Third-Party Libraries (Static Linking)
- FreeType 2.13+ (text rendering)
- HarfBuzz 8.0+ (text shaping)
- FriBidi (BiDi text)
- libpng 1.6+ (PNG decoding)
- libjpeg-turbo 3.0+ (JPEG decoding)
- zlib 1.3+ (compression)
- cJSON (JSON parsing)

### Android System Libraries (Dynamic Linking)
- libandroid.so - Native activity APIs
- liblog.so - Android logging
- libEGL.so - EGL context
- libGLESv3.so - OpenGL ES 3.0
- libz.so - System zlib

## Testing Strategy

### Unit Tests
Location: `tests/android/`
- Platform initialization
- Storage operations
- Touch event conversion
- Lifecycle transitions
- Renderer initialization

### Integration Tests
- Test on Pixel, Samsung, OnePlus devices
- Test API levels 24, 28, 31, 34
- Test rotation, backgrounding, low memory
- Test different screen densities

### UI Tests
- Port all examples to Android
- Verify rendering matches desktop
- Test touch input accuracy
- Measure performance (60fps target)

## Success Criteria

✅ **Build System:**
- `make build-android` builds for arm64-v8a and armeabi-v7a
- `make package-android` creates installable APK
- Build time <2 minutes for incremental builds

✅ **Functionality:**
- All core UI components render correctly
- Touch input works (single and multi-touch)
- .krb bytecode runs on Android
- Native compilation produces working APK

✅ **Performance:**
- Sustained 60fps rendering
- Memory usage <100MB for simple apps
- Cold start time <1 second
- Hot reload <500ms

✅ **Platform Integration:**
- Lifecycle management works correctly
- Storage persists across restarts
- Permissions can be requested
- All examples run without modification

## Integration with Existing Architecture

Android support integrates seamlessly into Kryon's KIR-centric architecture:

```
Source Files (.kry, .tsx, .md, .c)
        ↓
    Parsers
        ↓
    KIR JSON ← Universal IR (no changes needed)
        ↓
   ┌────┴────┐
   ↓         ↓
 .krb     C Source
   ↓         ↓
Android   Android
  VM       NDK
   ↓         ↓
   └────┬────┘
        ↓
 Android Platform Layer (NEW)
        ↓
 Android Renderer (NEW)
        ↓
   Android OS
```

**No Changes Required:**
- KIR format remains unchanged
- Existing parsers work as-is
- Existing codegens work as-is
- VM bytecode format unchanged

**New Additions:**
- Android platform abstraction layer
- Android OpenGL ES 3.0 renderer
- Kotlin/JNI bindings
- Android NDK build configuration
- APK packaging CLI commands

**Result:** Android becomes the 4th major platform (Desktop, Embedded, Web, **Mobile**)

---

*This plan follows Kryon's established patterns and integrates Android as a first-class platform target while preserving the universal KIR-centric architecture.*

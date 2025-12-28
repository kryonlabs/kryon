# Kryon Android Platform

Native Android platform support for the Kryon UI framework, enabling Kryon applications to run on Android devices with full access to platform capabilities.

## Overview

The Android platform layer provides:
- **Lifecycle Management**: Activity lifecycle integration (onCreate, onResume, onPause, etc.)
- **Storage**: Key-value storage (SharedPreferences-like) and file I/O
- **Input Handling**: Touch events (single and multi-touch), keyboard, IME
- **Display Management**: Window management, orientation, display metrics
- **System Integration**: Permissions, vibration, URL opening, device info

## Architecture

```
Kryon Application
      ↓
   KIR (IR)
      ↓
Platform Abstraction (android_platform.h)
      ↓
   ┌─────────┬──────────┬─────────┐
   ↓         ↓          ↓         ↓
Lifecycle Storage   Input    Display
   ↓         ↓          ↓         ↓
Android NDK APIs (JNI)
```

## Building

### Prerequisites

1. **Android NDK** (r26 or later)
   ```bash
   # Download from https://developer.android.com/ndk/downloads
   export ANDROID_NDK_HOME=/path/to/ndk
   ```

2. **CMake** (3.22.1 or later)
   ```bash
   cmake --version  # Should be 3.22.1+
   ```

3. **Build Tools**
   - GNU Make
   - GCC/Clang (provided by NDK)

### Build Commands

**Build all ABIs (arm64-v8a, armeabi-v7a, x86_64, x86):**
```bash
cd platforms/android
make ndk-build
```

**Build single ABI (arm64-v8a only):**
```bash
make ndk-build-single
```

**Build for specific API level:**
```bash
make API_LEVEL=28 ndk-build
```

**Debug build:**
```bash
make BUILD_TYPE=Debug ndk-build
```

**Clean:**
```bash
make clean
```

### Build Output

Libraries are generated in `build/<ABI>/`:
```
build/
├── arm64-v8a/
│   └── libkryon_android_platform.a
├── armeabi-v7a/
│   └── libkryon_android_platform.a
├── x86_64/
│   └── libkryon_android_platform.a
└── x86/
    └── libkryon_android_platform.a
```

## API Reference

### Initialization

```c
#include "android_platform.h"

// Initialize platform (call from ANativeActivity_onCreate)
bool kryon_android_platform_init(void* activity);

// Shutdown platform
void kryon_android_platform_shutdown(void);
```

### Lifecycle Management

```c
// Lifecycle callbacks (call from Activity lifecycle methods)
void kryon_android_on_create(void);
void kryon_android_on_start(void);
void kryon_android_on_resume(void);
void kryon_android_on_pause(void);
void kryon_android_on_stop(void);
void kryon_android_on_destroy(void);

// Get current lifecycle state
kryon_android_lifecycle_state_t kryon_android_get_lifecycle_state(void);
```

### Storage

**Key-Value Storage:**
```c
// Initialize storage
bool kryon_android_init_storage(void);

// Store value
const char* data = "my_value";
kryon_android_storage_set("my_key", data, strlen(data) + 1);

// Retrieve value
char buffer[256];
size_t size = sizeof(buffer);
if (kryon_android_storage_get("my_key", buffer, &size)) {
    printf("Value: %s\n", buffer);
}

// Check existence
if (kryon_android_storage_has("my_key")) {
    printf("Key exists\n");
}

// Remove entry
kryon_android_storage_remove("my_key");
```

**File Storage:**
```c
// Write file
const char* content = "file content";
kryon_android_file_write("myfile.txt", content, strlen(content));

// Read file
void* data = NULL;
size_t size = 0;
if (kryon_android_file_read("myfile.txt", &data, &size)) {
    printf("File size: %zu\n", size);
    free(data);
}

// Check existence
if (kryon_android_file_exists("myfile.txt")) {
    printf("File exists\n");
}

// Delete file
kryon_android_file_delete("myfile.txt");
```

### Input Handling

**Touch Events:**
```c
kryon_android_event_t events[16];
size_t event_count = 0;

// Poll events
kryon_android_poll_events(events, &event_count, 16);

for (size_t i = 0; i < event_count; i++) {
    if (events[i].type == KRYON_ANDROID_EVENT_TOUCH) {
        kryon_android_touch_event_t* touch = &events[i].data.touch;
        printf("Touch: id=%d, pos=(%.1f, %.1f), action=%d\n",
               touch->pointer_id, touch->x, touch->y, touch->action);
    }
}
```

**Soft Keyboard:**
```c
// Show keyboard
kryon_android_show_keyboard();

// Hide keyboard
kryon_android_hide_keyboard();

// Check visibility
if (kryon_android_is_keyboard_visible()) {
    printf("Keyboard is visible\n");
}
```

### Display Management

```c
// Get display info
kryon_android_display_info_t info = kryon_android_get_display_info();
printf("Display: %dx%d @ %d DPI (scale: %.2f)\n",
       info.width, info.height, info.density_dpi, info.density_scale);

// Get dimensions
uint16_t width, height;
kryon_android_get_display_dimensions(&width, &height);

// Get native window (for OpenGL)
ANativeWindow* window = kryon_android_get_native_window();

// Set orientation
kryon_android_set_orientation(KRYON_ANDROID_ORIENTATION_LANDSCAPE);
```

### System Utilities

```c
// Timing
uint64_t timestamp = kryon_android_get_timestamp();  // Milliseconds
kryon_android_delay_ms(100);

// Haptic feedback
kryon_android_vibrate(50);  // 50ms vibration

// Device info
printf("Device: %s\n", kryon_android_get_device_model());
printf("Android: %s (API %d)\n",
       kryon_android_get_android_version(),
       kryon_android_get_api_level());

// Storage paths
printf("Internal: %s\n", kryon_android_get_internal_storage_path());
printf("Cache: %s\n", kryon_android_get_cache_path());
printf("External: %s\n", kryon_android_get_external_storage_path());

// Open URL
kryon_android_open_url("https://example.com");
```

### Platform Capabilities

```c
kryon_android_capabilities_t caps = kryon_android_get_capabilities();

printf("OpenGL ES 3.0: %s\n", caps.has_opengl_es3 ? "Yes" : "No");
printf("Vulkan: %s\n", caps.has_vulkan ? "Yes" : "No");
printf("Multi-touch: %s (%d points)\n",
       caps.has_multitouch ? "Yes" : "No", caps.max_touch_points);
printf("Sensors: accelerometer=%s, gyroscope=%s, GPS=%s\n",
       caps.has_accelerometer ? "Yes" : "No",
       caps.has_gyroscope ? "Yes" : "No",
       caps.has_gps ? "Yes" : "No");

// Print full info
kryon_android_print_info();
```

### Error Handling

```c
kryon_android_error_t error = kryon_android_get_last_error();
if (error != KRYON_ANDROID_ERROR_NONE) {
    printf("Error: %s\n", kryon_android_get_error_string(error));
}
```

## Integration with NativeActivity

### Minimal Example

```c
#include <android/native_activity.h>
#include "android_platform.h"

void ANativeActivity_onCreate(ANativeActivity* activity,
                               void* savedState, size_t savedStateSize) {
    // Initialize Kryon platform
    if (!kryon_android_platform_init(activity)) {
        return;
    }

    // Set lifecycle callbacks
    activity->callbacks->onStart = on_start;
    activity->callbacks->onResume = on_resume;
    activity->callbacks->onPause = on_pause;
    activity->callbacks->onStop = on_stop;
    activity->callbacks->onDestroy = on_destroy;
    activity->callbacks->onInputEvent = on_input_event;

    // Initialize renderer, load UI, etc.
    // ...
}

static void on_start(ANativeActivity* activity) {
    kryon_android_on_start();
}

static void on_resume(ANativeActivity* activity) {
    kryon_android_on_resume();
}

static void on_pause(ANativeActivity* activity) {
    kryon_android_on_pause();
}

static void on_stop(ANativeActivity* activity) {
    kryon_android_on_stop();
}

static void on_destroy(ANativeActivity* activity) {
    kryon_android_on_destroy();
    kryon_android_platform_shutdown();
}

static int32_t on_input_event(ANativeActivity* activity, AInputEvent* event) {
    return kryon_android_process_input_event(event) ? 1 : 0;
}
```

## Storage Limits

| Parameter | Limit |
|-----------|-------|
| Max entries | 128 |
| Max key length | 128 bytes |
| Max value size | 4096 bytes |
| Max file size | Unlimited (limited by device storage) |

## API Compatibility

- **Minimum API Level**: 24 (Android 7.0 Nougat)
- **Target API Level**: 34 (Android 14)
- **Supported ABIs**: arm64-v8a, armeabi-v7a, x86_64, x86

## Platform Features

| Feature | Status | Notes |
|---------|--------|-------|
| Lifecycle management | ✅ Complete | Full activity lifecycle support |
| Key-value storage | ✅ Complete | File-based with checksums |
| File storage | ✅ Complete | Internal/external storage |
| Touch input | ✅ Complete | Single and multi-touch |
| Keyboard input | ✅ Complete | Hardware and soft keyboard |
| IME (soft keyboard) | ✅ Complete | Show/hide via JNI |
| Display info | ✅ Complete | Dimensions, DPI, orientation |
| Permissions | 🚧 Stub | TODO: JNI implementation |
| Vibration | 🚧 Stub | TODO: JNI implementation |
| URL opening | 🚧 Stub | TODO: Intent via JNI |
| Network status | 🚧 Stub | TODO: ConnectivityManager |

## TODO

- [ ] Complete JNI implementations for:
  - [ ] Runtime permissions (ActivityCompat)
  - [ ] Vibration (Vibrator service)
  - [ ] URL opening (Intent.ACTION_VIEW)
  - [ ] Share intent (Intent.ACTION_SEND)
  - [ ] Network status (ConnectivityManager)
- [ ] Add asset manager integration
- [ ] Add sensors support (accelerometer, gyroscope)
- [ ] Add battery status monitoring
- [ ] Add locale/language detection

## See Also

- [Plan 06: Android Platform Support](../../docs/architecture/plans/06-android-platform-support.md)
- [Android NDK Documentation](https://developer.android.com/ndk)
- [Kryon Platform Abstraction](../README.md)

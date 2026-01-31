# Kryon Kotlin/Android Bindings

This module provides Kotlin/Android bindings for the Kryon UI framework, including:
- JNI bridge to native Kryon libraries
- `KryonActivity` base class for Android apps
- CMake integration for building native code
- OpenGL ES 3.0 renderer integration

## Building as Part of Multi-Module Project (CLI Usage)

When included in a multi-module Gradle project (like the Kryon CLI temp projects), the parent project must declare plugin versions:

```kotlin
// Parent build.gradle.kts
plugins {
    id("com.android.application") version "8.2.0" apply false
    id("com.android.library") version "8.2.0" apply false
    id("org.jetbrains.kotlin.android") version "1.9.20" apply false
}
```

Then include this module:

```kotlin
// settings.gradle.kts
include(":kryon")
project(":kryon").projectDir = file("/path/to/kryon/bindings/kotlin")
```

## Building Standalone

To build this module standalone for testing or distribution:

```bash
# Use the standalone build configuration
gradle -b build.gradle.root.kts -c settings.gradle.root.kts build
```

Or create a wrapper project with the necessary plugin declarations.

## Requirements

- Android NDK 26.1+
- CMake 3.22.1+
- Kotlin 1.9.20+
- Android Gradle Plugin 8.2.0+

## Native Dependencies

This module depends on pre-built native libraries:
- `android/build/{ABI}/libkryon_android_platform.a`
- `renderers/android/build/{ABI}/libkryon_android_renderer.a`

Build these first:
```bash
cd ../../android && make ndk-build
cd ../../renderers/android && make ndk-build
```

## Integration

See the Kryon CLI for automated integration:
```bash
./cli/kryon run --target=android examples/kry/your_app.kry
```

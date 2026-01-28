# Kryon Platform Services Plugin Development Guide

This guide explains how to create platform services plugins for standalone kryon.

## Overview

The kryon platform services plugin system allows adding OS-specific features (file I/O, namespace operations, process control, etc.) without breaking portability. Apps detect and use services when available, falling back gracefully when not.

## Architecture

```
Kryon App (portable)
      ↓
Kryon Runtime
      ↓
Services Registry
      ↓
  ┌───────┴───────┐
  ↓               ↓
Standard      Inferno Plugin
Services      (lib9, 9P, /prog)
```

**Key Principles:**
- **Non-invasive**: Plugins are additive, don't modify core runtime
- **Opt-in**: Apps explicitly request services
- **Compile-time**: Plugins compiled in based on build target
- **Zero overhead**: No penalty when not used
- **Graceful fallback**: Missing services don't crash

## Creating a Plugin

### Step 1: Define Service Interfaces

Create header files in `include/services/` defining your service interface:

```c
// include/services/my_service.h
#ifndef KRYON_SERVICE_MY_SERVICE_H
#define KRYON_SERVICE_MY_SERVICE_H

typedef struct {
    // Service operations
    bool (*do_something)(const char *param);
    void (*cleanup)(void);
} KryonMyService;

#endif
```

### Step 2: Register Service Type

Add your service type to `include/platform_services.h`:

```c
typedef enum {
    // ... existing services ...
    KRYON_SERVICE_MY_SERVICE = 5,
    KRYON_SERVICE_COUNT
} KryonServiceType;
```

Update `kryon_service_type_name()` in `src/services/services_registry.c`:

```c
const char* kryon_service_type_name(KryonServiceType service) {
    switch (service) {
        // ... existing cases ...
        case KRYON_SERVICE_MY_SERVICE:
            return "My Service";
        default:
            return "Unknown";
    }
}
```

### Step 3: Create Plugin Directory

Create `src/plugins/myplugin/` with these files:

#### `plugin.c` - Main Registration

```c
#ifdef KRYON_PLUGIN_MYPLUGIN

#include "platform_services.h"
#include "services/my_service.h"

extern KryonMyService* myplugin_get_my_service(void);

static bool myplugin_init(void *config) {
    // Initialize plugin
    return true;
}

static void myplugin_shutdown(void) {
    // Cleanup
}

static bool myplugin_has_service(KryonServiceType service) {
    switch (service) {
        case KRYON_SERVICE_MY_SERVICE:
            return true;
        default:
            return false;
    }
}

static void* myplugin_get_service(KryonServiceType service) {
    switch (service) {
        case KRYON_SERVICE_MY_SERVICE:
            return (void*)myplugin_get_my_service();
        default:
            return NULL;
    }
}

static KryonPlatformServices myplugin_plugin = {
    .name = "myplugin",
    .version = "1.0.0",
    .init = myplugin_init,
    .shutdown = myplugin_shutdown,
    .has_service = myplugin_has_service,
    .get_service = myplugin_get_service,
};

__attribute__((constructor))
static void register_myplugin_plugin(void) {
    kryon_services_register(&myplugin_plugin);
}

#endif
```

#### `my_service.c` - Service Implementation

```c
#ifdef KRYON_PLUGIN_MYPLUGIN

#include "services/my_service.h"
#include <stdio.h>

static bool myplugin_do_something(const char *param) {
    printf("Doing something with: %s\n", param);
    return true;
}

static void myplugin_cleanup(void) {
    printf("Cleaning up\n");
}

static KryonMyService myplugin_my_service = {
    .do_something = myplugin_do_something,
    .cleanup = myplugin_cleanup,
};

KryonMyService* myplugin_get_my_service(void) {
    return &myplugin_my_service;
}

#endif
```

#### `Makefile` - Build Configuration

```makefile
# Plugin sources
MYPLUGIN_PLUGIN_SRC = \
	src/plugins/myplugin/plugin.c \
	src/plugins/myplugin/my_service.c

# Plugin objects
MYPLUGIN_PLUGIN_OBJ = $(MYPLUGIN_PLUGIN_SRC:src/%.c=$(OBJ_DIR)/%.o)

# Plugin flags
MYPLUGIN_PLUGIN_CFLAGS = -DKRYON_PLUGIN_MYPLUGIN

# Add to main build
PLUGIN_SRC += $(MYPLUGIN_PLUGIN_SRC)
PLUGIN_OBJ += $(MYPLUGIN_PLUGIN_OBJ)
PLUGIN_CFLAGS += $(MYPLUGIN_PLUGIN_CFLAGS)

# Platform-specific linkage (if needed)
PLUGIN_LDFLAGS += -lmyplatformlib
```

### Step 4: Integrate into Build System

Update main `Makefile` to include your plugin:

```makefile
# Include plugin makefiles
ifneq ($(filter myplugin,$(PLUGINS)),)
    include $(SRC_DIR)/plugins/myplugin/Makefile
endif
```

### Step 5: Create Build Configuration

Create `Makefile.myplugin`:

```makefile
# Enable myplugin
PLUGINS := myplugin

# Include main Makefile
include Makefile
```

## Using Services in Applications

Apps detect and use services at runtime:

```c
#include "platform_services.h"
#include "services/my_service.h"

int main() {
    // Initialize services
    kryon_services_init();

    // Check if service available
    if (kryon_services_available(KRYON_SERVICE_MY_SERVICE)) {
        // Get service interface
        KryonMyService *svc = kryon_services_get_interface(
            KRYON_SERVICE_MY_SERVICE
        );

        // Use service
        svc->do_something("Hello");
        svc->cleanup();
    } else {
        // Fall back to standard functionality
        printf("My service not available\n");
    }

    // Shutdown services
    kryon_services_shutdown();
    return 0;
}
```

## Best Practices

### 1. Design for Graceful Degradation

Apps should work without services:

```c
// GOOD: Check before using
if (kryon_services_available(KRYON_SERVICE_EXTENDED_FILE_IO)) {
    // Use extended file I/O
} else {
    // Fall back to fopen/fread
}

// BAD: Assume service exists
KryonExtendedFileIO *xfio = kryon_services_get_interface(...);
xfio->open(...); // CRASH if service not available!
```

### 2. Keep Interfaces Platform-Agnostic

Service interfaces should not expose platform-specific types:

```c
// GOOD: Generic interface
bool (*send_signal)(int pid, const char *signal);

// BAD: Platform-specific type
bool (*send_signal)(int pid, siginfo_t *info); // POSIX-specific!
```

### 3. Document Platform-Specific Behavior

```c
/**
 * Send signal to process
 *
 * Inferno: Writes to /prog/PID/ctl
 * POSIX: Uses kill() syscall
 * Windows: Uses TerminateProcess()
 */
bool (*send_signal)(int pid, const char *signal);
```

### 4. Use Feature Detection, Not Platform Detection

```c
// GOOD: Detect feature
if (kryon_services_available(KRYON_SERVICE_NAMESPACE)) {
    // Use namespace service
}

// BAD: Detect platform
#ifdef __linux__
    // Assume service available
#endif
```

## Testing Plugins

### Unit Testing

Test individual service implementations:

```c
void test_my_service() {
    KryonMyService *svc = myplugin_get_my_service();
    assert(svc != NULL);
    assert(svc->do_something("test") == true);
    svc->cleanup();
}
```

### Integration Testing

Test plugin registration and service lookup:

```c
void test_plugin_registration() {
    kryon_services_init();

    assert(kryon_services_available(KRYON_SERVICE_MY_SERVICE));

    KryonMyService *svc = kryon_services_get_interface(
        KRYON_SERVICE_MY_SERVICE
    );
    assert(svc != NULL);

    kryon_services_shutdown();
}
```

### Cross-Platform Testing

Build and test on multiple platforms:

```bash
# Standard build (no plugin)
make clean && make
./kryon test

# Plugin build
make clean && make -f Makefile.myplugin
./kryon test
```

## Example: Inferno Plugin

See `src/plugins/inferno/` for a complete example implementing:
- Extended file I/O using lib9
- Namespace operations (mount/bind)
- Process control via /prog device

Build with:
```bash
make -f Makefile.inferno
```

## Troubleshooting

### Plugin Not Registering

- Check `KRYON_PLUGIN_*` flag is defined during compilation
- Verify `__attribute__((constructor))` is present
- Check plugin included in `PLUGIN_SRC` in Makefile

### Service Returns NULL

- Check service type is in plugin's `has_service()` switch
- Verify `get_service()` returns non-NULL pointer
- Ensure service implementation is compiled and linked

### Linking Errors

- Check platform libraries are in `PLUGIN_LDFLAGS`
- Verify library paths are correct
- Use `nm` to check symbol presence

### Runtime Crashes

- Ensure service interface structs are static (not stack-allocated)
- Check all function pointers in interface are non-NULL
- Verify plugin initialization succeeded

## Reference

### Key Files

- `include/platform_services.h` - Services registry API
- `include/services/*.h` - Service interface definitions
- `src/services/services_registry.c` - Registry implementation
- `src/plugins/*/` - Plugin implementations

### API Functions

- `kryon_services_register()` - Register a plugin
- `kryon_services_available()` - Check if service available
- `kryon_services_get_interface()` - Get service interface
- `kryon_services_init()` - Initialize services system
- `kryon_services_shutdown()` - Shutdown services system

### Build Variables

- `PLUGINS` - List of plugins to enable
- `PLUGIN_SRC` - Plugin source files
- `PLUGIN_CFLAGS` - Plugin compiler flags
- `PLUGIN_LDFLAGS` - Plugin linker flags
- `PLUGIN_INCLUDES` - Plugin include paths

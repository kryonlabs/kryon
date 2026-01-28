# Kryon Platform Services Plugins

This directory contains platform-specific service plugins for kryon.

## Available Plugins

### Inferno Plugin (`inferno/`)

Provides Inferno-specific services when building for emu:

**Services:**
- **Extended File I/O**: Access device files (#c/cons, #p/PID/ctl, #D)
- **Namespace**: Mount/bind/unmount operations, 9P server creation
- **Process Control**: Process enumeration via /prog, control files

**Build:**
```bash
make -f Makefile.inferno
```

**Requirements:**
- Inferno installation at `/opt/inferno` or set `INFERNO` environment variable
- lib9 available

**Usage:**
```c
#include "platform_services.h"
#include "services/extended_file_io.h"

if (kryon_services_available(KRYON_SERVICE_EXTENDED_FILE_IO)) {
    KryonExtendedFileIO *xfio = kryon_services_get_interface(
        KRYON_SERVICE_EXTENDED_FILE_IO
    );
    KryonExtendedFile *cons = xfio->open("#c/cons", KRYON_O_RDWR);
    xfio->write(cons, "Hello from kryon!\n", 18);
    xfio->close(cons);
}
```

## Creating New Plugins

See [`../../docs/plugin_development.md`](../../docs/plugin_development.md) for detailed instructions on creating platform-specific plugins.

### Quick Start

1. Create plugin directory: `mkdir myplugin/`
2. Create `plugin.c` with registration code
3. Create service implementation files
4. Create `Makefile` with build configuration
5. Update main Makefile to include your plugin
6. Create `Makefile.myplugin` for easy building

## Plugin Architecture

```
plugin.c                  - Main registration, auto-loads via constructor
service1.c                - Service implementation 1
service2.c                - Service implementation 2
Makefile                  - Build configuration
```

Each plugin:
- Defines `KRYON_PLUGIN_*` flag
- Auto-registers using `__attribute__((constructor))`
- Implements service interfaces from `include/services/`
- Links to platform-specific libraries

## Building with Plugins

### Standard Build (No Plugins)

```bash
make
```

Apps work normally, services return NULL.

### Plugin Build

```bash
# Inferno
make -f Makefile.inferno

# Custom plugin
make PLUGINS=myplugin
```

### Multiple Plugins

```bash
make PLUGINS="plugin1 plugin2"
```

## Testing

Test standard build works:
```bash
make clean && make
./build/bin/kryon run examples/hello-world.kry
```

Test plugin build:
```bash
make clean && make -f Makefile.inferno
./build/bin/kryon run examples/hello-world.kry
```

Should see plugin registration message:
```
[Kryon Services] Registered plugin: inferno v1.0.0
```

## Future Plugins

Potential future plugins:

- **POSIX**: Extended features for Linux/macOS/BSD
- **Windows**: Windows-specific features (registry, services, etc.)
- **Plan 9**: Plan 9 from User Space integration
- **Wasm**: WebAssembly-specific features
- **Android**: Android platform integration
- **iOS**: iOS platform integration

## Notes

- Only one plugin can be active at a time
- Plugins are statically linked (no dlopen)
- Services provide opt-in OS features
- Apps remain portable - services are optional
- Missing services don't crash, apps adapt gracefully

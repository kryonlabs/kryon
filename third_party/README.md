# Third-Party Dependencies

This directory contains external dependencies managed as git submodules for the Kryon UI Framework.

## Dependencies

### cJSON (v1.7.15+)
- **Purpose**: Lightweight JSON parser in C
- **License**: MIT
- **Repository**: https://github.com/DaveGamble/cJSON
- **Used By**: IR serialization (JSON format), configuration files
- **Files**: `cJSON/cJSON.c`, `cJSON/cJSON.h`

### stb
- **Purpose**: Single-file public domain libraries
- **License**: Public Domain / MIT
- **Repository**: https://github.com/nothings/stb
- **Used By**:
  - `stb_image.h` - Image loading (PNG, JPEG, BMP) for sprites and assets
- **Files**: `stb/stb_image.h` (single-header library)

### libtickit (v0.4.5)
- **Purpose**: Terminal UI library for rendering TUI applications
- **License**: MIT
- **Repository**: https://github.com/leonerd/libtickit
- **Used By**: Terminal renderer backend
- **Status**: ⚠️ API compatibility issues being resolved (Phase 4)
- **Files**: Full library with headers in `libtickit/include/`

## Setup Instructions

### First-Time Clone

Clone the repository with submodules included:

```bash
git clone --recursive https://github.com/youruser/kryon.git
```

### Existing Repository

If you already have the repository cloned, initialize and update submodules:

```bash
git submodule update --init --recursive
```

### Verify Submodules

Check submodule status:

```bash
git submodule status
```

Expected output:
```
 <commit> third_party/cJSON (v1.7.15-33-g<hash>)
 <commit> third_party/stb (heads/master)
 <commit> third_party/libtickit (v0.4.5)
```

## Updating Dependencies

### Update All Submodules to Latest

```bash
git submodule update --remote --recursive
```

### Update Specific Submodule

```bash
cd third_party/cJSON
git pull origin master
cd ../..
git add third_party/cJSON
git commit -m "Update cJSON to latest"
```

### Pin to Specific Version

```bash
cd third_party/cJSON
git checkout v1.7.18  # or specific commit hash
cd ../..
git add third_party/cJSON
git commit -m "Pin cJSON to v1.7.18"
```

## Building

Submodules are automatically built as part of the main Kryon build system.

### Build IR Library (includes cJSON)

```bash
cd ir
make clean && make
```

This will:
1. Compile `cJSON.c` from the submodule
2. Build `libkryon_ir.a` (static library)
3. Build `libkryon_ir.so` (shared library)

### Build Desktop Backend

```bash
cd backends/desktop
make clean && make
```

The desktop backend automatically links against `libkryon_ir.a` which includes cJSON.

### Build Terminal Backend (in progress)

```bash
cd renderers/terminal
make clean && make
```

Note: Terminal backend currently has libtickit API compatibility issues (Phase 4 in progress).

## Include Paths

The build system automatically configures include paths for third-party dependencies:

### In ir/Makefile:
```makefile
CFLAGS += -I$(CJSON_DIR) -I$(STB_DIR)
```

### In Source Files:
```c
#include "cJSON.h"         // cJSON JSON parser
#include "stb_image.h"     // stb image loading
```

No need for full paths like `#include "third_party/cJSON/cJSON.h"` - the build system handles this.

## Troubleshooting

### Submodules Not Initialized

**Error**: `fatal: not a git repository` or missing files

**Solution**:
```bash
git submodule update --init --recursive
```

### Submodule Detached HEAD

**Error**: `HEAD detached at <commit>`

**Solution**: This is normal for submodules. The parent repository tracks specific commits, not branches.

### Build Errors with cJSON

**Error**: `cJSON.h: No such file or directory`

**Solution**:
1. Ensure submodules are initialized: `git submodule update --init`
2. Verify `third_party/cJSON/cJSON.h` exists
3. Check IR Makefile has correct include paths

### libtickit API Compatibility

**Error**: Terminal backend compilation fails with libtickit errors

**Status**: Known issue - Phase 4 (Terminal API fixes) in progress

**Workaround**: Desktop backend (SDL3) is fully functional and recommended for now

## Development Workflow

### Making Changes to Dependencies

1. **DON'T modify submodule code directly** - Changes will be lost on update
2. **DO fork the submodule** if you need custom changes
3. **Update submodule URL** in `.gitmodules` to point to your fork

### Testing with Different Versions

```bash
# Test with specific cJSON version
cd third_party/cJSON
git checkout v1.7.15
cd ../..
make clean && make

# Restore to tracked version
git submodule update --init
make clean && make
```

## CI/CD Integration

### GitHub Actions Example

```yaml
- name: Checkout with submodules
  uses: actions/checkout@v3
  with:
    submodules: recursive

- name: Build Kryon
  run: |
    make clean
    make
```

### GitLab CI Example

```yaml
variables:
  GIT_SUBMODULE_STRATEGY: recursive

build:
  script:
    - make clean
    - make
```

## License Information

Each submodule has its own license:

- **cJSON**: MIT License
- **stb**: Public Domain / MIT (dual license)
- **libtickit**: MIT License

See individual submodule repositories for full license text.

## Migration History

This third_party structure was established as part of the dependency management migration:

- **Phase 1**: Created centralized `third_party/` directory
- **Phase 2**: Added git submodules for cJSON, stb, libtickit
- **Phase 3**: Updated build system to use new paths
- **Phase 4**: Terminal backend API fixes (in progress)
- **Phase 5**: Updated source file include paths
- **Phase 6**: Created documentation (this file)

For detailed migration notes, see `THIRD_PARTY_MIGRATION_PLAN.md` and `PHASE3_BUILD_SYSTEM_COMPLETE.md`.

## Support

If you encounter issues with third-party dependencies:

1. Check this README for troubleshooting steps
2. Verify submodules are properly initialized
3. Review build system documentation in `PHASE3_BUILD_SYSTEM_COMPLETE.md`
4. Report issues at https://github.com/youruser/kryon/issues

---

**Last Updated**: 2025-12-02
**Migration Status**: ✅ Complete (except Phase 4 terminal backend)

# Flint UI

Flint is a lightweight C UI and app support library for embedded applications
and runtime environments. It provides core UI primitives, icon asset
management, shared Lyra account and sync helpers, and common third-party source
submodules for applications built on Flint.

## Layout

- `include/` - public headers for Flint API
- `src/` - C implementation files
- `icons/` - icon bitmaps embedded by the Flint icon asset source
- `vendor/curl` - shared libcurl source submodule for app networking builds
- `vendor/liboqs` - shared liboqs source submodule for Lyra account signatures
- `mk/` - reusable Make fragments for app, platform, asset, and vendor builds
- `scripts/` - build helpers and asset embedding tools
- `examples/` - example programs demonstrating Flint usage

## Building

Flint can be built directly or integrated as part of a larger project:

```bash
# Standalone build
make

# Or via CMake (for Android/embedded platforms)
cmake -B build
cmake --build build
```

On FreeBSD, use the repository `makefile` entrypoint. It delegates to the GNU
Make build while keeping `make` as the command maintainers and downstream apps
can run:

```bash
make bsd-check
```

## Integration

To integrate Flint into your project:

1. Add `include/` to your header search paths
2. Compile the `src/` files and link with your application
3. Use the `scripts/` tools to embed custom icon assets if needed
4. Include explicit Make fragments such as `mk/vendor.mk` when your app enables
   networking or Lyra account support

Applications should keep build artifacts in their own build directories, but
the dependency source of truth and common build recipes should live under Flint.
For TLS-enabled curl builds, Flint builds curl against the OpenSSL installation
discoverable by CMake or the `OPENSSL_*` make variables.

## Documentation

- `docs/API.md` documents the public API.
- `docs/AGENTS.md` documents how agents and downstream apps should use Flint,
  including modal/input capture and submodule update rules.

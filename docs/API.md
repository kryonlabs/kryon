# Kryon API

Kryon is a collection of ordinary Ziran modules under `src/ui/`. An
application imports only the modules it needs. The modules listed in
[`modules.txt`](../src/ui/modules.txt) currently pass Kryon's C, C++, and Go
build; other `.zi` files are moved source awaiting conversion.

## Use a checked module

Pass the Kryon source directory as an ordinary module path:

```sh
../ziran/build/bin/ziran check --root app \
    --module-path ../kryon/src/ui app/main.zi
```

For example, a non-graphical program can import layout policy without
initializing a window:

```zi
#module "main"
#import "scroll"

Answer :: () -> i32 #export {
    return ScrollClamp(120, 80)
}
```

The same module can be built into C, C++, or Go, serialized as `.zir`, or
linked into a `.zib`. Ziran owns those commands and formats. Kryon has no
compiler or bundle loader. A `.zib` links Kryon code only when imported by the
program; effects that need a platform host must be supplied by declared host
interfaces.

## Current surfaces

The checked modules cover geometry, layout, styling, themes, accessibility,
focus, input, scroll, canvas transforms, popup policy, and several widget
measurement and paint decisions. [`modules.txt`](../src/ui/modules.txt) is the
precise build inventory. [`ziran_*_test.sh`](../tests/ziran_moved_modules_test.sh)
files show source, saved-IR, and bundle use of those modules.

`build/ziran/libkryon.a` is the current C archive; generated headers are in
`build/ziran/c/`. The first portable host binding is
`FramePacingBinding()` from `include/kryon_portable_host.h` in
`build/ziran/libkryon_host.a`; the caller supplies the platform
`SetTargetFPS` and passes the binding to Ziran's `BundleRun`. Build and test
with `make` and `make test` from the Kryon repository. No display is started.

## Migration boundary

This is a policy library subset. Full `Text(TextProps)`, `Image(ImageProps)`,
widget composition, broader platform rendering and host linking, and downstream
application integration are unfinished. Modules outside `modules.txt` may
still contain syntax or imports from the retired implementation and are not
supported API yet. See [architecture](ARCHITECTURE.md) and
[boundaries](BOUNDARIES.md) for the intended ownership and completion gates.

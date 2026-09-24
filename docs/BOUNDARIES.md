# Kryon boundaries

Kryon is a reusable UI library authored in Ziran. It owns widgets, layout,
style, accessibility, focus, text editing, and interaction policy. Applications
import the Kryon modules they use; Kryon is not part of the Ziran compiler or
portable format.

## Source ownership

| Owner | Code |
| --- | --- |
| Ziran repository | `.zi` language, `.zir` representation, `.zib` format, checker, code generation, and portable execution |
| Kryon `src/ui/*.zi` | Widget APIs and reusable UI behavior |
| Kryon Ziran platform modules | Operating system input, windows, font and image loading, rasterization, and storage effects through general Ziran capabilities |
| Application repositories | Screens, product state, copy, assets, and app-specific workflows |

The Ziran compiler and portable loader have no widget-specific branches. A
program can compile and run without importing Kryon. A program that imports
Kryon links its imported modules and supplies only the host capabilities those
modules call.

## Current implementation

Every maintained `.zi` module in `src/ui/` appears in
[`src/ui/modules.txt`](../src/ui/modules.txt). All 192 modules use current
Ziran syntax. `make all` passes source checking, checked `.zir` output,
strict C, C++, and Go generation, native C/C++ compilation, and Go package
compilation. The default build emits only the library generated from Ziran;
the C portable host archive is still built for the existing test gate. Backend
host declarations also compile with the current Ziran toolchain. Platform
hosts and downstream applications have not yet completed
their integration with the library.

The unused handwritten Go runtime has been removed. The current archives
contain all checked UI modules and C portable host adapters. Those C adapters
and C examples are migration debt: they still support today's tests and must
be replaced with current Ziran source. Application integration and supported
renderer paths still need completion.

The optional SDL2/Cairo example opens a Kryon bundle in a private-display
desktop test. Its checks capture a clicked button and a tinted, asset-backed
PNG image. It is an integration proof; texture handles, other image formats,
text services, and production platform hosts remain unfinished.

## Module rule

Put a UI decision in a `.zi` module when it can be expressed with explicit
inputs and outputs. The host may observe device state or perform effects, but
it must not independently decide widget behavior. Add a module to
`src/ui/modules.txt` only after its generated C, C++, and Go compile and its
relevant behavior is checked. Keep application-specific behavior in the
application repository.

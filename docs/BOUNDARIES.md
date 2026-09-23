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
| Kryon platform hosts | Operating system input, windows, font and image loading, rasterization, and storage effects |
| Application repositories | Screens, product state, copy, assets, and app-specific workflows |

The Ziran compiler and portable loader have no widget-specific branches. A
program can compile and run without importing Kryon. A program that imports
Kryon links its imported modules and supplies only the host capabilities those
modules call.

## Current implementation

The modules in [`src/ui/modules.txt`](../src/ui/modules.txt) pass the native C,
C++, and Go build. The other `.zi` files in `src/ui/` preserve the UI source
while its types, imports, and host interfaces are converted. They are not part
of the library archive yet. Some still refer to generated headers or syntax
from the retired compiler and must not be advertised as working modules.

The existing C and Go host implementations are migration material. They are
not included by `make` and do not presently provide a linked application UI.
The checked archive is a policy subset, not the complete widget runtime.

## Module rule

Put a UI decision in a `.zi` module when it can be expressed with explicit
inputs and outputs. The host may observe device state or perform effects, but
it must not independently decide widget behavior. Add a module to
`src/ui/modules.txt` only after its generated C, C++, and Go compile and its
relevant behavior is checked. Keep application-specific behavior in the
application repository.

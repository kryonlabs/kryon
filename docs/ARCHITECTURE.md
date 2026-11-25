# Kryon architecture

Kryon supplies reusable UI behavior as Ziran modules. Ziran owns the source
language, checker, standard library, `.zir`, `.zx`, code generation, and
portable execution. Applications import Kryon only when they need its UI
library. The compiler and bundle loader have no Kryon-specific branches.

## Source ownership

| Location | Responsibility |
| --- | --- |
| `src/ui/*.zi` | Widget APIs, retained tree, layout, input decisions, styling, text, accessibility policy, and paint commands |
| `src/backend/*.zi` | Terminal and window hosts: platform observations, native bindings, rasterization, and presentation |
| `src/project/*.zi` | Kryon project tool that builds an app through Ziran |
| `tests/*_host.zi` | Ziran behavior-test hosts; not part of the library or application hosts |
| Application repositories | Screens, assets, translations, product data, and workflows |

All maintained implementation files under `src/` are `.zi`. The core module
inventory is [`src/ui/modules.txt`](../src/ui/modules.txt). Applications import
each widget package by name, such as `Text :: #import "kryon/text";`, and the
shared core modules they use. Optional packages
have separate imports and build targets; the core UI does not import them.

Leaf widgets ask `layout_width` for the width supplied by their parent's
measurement callback. Container builders own that callback alongside their
placement policy; a label does not import rows, grids or their painters.
`paint_primitives` connects container painting to the frame's queue without
requiring a renderer for layout-only programs. Shared `style_facts` and
`color_props` hold values independently of installed rule tables and material
effects.

Sessions and retained paint consult `bitmap_state`; the bitmap store registers
metadata and disposal callbacks only after allocating an image. Portable
editing widgets install `text_editor_zx` through `text_host` frame and transport
slots. Plain label frames carry neither editor buffers nor edit-request queues.
These are ordinary module and procedure boundaries, without compiler cases or
feature flags.

## Widget and host boundary

The host observes input and submits it to a `Session`. Kryon widget modules
decide focus, hit testing, layout, semantics, styling, and paint commands.
The host supplies system effects such as windows, terminal output, font and
image loading, rasterization, and storage. A host should not implement a
second set of widget decisions.

An application owns `main` and calls `App.Show` or `App.Run`, or exports
`Frame(session: Session, viewport: Rectangle) -> s32` for a profile host.
A frame host calls it between `BeginFrame` and `EndFrame`. Package
profiles choose the terminal, desktop, Android or Canvas2D host;
see [BACKENDS.md](BACKENDS.md). A non-graphical Ziran program can
import a Kryon policy module without selecting any of these hosts.

## Build and verification

`make` checks the Ziran modules, writes `.zir`, generates C and Go,
compiles native objects, and creates a C archive from generated source.
`make test` exercises the checked library through source, saved `.zir`, and
portable `.zx` behavior, Canvas2D Wasm effects, and packaged Canvas2D
browser applications in private headless Chromium. Desktop package tests run on private Xvfb displays;
CI runs them too. `make sanitize-test` repeats the behavior tests with AddressSanitizer and UndefinedBehaviorSanitizer.
[FEATURE_MATRIX.md](FEATURE_MATRIX.md) states exactly what those checks cover.

Ziran's generic capabilities should be added to Ziran and imported here.
Kryon-specific compiler cases, compatibility runtimes, and app-specific
screens do not belong in this repository.

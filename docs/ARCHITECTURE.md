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
| `src/tsx/*.ts` and `src/tsx/*.tsx` | Typed composition bindings to the native widgets and containers |
| `src/project/*.zi` | Kryon project tool that builds an app through Ziran |
| `tests/*_host.zi` | Ziran behavior-test hosts; not part of the library or application hosts |
| Application repositories | Screens, assets, translations, product data, and workflows |

Native widget and host implementations remain `.zi`; their core module
inventory is [`src/ui/modules.txt`](../src/ui/modules.txt). Applications compose
screens in `.tsx`, importing typed bindings such as
`import { Text } from "kryon/tsx/text"`. Those bindings call the same native
widget implementations and submit to the same retained tree. Ziran application
modules own domain state, resources and operations; their checked declarations
are imported directly into TSX. Optional packages have separate imports and build
targets; the core UI does not import them.

Leaf widgets ask `layout_width` for the width supplied by their parent's
measurement callback. Container builders own that callback alongside their
placement policy; a label does not import rows, grids or their painters.
`paint_primitives` connects container painting to the frame's queue without
requiring a renderer for layout-only programs. Shared `style_facts` and
`color_props` hold values independently of installed rule tables and material
effects.

Plain text carries a typography-only `TextFace` through `text_style` resolver
and motion slots. `style_sheet` projects full cascade results into that face;
`style_motion` installs sampling only when used. Frames publish their session
through `style_frame`; installing rules connects the optional `style_tree`
selector matcher, including sheets installed during a frame. `text_theme`
supplies the built-in text and link colors until an installed theme registers
its live getters. Shadow painting belongs to `elevation_paint`, imported by
containers and floating surfaces, rather than the generic paint queue. A plain
portable label therefore imports neither full styles, selector matching,
shadow policy nor theme tables.

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

An application owns its TSX `main` and passes a typed `App.Frame` composition
function to `App.Run`. A native frame host invokes that function between
`BeginFrame` and `EndFrame`. Package
profiles choose the terminal, desktop, Android or Canvas2D host;
see [BACKENDS.md](BACKENDS.md). A non-graphical Ziran program can
import a Kryon policy module without selecting any of these hosts.

The generic Ziran frontend checks `.ts`, `.tsx` and `.zi` in one project.
`ziran ir --project -o build/ir` saves the common checked IR used by the
existing native and portable targets. JSX components are ordinary typed
callables. Container bindings declare scoped child callbacks so composition
runs inside the native container scope. The compiler has no widget catalogue,
and the build does not generate Ziran screen sources.

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

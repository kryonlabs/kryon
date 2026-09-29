# Kryon architecture

Kryon supplies reusable UI behavior as Ziran modules. Ziran owns the source
language, checker, standard library, `.zir`, `.zib`, code generation, and
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
| Game2D repository | Optional game API; Kryon determines its shared raylib revision when both packages are used |

All maintained implementation files under `src/` are `.zi`. The core module
inventory is [`src/ui/modules.txt`](../src/ui/modules.txt). The two public
import modules, `Kryon` and `Widgets`, compose that surface. Optional packages
have separate imports and build targets; the core UI does not import them.

## Widget and host boundary

The host observes input and submits it to a `Session`. Kryon widget modules
decide focus, hit testing, layout, semantics, styling, and paint commands.
The host supplies system effects such as windows, terminal output, font and
image loading, rasterization, and storage. A host should not implement a
second set of widget decisions.

An application exports `Frame(session: Session, viewport: Rectangle) -> s32`.
The selected host calls it between `BeginFrame` and `EndFrame`. Package
profiles choose the terminal, desktop, libdraw, raylib, Canvas2D, or semantic
DOM host; see [BACKENDS.md](BACKENDS.md). A non-graphical Ziran program can
import a Kryon policy module without selecting any of these hosts.

## Build and verification

`make` checks the Ziran modules, writes `.zir`, generates C, C++, and Go,
compiles native objects, and creates a C archive from generated source.
`make test` exercises the checked library through source, saved `.zir`, and
portable `.zib` behavior, Canvas2D Wasm effects, and packaged Canvas2D and
semantic DOM browser applications in private headless Chromium. Desktop,
raylib, and libdraw package tests run separately on private Xvfb displays;
CI runs them too. `make sanitize-test` repeats the behavior tests with AddressSanitizer and UndefinedBehaviorSanitizer.
[FEATURE_MATRIX.md](FEATURE_MATRIX.md) states exactly what those checks cover.

Ziran's generic capabilities should be added to Ziran and imported here.
Kryon-specific compiler cases, compatibility runtimes, and app-specific
screens do not belong in this repository.

# Repository boundaries

| Owner | Code and data |
| --- | --- |
| Ziran | Generic TS/TSX and Ziran frontends, common checked IR, ownership checks, standard library, `.zir`, `.zx`, and generic host capabilities |
| Kryon | Native Ziran widget implementations, typed TSX component bindings, retained tree, widget decisions, and UI hosts |
| Applications | TSX screens, composition, styling and event wiring; Ziran domain state, actions, resources, storage and computation; product copy, translations and assets |

Kryon is imported by applications. It is not linked into programs that never
import it, and the Ziran compiler does not special case widgets. A generic
capability missing from the language belongs in Ziran. A reusable UI choice
belongs in Kryon. A product-specific behavior belongs in the application.

Application screens import ordinary typed component values from `kryon/tsx/*`.
Their native props, results and actions come from the checked Ziran declarations;
applications do not maintain duplicate TypeScript schemas or run a transpiler.
The normal mixed-source build checks `.tsx`, `.ts` and `.zi` together and emits
the common IR. Widget implementations, platform hosts, nonvisual policy modules
and custom drawing remain Ziran. One retained tree supplies rendering, layout,
input, clipping, focus and accessibility.

Text tokenization, word/grapheme wrapping and line height arithmetic live in
Ziran's `std/text_layout`. It borrows UTF-8 ranges and accepts a typed measurement
callback plus caller-owned context, without windows, font services or allocation.
Kryon supplies glyph measurement, caches tied to font revisions, widget properties,
selection and paint. Importing the core alone does not import Kryon.

Portable frame time is supplied by the optional clock module to motion and
component timers. Low-level lifecycle tests can supply explicit ticks without
a clock. Static `App.Run` programs do not import the concrete host clock or
component timer storage.

Kryon's graphical host owns platform windows, input events, and presentation.
Its widget modules own focus, pointer capture, layout, semantics, and paint
decisions. The host may call native libraries through declared Ziran foreign
bindings, but must not duplicate widget policy.

Make upstream Kryon changes on `master` and commit them there. A downstream
app picks them up by updating its `ziran.lock`. Never edit
Kryon's source inside an application. See [ARCHITECTURE.md](ARCHITECTURE.md)
for source modules and [PROJECTS.md](PROJECTS.md) for package use.

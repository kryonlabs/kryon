# Repository boundaries

| Owner | Code and data |
| --- | --- |
| Ziran | Language, compiler, standard library, `.zir`, `.zx`, and generic host capabilities |
| Kryon | Reusable Ziran UI modules, widget decisions, and UI hosts |
| Applications | Screens, copy, translations, assets, data storage, migrations, and product workflows |

Kryon is imported by applications. It is not linked into programs that never
import it, and the Ziran compiler does not special case widgets. A generic
capability missing from the language belongs in Ziran. A reusable UI choice
belongs in Kryon. A product-specific behavior belongs in the application.

Text tokenization, word/grapheme wrapping and line height arithmetic live in
Ziran's `std/text_layout`. It borrows UTF-8 ranges and accepts a typed measurement
callback plus caller-owned context, without windows, font services or allocation.
Kryon supplies glyph measurement, caches tied to font revisions, widget properties,
selection and paint. Importing the core alone does not import Kryon.

Portable frame time is observed only by optional motion modules, once per frame
at first use. Static `App.Run` programs do not import the concrete host clock.

Kryon's graphical host owns platform windows, input events, and presentation.
Its widget modules own focus, pointer capture, layout, semantics, and paint
decisions. The host may call native libraries through declared Ziran foreign
bindings, but must not duplicate widget policy.

Make upstream Kryon changes on `master` and commit them there. A downstream
app picks them up by updating its `ziran.lock`. Never edit
Kryon's source inside an application. See [ARCHITECTURE.md](ARCHITECTURE.md)
for source modules and [PROJECTS.md](PROJECTS.md) for package use.

# Repository boundaries

| Owner | Code and data |
| --- | --- |
| Ziran | Language, compiler, standard library, `.zir`, `.zib`, and generic host capabilities |
| Kryon | Reusable Ziran UI modules, widget decisions, and UI hosts |
| Game2D | Optional game API and game behavior using Kryon's selected raylib backend |
| Applications | Screens, copy, translations, assets, data storage, migrations, and product workflows |

Kryon is imported by applications. It is not linked into programs that never
import it, and the Ziran compiler does not special case widgets. A generic
capability missing from the language belongs in Ziran. A reusable UI choice
belongs in Kryon. A product-specific behavior belongs in the application.

Kryon's graphical host owns platform windows, input events, and presentation.
Its widget modules own focus, pointer capture, layout, semantics, and paint
decisions. The host may call native libraries through declared Ziran foreign
bindings, but must not duplicate widget policy.

Make upstream Kryon changes on `master`, commit them there, and update a
downstream app only through a clean `vendor/kryon` submodule pointer. Never
edit the vendored source inside an application. See [ARCHITECTURE.md](ARCHITECTURE.md)
for source modules and [PROJECTS.md](PROJECTS.md) for package use.

# Kryon laws

Kryon's current executable gates are `make all` and `make test`. They check
the modules listed in [`src/ui/modules.txt`](../src/ui/modules.txt), generate
`.zir`, C, C++, and Go, compile native objects, and run the current Ziran
integration tests. The checked modules are only a subset of the UI library.

The previous KIR compiler laws and `.kry` runtime test harness were removed
with that compiler. Existing Bend packages under `laws/` are preserved as
reference material, but they are not part of the current build gate or a proof
of the new Ziran implementation. Before relying on a law for a migrated UI
module, connect it to that module and run its proof and behavioral comparison
again.

Ziran's language laws and portable-format checks belong in the separate Ziran
repository. Kryon-specific laws should describe observable widget behavior
and should not require special cases in the Ziran compiler or loader.

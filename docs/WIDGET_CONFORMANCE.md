# Widget verification

Each widget has a Ziran implementation under `src/ui/` and should have a
focused behavior test for its public values, retained state, and input
decisions. Tests that only verify code generation do not prove behavior.

The current `make test` gate builds the core and optional packages and runs
the `tests/ziran_*` behavior suite through the relevant source, saved `.zir`,
portable `.zib`, and native targets. Package project tests verify imports and
terminal host behavior. `make desktop-project-test`,
`make raylib-project-test`, and `make libdraw-project-test` verify graphical
capture and input on private Xvfb displays.

For an affected widget, check the interaction that changed through the
session and selected host. Input widgets need focus, editing, Unicode, and
selection cases; collection widgets need selection, scrolling, and
activation cases; image and text widgets need paint and clipping cases.
An untested host or platform remains unverified even when another target
passes. [FEATURE_MATRIX.md](FEATURE_MATRIX.md) records supported hosts.

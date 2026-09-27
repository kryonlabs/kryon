# Remaining Kryon work

This plan lists only unfinished work. Current behavior and build commands are
documented in [ARCHITECTURE.md](../docs/ARCHITECTURE.md),
[PROJECTS.md](../docs/PROJECTS.md), and [BACKENDS.md](../docs/BACKENDS.md).

| Open work | Completion evidence |
| --- | --- |
| Replace the C portable test host fixtures in `tests/support/` with Ziran test hosts or generic Ziran host capabilities | No handwritten Kryon test-host implementation remains; the same behavior tests pass from source, saved `.zir`, and `.zib` |
| Verify public widget behavior through the packaged Ziran imports and supported hosts, including text editing, focus, accessibility, image loading, and optional packages | Focused executable tests pass for each affected path; `make test` and the relevant private-display project tests pass |
| Add platform accessibility adapters only where a host can provide and test them | A real private-session accessibility test proves semantic tree, focus, action, and text behavior; documentation names unsupported platforms |
| Keep the public site and application instructions aligned with tested package behavior | Examples, links, and build commands resolve against the current `master` checkout |

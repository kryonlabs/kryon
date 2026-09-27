# Remaining Kryon work

Kryon is a Ziran UI package. All maintained implementation files under
`src/` are `.zi`; the old `.kry` compiler, KIR, KRB loader, C widget runtime,
and handwritten JavaScript web runtime have been removed. The current build
and host contract is documented in [ARCHITECTURE.md](../docs/ARCHITECTURE.md),
[PROJECTS.md](../docs/PROJECTS.md), and [BACKENDS.md](../docs/BACKENDS.md).

This page records work that still has a current owner. Completed migrations
and the former phase plans are retained only in Git history.

| Open work | Completion evidence |
| --- | --- |
| Replace the C portable test host fixtures in `tests/support/` with Ziran test hosts or generic Ziran host capabilities | No handwritten Kryon test-host implementation remains; the same behavior tests pass from source, saved `.zir`, and `.zib` |
| Verify public widget behavior through the packaged Ziran imports and supported hosts, including text editing, focus, accessibility, image loading, and optional packages | Focused executable tests pass for each affected path; `make test` and the relevant private-display project tests pass |
| Add platform accessibility adapters only where a host can provide and test them | A real private-session accessibility test proves semantic tree, focus, action, and text behavior; documentation names unsupported platforms |
| Keep the public site and application instructions aligned with tested package behavior | Examples, links, and build commands resolve against the current `master` checkout |

The web backend is unavailable. A future web host is separate work and must
use Ziran as an ordinary language and Kryon as an imported library. Language
features and `.zir`/`.zib` changes belong in the Ziran repository. Game APIs
belong in Game2D; application screens and data migrations belong in the app
repositories.

Never run a graphical or window-management test on the developer's live
display. Use a private Xvfb/Xephyr display and scrub inherited display
variables.

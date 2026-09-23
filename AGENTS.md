# Kryon repository rules

Kryon is a reusable UI library authored in Ziran. The Ziran language,
compiler, `.zir` representation, `.zib` format, and portable runtime live in
the separate `../ziran` repository. Kryon source lives in `src/ui/*.zi`.

## Source and build

- Make changes in this upstream repository on `master`. Commit and push here
  before updating a downstream application's clean `vendor/kryon` pointer.
- `src/ui/modules.txt` lists modules that pass the current C, C++, and Go
  build. Moved modules outside that list still need conversion; a checker pass
  alone does not establish native or portable readiness.
- `make test` is the current Kryon gate. It builds the checked Ziran library
  and runs Ziran source, saved-IR, and bundle behavior tests without a display.
- Use direct domain names for APIs. Do not restore the removed KIR compiler,
  `.kry` runtime, or `.krb` loader as compatibility paths.

## UI ownership

Author reusable widget behavior, composition, layout, interaction, styling,
accessibility, and UI tree decisions in `.zi`. Platform hosts may gather OS
input, manage windows, load fonts and images, rasterize, and store data. Host
code must not independently choose widget behavior.

An application imports Kryon as an ordinary Ziran library. The Ziran compiler
and portable loader must never recognize Kryon widget names specially. An
application that does not import Kryon needs no Kryon code or graphical host.
Keep app-specific screens, assets, copy, workflows, and state in the app repo.

One widget concept has one public implementation. `Text(TextProps)` and
`Image(ImageProps)` are the intended text and image widget surfaces. Do not
add parallel wrappers or expose raw texture draw calls as app widget APIs.

## Testing and desktop safety

Test changed modules through all relevant generated targets and portable
bundles. Use focused behavior tests for decisions that could diverge across
targets. Do not treat code generation alone as proof that a host or app works.

Never run tests or binaries that can send X/Wayland requests against the
developer's live desktop. Use a private Xvfb/Xephyr display for any visual or
window-management test, and scope window effects to processes Kryon owns.
Never touch the inherited display or `/tmp/.X11-unix/X0` during cleanup.

## Bend

When using Bend, run `bend guide`, keep important rules in `LAWS.bend`, run
`bend PROOF.bend` before committing, and parallelize the code where possible.

# Kryon architecture

Kryon is an ordinary Ziran library. Ziran owns the language, checker, `.zir`,
native backends, `.zib` linker, and portable runtime. Kryon owns widget APIs,
composition, layout, interaction, styling, accessibility, and rendering policy.
Applications import Kryon modules from `src/ui/`. Neither Ziran's compiler nor
its portable loader recognizes widget names.

## Source and host boundary

| Location | Responsibility |
| --- | --- |
| `src/ui/*.zi` | Kryon declarations and reusable UI behavior |
| `src/ui/modules.txt` | Modules that pass the current full native build |
| `src/backend/`, other C and Go host code | Raw platform input, windows, font measurement, rasterization, storage, and device services |
| `../ziran` | Language implementation and generic execution |

Host adapters provide observations and effects through declared interfaces.
Widget state, policy, layout, and draw decisions belong in `.zi`, including for
desktop, browser, and portable hosts. Platform adapters may use C or Go where
the operating system requires it; they must not duplicate widget policy.

## Current build

`make` compiles the [checked module list](../src/ui/modules.txt) to `.zir`, C,
C++, and Go, then builds `build/ziran/libkryon.a`. `make test` runs the current
source, saved-IR, and portable bundle tests. The checked modules cover
geometry, layout, accessibility, focus, canvas transforms, drag, swipe, and
scroll interaction, text input and text row decisions, theme and style values,
menu, color picker, and selected Image, Separator, and Progress paint decisions.

The other `.zi` modules were moved from the previous implementation and are
not yet in the checked build. Native widget rendering and downstream app
integration are incomplete. The current C archive is therefore a subset of
the intended Kryon library. A `.zib` containing Kryon code must link only the
modules an application imports and require host capabilities explicitly.

## Completion requirements

- Every maintained widget and its state, style, and layout behavior is a
  checked `.zi` module and joins the build.
- Native C, C++, Go, browser, and portable hosts execute equivalent supported
  behavior through declared effects.
- The public widget API is importable from applications without compiler rules
  keyed to Kryon names.
- Downstream applications build and pass their behavior gates with Ziran source,
  saved `.zir`, and linked `.zib` where applicable.

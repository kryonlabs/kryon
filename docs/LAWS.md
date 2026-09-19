# Kryon laws

A law is a named invariant that is checked by a machine, not by review.
Laws gate strict compiler builds and the `make laws-test` target, so code
that violates one cannot merge green. Each law has a stable dotted name
that appears in diagnostics and test output; agents and humans can grep
for it. To add a law: extend `cmd/kir/kir_laws.c` (compiler tier), add a
`LAW_BEGIN` block under `tests/laws/` (runtime tier), or extend a
`tools/check-*.sh` script (static tier), then register it in this file.

## Compiler tier

Enforced by `KirCheckLaws` (`cmd/kir/kir_laws.c`) on every strict compile
(`k2c --strict`, `k2cpp --strict`, `k2go --strict`, `k2js --strict`);
lenient builds count violations silently, mirroring `KirCheckPrograms`.

| Law | Enforces |
|---|---|
| `image.surface.no_low_level_calls` | `.kry` must draw images through `Image(ImageProps)`, not raw `Texture`/`DrawTexture*`/`UIText*`/`UIRender*`/`TextInputControl` host calls |

## Runtime tier

Property laws over generated inputs, run by `make runtime-laws-test`
(part of `make laws-test`). Binaries live under `tests/laws/` and use the
`lawcheck.h` harness (`LAW_BEGIN`/`REQUIRE`) with a deterministic shared
LCG so runs are reproducible.

| Law | Binary | Enforces |
|---|---|---|
| `slider.ratio.clamped` | slider_laws_test | slider ratios always clamp to 0..1 |
| `slider.value.ratio.roundtrip.clamped` | slider_laws_test | value -> ratio -> value stays in range |
| `slider.pointer.ratio.clamped` | slider_laws_test | pointer ratios clamp, zero-length tracks yield 0 |
| `slider.discrete.value.in.range` | slider_laws_test | discrete values stay within min..max |
| `layout.metrics.content.nonnegative` | layout_laws_test | content boxes never go negative |
| `input.drag.monotonic.with.distance` | layout_laws_test | drag detection is monotonic in distance |
| `paned_view.split.clamped` | layout_laws_test | pane splits clamp to their limits |
| `style.priority.deterministic.ordering` | layout_laws_test | style priority follows layer/specificity/order |
| `style.priority.absent.never.wins` | layout_laws_test | absent priorities never win |
| `layout.tree.deterministic` | layout_laws_test | identical builds (same seed, retained keys) produce identical bounds |
| `layout.tree.idempotent` | layout_laws_test | re-running layout on a committed tree reproduces its bounds |
| `layout.tree.children.start.in.content` | layout_laws_test | auto children start at the content origin and the cursor only advances |
| `layout.tree.bounds.finite` | layout_laws_test | all post-layout bounds are finite with non-negative sizes |
| `semantic.button.label.equals.text.child` | semantic_tree_laws_test | a button label and an equivalent child text node lay out identically |

## Static tier

Shell checks over sources and generated output, run by `make
api-laws-test` and `make backend-capability-laws-test`.

| Law | Script | Enforces |
|---|---|---|
| generated-output blocked names | `tools/check-kryon-laws-api.sh` | internal runtime APIs (`DrawUI*`, `BeginUIFrame`, ...) never leak into generated C |
| app-facing blocked UI surface | `tools/check-kryon-laws-api.sh` | `examples/` and `tests/fixtures/` `.kry` never call the blocked texture/UI surface |
| generated props ownership | `tools/check-kryon-laws-api.sh` | every `include/ui_*_props.generated.h` has a `runtime/*.kry` source |
| backend capabilities | `tools/check-backend-capability-laws.sh` | `docs/BACKEND_CAPABILITIES.json` stays complete and honest per backend |

## Cross-target tier

| Law | Check | Enforces |
|---|---|---|
| runtime parity | `make cross-target-laws-test` | C, Go, and JS runtime outputs agree |

## Running

- Everything: `make laws-test` (also part of `make test` and `preflight`)
- Compiler laws only: compile any `.kry` with `k2c --strict`
- One runtime binary: `make build/linux-x86_64/tests/layout_laws_test && ./build/linux-x86_64/tests/layout_laws_test`

# Kryon laws

A law is a named invariant that is checked by a machine, not by review.
Compiler laws gate every compiler build; runtime laws gate tests, so code
that violates one cannot merge green. Each law has a stable dotted name
that appears in diagnostics and test output; agents and humans can grep
for it. To add a law: extend `cmd/kir/kir_laws.c` (compiler tier), add a
`LAW_BEGIN` block under `tests/laws/` (runtime tier), or extend a
`tools/check-*.sh` script (static tier), then register it in this file.

## Compiler tier

Enforced by `KirCheckLaws` (`cmd/kir/kir_laws.c`) before lowering in `k2c`,
`k2cpp`, `k2go`, `k2js`, `k2kir`, and `k2b`. Default builds, `--strict`, and
`--no-strict` all enforce laws. The strictness flags only control the broader
type checker; `k2b --allow-unsupported` cannot disable a law either.

These compiler laws are checks over parsed syntax, not general proofs of
program behavior. Runtime property tests exercise generated cases and do not
prove a property for every possible input. Keep that distinction explicit.

| Law | Enforces |
|---|---|
| `image.surface.no_low_level_calls` | `.kry` must draw images through `Image(ImageProps)`, not raw `Texture`/`DrawTexture*`/`UIText*`/`UIRender*`/`TextInputControl` host calls |

## Proof tier

`tools/bend-laws.mjs` checks paired `LAWS.bend` / `PROOF.bend` packages with
the upstream Bend 2 checker. Bend is pinned in `vendor/bend`; its checker and
Base library hashes are checked before loading. Node.js 22.18+ is a host build
dependency. The checker rejects holes, open declarations, `@unsafe`, foreign
implementations, remote imports, and imports outside the package. No Bend
runtime or network fetch is needed on the target device.

Run `node tools/bend-laws.mjs path/to/PROOF.bend`. For finite policies, import
`checkLaws` and use the returned `table(functionName)` to enumerate every input
constructor and normalize the checked implementation directly in the kernel.
Only finite, nonempty enumerations without fields are accepted as inputs;
outputs are closed constructors and U32 values. The consumer must validate
its domain mapping and compile the generated table into the application.

These are proofs of the declared pure policy, conditional on the pinned
checker and Base semantics. The small host generator, C adapter, build graph,
and target compiler remain trusted integration code and need regression tests.
This does not prove arbitrary `.kry`, network delivery, or UI behavior. Keep
application policy and its laws in the application repository. Changes to
contracts and this trust boundary require review separately from implementation
changes; a modifiable repository cannot make its own checks immutable.

`make bend-laws-test` verifies acceptance, evaluation, and rejection paths and
is part of `make laws-test`. This is distinct from the runtime tests below.

### First production-policy reference laws

[laws/focus](../laws/focus/README.md) contains three checked Bend laws for
`FocusTabDirectionFor`: no movement without Tab, forward movement with Tab,
and backward movement with Shift+Tab. `make focus-bend-laws-test`, included in
`make laws-test`, compares the checked reference against generated C over all
four Boolean input combinations and rejects missing proofs and deliberate
reference/native mutations. This is finite-domain native execution evidence,
not a formal compiler-preservation proof or C++/Go evidence.

The [ten-phase law plan](../plan/law/README.md) describes the remaining formal
connection, broader widget coverage and dependency-free release gates.

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

## Inventory

[laws/inventory.json](../laws/inventory.json) is the machine-readable law
inventory introduced by [phase 1](../plan/law/01-baseline-and-contracts.md)
of the law plan. Every canonical widget module, KIR statement/expression
construct, compiler pass, KRB operation and enforced law has a row with a
stable dotted id, phase assignment, targets, status and evidence level.
Statuses are `proposed`, `specified`, `model-proved`,
`implementation-connected`, `integration-verified` and `deferred`; a test
pass never promotes a proof status. `node tools/check-law-inventory.mjs`
validates the inventory against `docs/CANONICAL_WIDGET_SURFACE.md`,
`cmd/kir/kir.h` and the runtime module set on disk; `--report` renders a
deterministic coverage table. Mutation tests live in
`tests/law_inventory_test.mjs`. Both run under `make law-inventory-check`,
part of `make laws-test`.

`make law-release-boundary-check` (also part of `make laws-test`) probes the
phase 1 dependency boundary: released surfaces (`include/`, `cmd/`,
`runtime/`) must be free of proof-tier references, `vendor/bend` must match
the pinned checker commit in `tools/bend-pin.json`, and Bend packages stay
under `laws/`.

## Running

- Everything: `make laws-test` (also part of `make test` and `preflight`)
- Compiler laws: every `.kry` compile; regression coverage runs in `make spec-test`
- One runtime binary: `make build/linux-x86_64/tests/layout_laws_test && ./build/linux-x86_64/tests/layout_laws_test`

# Phase 2 — Harden proof gates and connect one production policy

Status: planned, not implemented by this planning change.
Estimate: **5–10 focused engineer-days; 250–600 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: phase 1 accepted. Reuse `tools/bend-laws.mjs`, its pinned upstream
checker, finite table evaluation and `tests/bend_laws_test.mjs`.

Work:

- Audit loading, pinned hashes, required laws, termination restrictions, holes,
  foreign/unsafe escape paths and import containment. Extend regression tests
  rather than building a second checker. Add deterministic resource/time limits
  and diagnostics for failed or stalled checking.
- Review CI contract ownership. Separate edits to laws/checkers from routine
  implementation patches; enforce this through repository/CI access controls
  where available. A policy file writable by the agent is not a security boundary.
- Select one small finite widget policy with an existing `.kry` owner, such as
  activation/release consumption. Specify activation and nonactivation behavior,
  reset and disabled states so an always-false implementation cannot satisfy it.
- Connect the checked result to production. For this bounded pilot, derive a
  truth table from the checked implementation and prove exhaustive equality with
  the actual restricted `.kry` function through a reviewed extractor/evaluator.
  Preserve `.kry` as the maintained runtime owner; do not add a second hand-kept
  implementation. Hash source, domain mappings, checker and output in evidence.
- Mutate each law-relevant branch and a generated result; verify the expected
  gate fails. Include omitted laws, stale evidence and changed enum ordering.
- Ensure published tools, libraries and downstream build paths never invoke the
  proof checker. Ship required generated artifacts with reproducibility metadata;
  runtime/device code has no Bend or Node dependency.

Acceptance: the same production policy is checked and exercised on native paths;
contract-breaking mutations fail; clean release/downstream/app probes pass without
proof tooling. A proof of a disconnected reference model is a failure.

Working checkpoint: one verified policy in an otherwise unchanged usable Kryon.
Revert the policy replacement if needed while keeping additive gates; do not
leave an alternate policy branch permanently enabled.

Evaluation: bounded cheaper-model trial on one implementation patch. Record
accepted-change cost, proof failures and review time against a stronger-model
baseline. Continue only with a usable evidence connection.

## Code guidance for implementation tasks

Existing worked example: `laws/focus/{main,LAWS,PROOF}.bend`,
`tests/focus_bend_laws_test.mjs` and `make focus-bend-laws-test`. It proves three
reference laws and exhaustively compares four inputs to native C. It does not
complete the extractor/compiler proof or C++/Go coverage promised by this phase.

This API is real and already works:

```js
import { checkLaws } from './tools/bend-laws.mjs';
const checked = await checkLaws('laws/focus/PROOF.bend');
const table = checked.table('main.tab_direction');
// Validate constructor names, exact domains, uniqueness and output mappings.
// Compare every row to freshly generated production behavior.
```

Small tasks: add one wrapper rejection test; implement resource limits in a
separate process; extend one native target comparison; then prototype extraction
of only the reviewed Boolean/enum KIR subset. The extractor must consume parsed
KIR, not regex-rewrite source. Unsupported nodes reject before proof generation.
Keep law declarations human/strong-review owned; generated models must never
silently replace the independent specification.

Mutation recipes: delete each proof, replace Forward with Stay, reverse backward
and forward, omit a domain row, reorder enum mapping, alter a source hash. Each
must fail a named gate. Do not turn compiler errors into a passing mutant result.

Run `make bend-laws-test focus-bend-laws-test`. First patch: one independently
reviewable missing-domain test or C++ comparison. Strong review is required for
the extraction/equivalence mechanism and any checker/import/pin modification.

# Plan Status

Reviewed 2026-09-19. This index distinguishes unfinished work from completed
implementation slices and deferred targets. It is not a fresh verification of
every subsystem; the linked evidence and completion gates remain authoritative.

| Area | Plans | Status |
|---|---|---|
| Overall completion | [Completion](COMPLETION.md), [ownership](OWNERSHIP_LEDGER.md) | Partial; requirement reconciliation and final evidence remain open. |
| Native language | [Language completion](NATIVE_LANGUAGE_COMPLETION.md), [slices](SLICE_VALUES.md) | Active; slices, callable values, integration and validation remain. |
| Fixed-array calls | [Array ABI](ARRAY_CALL_ABI.md) | Implemented for ordinary functions with recorded native integration checks; retained during concurrent edits and evidence reconciliation. |
| Native accessibility | [Accessibility completion](NATIVE_ACCESSIBILITY_COMPLETION.md) | Partial; Linux trees and list selection implemented, other controls and platform adapters remain. |
| Shared widget policy | [Canonical index](canonical/README.md) | Partial; shipped policy slices coexist with ownership audits, native IME and platform verification. |
| Styling | [Style index](style/00-status.md) | Partial; tooling, backend verification, migration and legacy removal remain. |
| Style ownership | [KSS services](KSS_HOST_SERVICE_LEDGER.md), [style bridges](STYLE_BRIDGE_LEDGER.md) | Maintained inventories used by guards, not disposable completed task lists. |
| Application rollout | [Consumers](DOWNSTREAM_CONSUMERS.md) | Partial; per-app migration and platform evidence remain. |
| Browser-native DOM | [DOM index](dom/00-index.md) | Deferred future target, not completed or a current native release prerequisite. |
| Website redesign | [Completion record](../docs/COMPLETION_EVIDENCE.md#website-redesign-completion-2026-09-19) | Completed plan removed; implementation and deployment evidence preserved. |

## Priority

Prioritize correctness and regressions in supported native apps, safe language
semantics needed by real callers, and focused downstream/platform verification.
Finish Linux accessibility for controls used by those apps and verify actual
keyboard/screen-reader workflows. This is usability work, not visual polish.
Do not advertise full accessibility before those workflows pass.

Windows/macOS/mobile adapters become release requirements when shipping apps
on those targets; they need native test environments. Until then they are
target-dependent roadmap work. The paused DOM target and optional tooling
conveniences should not delay current native correctness fixes.

## Removal Rule

Remove a plan only when all in-scope work is closed, its evidence and durable
contracts are preserved in documentation/tests, and inbound references are
updated. Deferred or unsupported work is not completed work. Leave files under
concurrent edits intact. In particular, the style index labels the KSS language
batch complete while its detailed plan and ownership ledger still contain
follow-ups; reconcile those before deleting that document.

# 02 Widget Policy Migration

## Goal

Move widget policy out of host C and generated Go into `.kry` runtime modules.
The host may sample input, store state, draw, and call platform services, but
widget decisions belong in `.kry`.

## Current State

Many policies already live in `.kry`: button, dropdown, scroll, tab bar,
paned view, table, modal, popup, guide, toast, profile header, text input,
reorder, drag/drop, swipe, and shared style primitives.

The remaining work is not renaming. It is removing C-owned decisions such as:

- release handling
- click activation rules
- keyboard intent mapping
- style selector facts
- geometry constants
- fallback metrics
- commit/cancel gates
- ownership lifecycle decisions

## Tasks

1. For each `src/ui/*.c` widget file, classify every remaining literal,
   branch, and helper as one of:
   - host service
   - input sampling
   - state storage
   - drawing
   - widget policy that must move to `.kry`
2. Move widget policy into the matching `runtime/*.kry` module.
3. Export small records for decisions instead of leaking host behavior into C.
4. Regenerate C/Go runtime outputs with `make generate-runtime`.
5. Add or extend focused policy tests under `tests/*_policy_test.c`.
6. Update `docs/CANONICAL_WIDGET_SURFACE.md` whenever ownership changes.

## Immediate Targets

- Finish the in-progress reorder release decision move:
  `runtime/reorder.kry`, `src/ui/reorder.c`, `tests/reorder_policy_test.c`.
- Audit remaining direct `ConsumeRelease()` callsites and decide whether they
  are policy or host side effects applying `.kry` policy.
- Audit direct `InputPointerInteractionFor(...)` use in widget C files.
- Audit raw geometry constants in shared immediate-mode helpers.

## Proof

Use targeted tests for each widget, then broader guards:

```sh
make generate-runtime
make reorder-policy-test
make ui_tk_test
go test ./... ./go/kryon/...
python3 tests/canonical_widget_surface_doc_test.py
sh tests/public_api_names_test.sh
git diff --check
```

Use the actual existing make targets where names differ.

## Done When

- Every widget policy branch has a `.kry` owner or an explicit host-service
  reason in `docs/CANONICAL_WIDGET_SURFACE.md`.
- C widget files apply `.kry` decisions instead of inventing behavior.
- Go output is generated from `.kry`, not hand-authored compatibility logic.

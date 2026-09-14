# 10 Execution Order

## Goal

Finish the canonical widget migration in small, verified commits without
drifting into downstream apps or unrelated refactors.

## Order

1. Finish and commit the in-progress reorder release-policy move.
2. Audit remaining release consumption callsites.
3. Move remaining small input lifecycle decisions into widget `.kry` modules.
4. Clean old immediate helper policy in `src/ui/ui.c` and `src/ui/ui_tk.c`.
5. Finish text editing policy classification and move any remaining decisions.
6. Finish rich text layout policy and document host rendering boundaries.
7. Finish lowered block backend parity for native, Go, and web.
8. Audit host-service boundaries and update `docs/CANONICAL_WIDGET_SURFACE.md`.
9. Run public surface and transpilation guardrails.
10. Do a completion audit against the original goal before claiming done.

## Commit Rules

- Commit directly on `master` in `/mnt/storage/Projects/kryon`.
- Do not edit downstream `vendor/kryon`.
- Keep commits small and authoritative:
  - one widget policy move
  - generated outputs
  - focused tests
  - canonical docs update
- Do not commit unrelated dirty files unless they are part of the same verified
  slice.

## Completion Audit

Before marking the overall goal complete, prove all of these:

1. Public names are canonical and prefix-free.
2. No compatibility layers remain.
3. Widget policy lives in `.kry` unless explicitly documented as host service.
4. Native, Go, and web generated paths agree.
5. The shared canonical docs are current.
6. Tests and scans cover the final state.

## Final Proof Set

```sh
git status --short
make generate-runtime
make fast-test
go test ./... ./go/kryon/...
python3 tests/canonical_widget_surface_doc_test.py
sh tests/public_api_names_test.sh
sh tests/canonical_surface_test.sh
sh tests/generated_runtime_parity_test.sh
git diff --check
```

## Done When

- The final proof set passes or any skipped command has a documented reason.
- The completion audit maps every original requirement to current evidence.

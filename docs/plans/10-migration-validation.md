# Phase 10: Migration And Validation

## Goal

Remove stale compiler assumptions, finish downstream migration, and validate the
KIR-centered architecture end to end.

## Current State

Docs, tests, and Krait integration still contain old references to the direct
compiler path and old cartridge emission flow.

## Target Design

The stable developer model is:

```text
k2ir source.kry
k2c  source.kry|source.kir
k2b  source.kry|source.kir
```

Kryon docs describe KIR as the source of truth, C as the native backend, and KRB
as the portable cartridge backend. Downstream apps call current tools directly;
there are no legacy wrappers or alias layers.

## Implementation Phases

1. Update all Kryon tests to use `k2ir`, `k2c`, and `k2b`.
2. Remove old cartridge flags and stale generated-host assumptions.
3. Update examples and Makefiles to call the current tools.
4. Update Krait's vendored Kryon pointer after root Kryon changes are committed.
5. Add end-to-end checks for `.kry -> .kir -> C` and `.kry -> .kir -> KRB`.
6. Audit docs and website text for stale compiler names and unsupported claims.

## Tests And Acceptance

- Full Kryon test suite passes.
- Krait builds against the updated vendored Kryon.
- `.kry -> C`, `.kry -> KRB`, `.kry -> .kir -> C`, and `.kry -> .kir -> KRB`
  are all covered by focused tests.
- Documentation no longer points users at removed compiler paths.

## Risks

- Removing old entrypoints before downstream repositories are updated will break
  app builds. Follow the root-repository-first workflow and update vendors after
  root Kryon lands.

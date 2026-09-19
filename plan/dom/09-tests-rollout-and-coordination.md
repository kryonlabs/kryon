# Tests, Rollout, And Coordination

Goal: ship the DOM plan through small verified slices while other agents work
on KSS and `.kry` widget policy.

## Required Gates

- `git diff --check` for every touched path.
- Parser whitelist count check when widget names change.
- `tests/public_api_names_test.sh` when runtime exports or public names change.
- Future web-native compiler/runtime/browser DOM coverage once the paused web
  target is redesigned and reactivated. Do not use `k2js` gates as current
  completion evidence.
- Focused direct runtime smokes for narrow DOM aliases.
- Browser DOM and inspector tests for mounted behavior.

## Parallel-Agent Rules

1. Work in the real Kryon repository on `master`.
2. Do not edit downstream `vendor/*` copies.
3. Read `git status --short` before each slice.
4. Ignore unrelated dirty files from KSS or widget-policy agents.
5. Commit with explicit path lists.
6. If a broad gate fails because of unrelated parallel work, record the exact
   failure and run the strongest focused checks available.
7. Do not mark the full plan complete until all plan documents and current
   implementation contracts are verified against the repo.

## Rollout Order

1. Stabilize all DOM identity and compiler metadata.
2. Finish source ranges for all DOM-producing expressions.
3. Complete the native element audit.
4. Complete attribute, state, and data synchronization.
5. Expand semantic relationships and accessibility snapshots.
6. Finish KSS selector/property integration as the language lands.
7. Expand browser-backed integration coverage.
8. Update downstream apps only by bumping `vendor/kryon` after upstream commits.

## Completion Evidence

The plan is complete only when docs, compiler output, runtime behavior,
pre-mount snapshots, mounted DOM objects, KSS traces, browser tests, and public
API guards agree on the same `.kry` to native DOM contract.

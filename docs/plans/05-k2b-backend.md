# Phase 05: k2b Backend

## Goal

Make KRB cartridge emission consume KIR instead of using a separate `.kry`
parser.

## Current State

`cmd/k2b/` exists as a standalone experiment and can emit simple KRB files, but
it duplicates parsing logic and still carries references to the old cartridge
emission path.

## Target Design

`k2b` accepts either `.kry` or `.kir`:

```sh
k2b --root . -o build/krb app.kry
k2b --root . -o build/krb app.kir
```

For `.kry`, it runs the shared frontend internally. For `.kir`, it writes KRB
directly. KRB output uses KIR source spans for inspection and KIR event/function
metadata for portable logic.

## Implementation Phases

1. Remove the private k2b parser after KIR input is available.
2. Port node, string, import, control, and state-field collection to KIR.
3. Add source-map section planning without breaking current v1 readers.
4. Update tests to call `k2b` directly, not old `kc` flags.
5. Keep raw `.krb` output deterministic for fixtures.

## Tests And Acceptance

- Current KRB cartridge tests pass through `k2b`.
- `.kry -> KRB` and `.kry -> .kir -> KRB` produce equivalent cartridges.
- Krait can load cartridges emitted by the root Kryon `k2b`.

## Risks

- KRB currently represents only part of app behavior. Do not promise full
  portable execution until logic sections exist.

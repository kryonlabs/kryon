# Phase 04: k2c Backend

## Goal

Make readable C generation consume KIR instead of parsing `.kry` directly.

## Current State

The native C path is the most complete path and must stay working. It also owns
behavior that should move behind the KIR frontend.

## Target Design

`k2c` accepts either `.kry` or `.kir`:

```sh
k2c --root . -o build/gen app.kry
k2c --root . -o build/gen app.kir
```

For `.kry`, `k2c` runs the frontend internally. For `.kir`, it loads the KIR
artifact and emits the same C. Output stays simple, readable, and compatible
with the existing C toolchain.

## Implementation Phases

1. Split existing C emission into a KIR-to-C module.
2. Keep generated symbol naming, headers, module prefixes, source inspection,
   and app hook behavior stable.
3. Add `cmd/k2c/` with `.kry` and `.kir` input detection.
4. Move the current `kc` command surface to call `k2c` internals or retire it
   after downstream build files are migrated.
5. Remove duplicated frontend parsing from C emission.

## Tests And Acceptance

- Existing syntax and generated-C tests pass.
- `.kry -> C` and `.kry -> .kir -> C` produce equivalent output.
- Downstream Krait builds still compile generated app hosts.

## Risks

- The current C backend is behaviorally important; migrate in thin slices and
  keep generated output diffs reviewable.

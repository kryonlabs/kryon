# Phase 03: k2ir Tool

## Goal

Add `k2ir`, a standalone `.kry -> .kir` tool for debugging, tests, Krait
inspection, and backend handoff.

## Current State

There is no standalone KIR artifact. Tooling must infer compiler state from
generated C, generated KRB, or ad hoc dumps.

## Target Design

`k2ir` reads one or more `.kry` files and writes deterministic `.kir` files:

```sh
k2ir --root . -o build/kir app.kry modules/ui.kry
```

The first `.kir` format should be text, line-oriented, source-mapped, and easy
to diff. A binary or compact form can come later only after the structure is
stable.

## Implementation Phases

1. Add `cmd/k2ir/` using the shared frontend and KIR dump writer.
2. Support `--root DIR`, `-o DIR`, and one or more `.kry` inputs.
3. Write output paths matching source basenames under the output directory.
4. Include source spans, symbols, state fields, functions, and imports.
5. Add `make tools` integration once the tool is useful.

## Tests And Acceptance

- `k2ir` emits deterministic output for examples.
- Re-running `k2ir` on unchanged input produces byte-identical `.kir`.
- Diagnostics match frontend errors and stop output for invalid files.

## Risks

- If `.kir` is over-specified before backend migration, every frontend change
  will churn fixtures.

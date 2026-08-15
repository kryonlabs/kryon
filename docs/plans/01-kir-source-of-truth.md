# Phase 01: KIR Source Of Truth

## Goal

Make KIR the canonical compiler representation between `.kry` source and every
backend. No backend should parse `.kry` directly once this phase is complete.

## Current State

The native compiler path and the cartridge path are split. `k2c` owns the main
`.kry -> C` behavior, while the new `k2b` path has its own parser and KRB
emitter. This duplicates language understanding and lets the two outputs drift.

## Target Design

KIR represents the resolved program:

- modules, imports, app metadata, state fields, globals, functions, and exports
- statements, expressions, widget calls, event handlers, and raw C edges
- type names and storage/ABI attributes where Kry intentionally mirrors C
- source file, line, and column spans for every load-bearing node
- capability and host-import declarations used by portable cartridges

KIR should be readable enough for debugging and stable enough for tests, but it
does not need to be optimized or clever.

## Implementation Phases

1. Define private C structs for `KirProgram`, `KirModule`, `KirFunction`,
   `KirStmt`, `KirExpr`, `KirWidget`, `KirStateField`, and `KirImport`.
2. Add ownership helpers that allocate, append, and free KIR trees explicitly.
3. Add a deterministic text dump used by tests and Krait inspection.
4. Add source-span storage to every node that can produce diagnostics or runtime
   inspector jumps.
5. Keep KIR internal until `k2ir` has stable fixtures.

## Tests And Acceptance

- A small `.kry` fixture can lower into KIR and dump deterministically.
- Source spans survive through app, state, function, and widget nodes.
- Existing `.kry` syntax tests still pass through the current compiler path
  while KIR is introduced.

## Risks

- Making KIR too abstract too early will slow migration.
- Copying old text-emitter quirks into KIR will make the representation hard to
  use for portable logic later.

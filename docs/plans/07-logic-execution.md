# Phase 07: Logic Execution

## Goal

Add portable logic to KRB without making native C the only source of behavior.

## Current State

KRB can draw a tree and call host imports, but arbitrary app logic still belongs
to generated or host C.

## Target Design

Use interpreted KIR as the first portable logic path:

- event handlers point to KIR functions
- KIR functions can read/write cartridge state
- expressions, branches, locals, calls, and returns run in the runtime
- source spans allow Krait to step and inspect execution
- host capabilities are called through the import table

Bytecode or WASM can be added later after KIR semantics are proven.

## Implementation Phases

1. Define the minimum executable KIR subset for UI event handlers.
2. Implement a small interpreter over KIR function bodies.
3. Add typed value storage for locals, arguments, and return values.
4. Add state read/write operations.
5. Add capability/import calls with strict signatures.
6. Add Krait stepping hooks after the interpreter is stable.

## Tests And Acceptance

- Button handlers can mutate cartridge state without generated C.
- Simple arithmetic, branches, and function calls work in the interpreter.
- Runtime errors report source locations.
- Missing or mistyped imports fail before execution.

## Risks

- Interpreting opaque C expressions is impossible; the frontend must gradually
  structure expressions needed by portable logic.

# Phase 02: Kry Frontend

## Goal

Replace line-oriented backend parsing with one frontend that parses `.kry`,
resolves names, records diagnostics, and emits KIR.

## Current State

The compiler accepts substantial C-close syntax, module imports, app hooks,
state blocks, globals, externs, and widget calls. Some of this is still
recognized in emitter-shaped code instead of a shared frontend.

## Target Design

The frontend owns:

- lexical scanning, logical-line handling, and brace/paren/string tracking
- module aliases and output basename metadata
- state/app/function/global/extern declarations
- statement classification and expression capture
- source-aware widget call recognition
- diagnostics that always include file, line, and column

Expressions may remain C-close text in the first frontend version, but they must
be wrapped in KIR nodes with source spans and type hints where known.

## Implementation Phases

1. Move parsing routines out of backend-specific files into a frontend module.
2. Emit KIR from the parser instead of directly filling C-emitter structs.
3. Port existing syntax diagnostics to the frontend.
4. Resolve module aliases before backend emission.
5. Add KIR fixture dumps for representative examples and failure cases.

## Tests And Acceptance

- Existing `kc_syntax_test.sh` behavior remains unchanged.
- KIR dumps cover app metadata, state, imports, functions, and widget calls.
- Syntax errors are reported before either C or KRB backend runs.

## Risks

- Changing diagnostics too early may break current tests.
- Treating expressions as opaque forever would block portable logic, so the
  frontend should preserve enough structure to refine them later.

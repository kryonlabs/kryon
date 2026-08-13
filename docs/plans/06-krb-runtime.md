# Phase 06: KRB Runtime

## Goal

Grow the KRB runtime from a render-first loader into a portable cartridge host.

## Current State

The runtime can load KRB bytes, read nodes/strings/imports/controls, mount C
state fields, bind host functions, execute a tiny program, and draw through
`KryBackend`.

## Target Design

The runtime should support:

- strict header and section validation
- node rendering through backend-independent primitives
- typed state schema and state reads/writes
- source maps for inspector and diagnostics
- event routing from nodes to logic functions or host imports
- capability lookup and optional/required import reporting
- asset lookup through runtime files or embedded cartridge assets

## Implementation Phases

1. Harden KRB validation around bounds, alignment, counts, and string offsets.
2. Add section identifiers or versioned extension layout for new data.
3. Add source-map section support.
4. Add state schema section support independent of mounted C structs.
5. Add event dispatch hooks that can call interpreted KIR logic first, then host
   imports when declared.

## Tests And Acceptance

- Malformed cartridges fail with clear load errors.
- Render-only v1 cartridges continue to draw.
- Source maps can map a drawn node back to `.kry` file and line.
- Missing required imports produce deterministic diagnostics.

## Risks

- Expanding the format without version discipline will make renderers fragile.

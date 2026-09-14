# 06 Lowered Block Backends

## Goal

Make canonical `.kry` block syntax work consistently across native, Go, and web
without exposing lowered host helpers as public widgets.

## Current State

Canonical block concepts include:

- `Scroll`
- `Popup`
- `Disabled`
- `TableCell`
- `Canvas`
- composed content blocks

Several scroll policies already route through `runtime/scroll.kry`, including
scope geometry, wheel, content drag, thumb drag, scrollbar drag/release, and
ensure-visible behavior.

## Tasks

1. Audit lowered block generation in C, Go, and web outputs.
2. Keep host begin/end helpers private to generated code.
3. Ensure generated backends do not export compatibility block helpers.
4. Move remaining block geometry and lifecycle decisions into matching
   `runtime/*.kry` modules.
5. Add parity tests for native, Go, and web behavior.
6. Update snapshots only after proving the canonical behavior.

## Proof

```sh
sh tests/generated_runtime_parity_test.sh
sh tests/k2go_syntax_test.sh
sh tests/k2js_syntax_test.sh
sh tests/public_api_names_test.sh
python3 tests/canonical_widget_surface_doc_test.py
```

## Done When

- Public users write canonical block syntax only.
- Lowered helpers are invisible outside generated backend internals.
- Native, Go, and web agree on behavior for block scopes.

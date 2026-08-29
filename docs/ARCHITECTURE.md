# Kryon Architecture

Kryon is organized as a reusable runtime with a small set of public headers,
backend implementations, format/tooling paths, and conformance tests. The
runtime should expose primitives; downstream applications compose those
primitives into product behavior.

## Public API

Public headers live in `include/`. They define the app-facing Kryon surface:
window/runtime compatibility, UI widgets, platform services, formats, update,
sync, filesystem, desktop integration, and optional terminal primitives.

Public APIs should be named after the real domain concept. Do not add temporary
prefixes, compatibility aliases, or product-flavored names for new behavior.

## Runtime Implementation

Runtime code lives in `src/`. It owns widget behavior, rendering helpers,
platform adapters, serialization, sync/update primitives, and backend-specific
translation. Internal helpers should stay private to `src/` unless an unrelated
downstream application demonstrably needs the behavior.

## Backends

Backend selection is controlled by `KRYON_BACKEND`. The default native backend
uses raylib through Kryon's compatibility surface. Other backends are expected
to implement Kryon behavior through the same public/runtime contracts, with
their support documented in `docs/BACKENDS.md` and `docs/FEATURE_MATRIX.md`.

## Formats And Tools

The `.kry`, KIR, generated C, generated Go, and KRB paths are Kryon-owned
tooling surfaces. Tool changes should update the matching specs, conformance
matrix data, examples or fixtures, and generated documentation when applicable.

## Examples And Fixtures

Examples under `examples/` should remain generic and readable. They demonstrate
Kryon capabilities, not downstream product screens. Exact rendering and parity
fixtures should be generic enough that they can run in any app-independent
Kryon build.

## Tests And Matrices

Kryon uses several test layers:

- boundary and naming checks for repository hygiene
- public API snapshot checks for app-facing identifier drift
- public header compile checks for app-facing include hygiene
- examples manifest checks for example inventory and exactness fixtures
- generated-file checks for docs, compatibility headers, icons, and matrices
- parser, runtime, sync, update, platform, and widget tests
- conformance and visual matrix checks across renderers and runtime paths

Use `make preflight` before committing focused Kryon changes. Use `make test`
for the broader local regression suite.

## Downstream Integration

Downstream applications vendor Kryon as a submodule. Permanent Kryon changes
must be made and committed in this repository first, then brought into apps by
updating the submodule pointer. Never edit a downstream `vendor/kryon` tree as
the source of a Kryon change.

# Kryon Boundaries

Kryon is a reusable UI runtime. It may contain generic runtime primitives,
generic examples, generic test fixtures, and documentation for integrating the
runtime into downstream applications. It must not contain product behavior,
branding, copy, assets, or fixtures from a downstream application.

## Belongs In Kryon

- reusable widgets, layout, text input, focus, theme, DPI, modal, scroll, and
  input-capture behavior
- reusable renderer, backend, format, preview, packaging, update, sync, and
  desktop primitives
- generic examples that demonstrate one Kryon feature at a time
- generic tests and fixtures that assert Kryon behavior across backends
- documentation for Kryon APIs, formats, backends, and downstream update flow

## Belongs In Applications

- product screens, routes, onboarding, workflows, and state machines
- product names, screenshots, icons, store metadata, app IDs, and domains
- app-specific copy, locale keys, settings semantics, and persistence policy
- business rules for sessions, habits, documents, projects, accounts, or other
  product concepts
- downstream integration code that only one application needs

## Forbidden Downstream Terms

`make kryon-boundary-check` rejects known downstream product material. The hard
forbidden list lives in `tools/check-kryon-boundaries.sh`, which is excluded
from its own content scan so the banned terms do not appear elsewhere in this
repository. When another downstream product leaks into Kryon, remove the
material from Kryon and add that product term to the checker in the same
change.

## Runtime Primitive Test

Before adding a public API or example, ask whether another unrelated
application could use it without inheriting product assumptions. If the answer
is no, keep it in the downstream app. If the answer is yes, name it after the
domain concept directly and cover it with Kryon-owned tests.

## Generated And Showcase Material

Generated files should describe their generator and source inputs. Do not hand
edit generated outputs except to repair or replace the generator in the same
change.

Showcase material may reference external users of Kryon only when it is clearly
documentation about adoption, not an input fixture, public API example, or
runtime asset. Product examples used for rendering, parity, screenshots, or
coverage belong in the product repository.

Keep `examples/manifest.json` current when adding, removing, or renaming
examples. The manifest is the stable inventory for example metadata and curated
render-exact fixtures.

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

Instance storage is a host primitive: it allocates typed state by key and
releases it with its render host. Widget declarations own the record shape and
its updates. Generated runtime methods may bind to their owning host; generated
application functions use the active host API without an injected runtime
parameter. Product-specific state remains in application declarations.

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

Widget props contracts belong in the declaration source. Button's C header and
native Go props type are generated from `runtime/button_props.kry`; handwritten
host code consumes these types. Geometry and texture fields are declared in
`runtime/drawing_props.kry`. Its `struct #extern` contracts reuse C/C++ host
definitions and generate native Go definitions; graphics resource ownership
remains with the host. Control style records use their own shared declaration.
Button's measurement consumes these actual props and style records directly;
hosts supply font measurements and available space, while `.kry` owns bounds
and shape decisions. Borrowed label/typeface fields retain host-owned storage;
shared code does not turn length-aware string views into C pointers.
Runtime-generated Go binds host services to the owning runtime receiver.
Generic `#extern` calls propagate this dependency through shared helpers, while
direct Go package imports stay stateless. Application host bridges remain
separate from runtime implementation; no runtime-wide mutable service setter is
generated. MeasureTextWidth supplies font-specific measurement without making
widget size decisions and restores the C host's previous typeface after use.
ReadActivation supplies pointer and keyboard samples using the host's focus and
popup ownership rules. `runtime/input_props.kry` owns the sample contract.
Button interprets that sample in `.kry`, including disabled/loading gating and
explicit visual states. Deferred painting resolves its stored sample without
polling the host again. Its retained motion consumes that same resolved input.
Button content selection and placement also belong to `.kry`. The paint module
defines Drawing commands for text, icons, textures, rings, and chevrons. Native
hosts rasterize commands; they do not choose which content a Button displays.
Font measurement, font resource lookup, clipping, and pixel blending remain
device services. C supports texture commands; the Go frame stream still lacks
texture resource rendering.

The compiler's embedded runtime contracts contain `.kry` source text, not a second
parsed schema or handwritten field table. KIR parses embedded and file sources
through the same frontend. Runtime types are a fallback after lexical and
explicitly imported application types.

Control style records and constants belong to `runtime/control_props.kry`. Native
hosts consume generated C/Go interfaces and the web host re-exports generated
JavaScript constants. Backends must not add independent control enum or field
tables; the compiler reads this contract through its embedded declaration source.

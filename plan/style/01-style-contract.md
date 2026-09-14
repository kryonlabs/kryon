# 01 - Style Contract

Status: implementation plan
Scope: define the canonical contract between `.kry`, `.kss`, runtime style data, and backends.

## Objective

Make Kryon styling a single explicit path:

- `.kry` declares structure, semantic facts, state, behavior, measurement, and widget policy.
- `.kss` declares visual values, tokens, selectors, states, variants, and style packs.
- The resolver in runtime `.kry` lowers style rules into deterministic `StyleData`.
- C, Go, JS, KRB, DOM, canvas, libdraw, termi, and other backends consume the same resolved frames.
- Widgets render with no hidden product chrome when no style pack is active.

The contract must be strong enough that a widget can be inspected and tested in `KRYON_STYLE=none`, then styled by attaching a pack without changing the widget implementation.

## Vocabulary

Use these public names:

- `StyleSheet`: compiled rule table.
- `StylePack`: named set of style sheets and metadata.
- `StyleRule`: selector plus declarations.
- `StyleToken`: named value in a pack.
- `StyleFacts`: semantic selector facts emitted by widgets.
- `StyleData`: resolved visual data.
- `StyleFrame`: resolved `StyleData` plus derived paint metadata.
- `StylePicker`: standard dropdown control for switching active packs.

Use `kss_` names only for internals that truly parse or format `.kss` text:

- `kss_parser.c`
- parser diagnostics
- syntax tests
- formatter/import loader code

Do not add user-facing APIs named `KssButton`, `KssStyle`, `KssPack`, or similar. The runtime API is style, not parser syntax.

## Core Invariant

No widget may supply decorative defaults outside a style sheet.

Allowed without an active style pack:

- widget existence
- input handling
- focus handling
- accessibility metadata
- layout identity
- measurable structural minima needed for behavior
- text fallback only when needed for diagnostics or accessibility

Not allowed without an active style pack:

- product colors
- decorative border colors
- default material/glass/lightfield chrome
- radius defaults that create visible shape
- opacity defaults that make hidden style visible
- shadows, glows, highlights, gradients, or outlines
- theme fallback colors for app-facing widgets

If no style pack is active and a widget emits a visible rectangle, that is a bug unless the rectangle is a debug/test overlay explicitly requested by the caller.

## Canonical Resolution Flow

Every app-facing widget must follow this flow:

1. Build structural props from the `.kry` call.
2. Convert state into semantic facts in `.kry`.
3. Resolve style from the active style pack with a zero visual base.
4. Derive metrics from resolved style fields, preserving explicit zero.
5. Run behavior/input policy.
6. Emit backend paint commands using only resolved style values.

Example shape:

```c
StyleFrame frame = {0};
frame.value = ResolveActiveStyle((StyleData){0},
    ButtonStyleFactsFor(props, state),
    state);
metrics = ButtonMetricsFor(scale, frame);
paint = ButtonPaintFor(props, metrics, frame);
```

The preferred end state is that `ButtonStyleFactsFor`, `ButtonMetricsFor`, and `ButtonPaintFor` are all generated from `.kry`.

## Style Facts

Every widget should emit facts using typed helpers in `.kry`, not open-coded C or Go calls.

Required fields:

- `kind`: widget style kind, such as `StyleKindButton()`.
- `role`: subpart role, or `StyleAny()`.
- `name`: stable per-node style id when needed.
- `class_name`: hashed class selector.
- `tone`: neutral, accent, danger, etc.
- `emphasis`: filled, soft, outline, ghost, etc.
- `size`: small, medium, large, etc.
- `state`: normal, hover, pressed, focus, disabled, selected, loading.
- optional axes as they land: validation, orientation, placement.

Rules:

- Facts are semantic, not visual.
- Do not encode color choices into facts.
- Do not encode pack-specific concepts into facts.
- Do not use class names as a replacement for built-in semantic roles.

## Zero Base

The style resolver should receive a zero visual base for app-facing widgets:

```c
ResolveActiveStyle((StyleData){0}, facts, active_state)
```

Temporary exceptions must be documented in the relevant migration file and covered by tests. An exception is allowed only for structural behavior, not decoration.

Examples of suspicious bases to remove:

```c
StyleData base = {
    .fields = StyleOpacity | StyleFontSize | StyleMaterial,
    .opacity = 1,
    .font_size = 16,
    .material = MaterialFlat,
};
```

That kind of base makes no-style mode visible and must move into `.kss` packs.

## Metrics Contract

Metrics can have structural fallbacks, but those fallbacks must be non-decorative and must preserve explicit zero.

Allowed metric fallback:

```kry
if (fields & StylePaddingX) == 0 || value < 0 {
    value = fallback
}
```

Required behavior:

- missing value may use a structural fallback;
- explicit zero must remain zero;
- negative values can clamp or fallback according to the widget policy;
- visual values such as radius, border, color, material, and opacity should not grow visible defaults.

## File Placement

Put shared policy in `runtime/*.kry`.

Use C/Go only to:

- call generated helpers;
- pass host/backend data;
- measure actual text when required;
- emit backend drawing commands;
- integrate platform input.

Do not add new style policy to:

- `src/ui/ui.c`
- `src/ui/ui_tk.c`
- backend-specific renderers
- Go retained runtime helpers

unless the code is a temporary bridge with a migration note.

## Done Criteria

This contract is implemented when:

- every app-facing widget uses `.kry` facts helpers;
- every app-facing widget resolves from zero visual base;
- no-style tests exist for key widget families;
- style pack tests prove all built-in packs cover the shipped widget surface;
- no public widget prop carries visual chrome;
- C and Go retained paths produce matching style facts and frames.

# Phase 7 — Layout, KSS and visual-property laws

Status: first patch registered 2026-09-19: explicit-zero precedence is inventory law
`style.explicit-zero.precedence` (existing `style_widget_policy_test` coverage).
KSS parser/formatter proof connections and layout law owners remain open.
Estimate: **10–20 focused engineer-days; 500–1,200 thousand model tokens**.
Assumptions and shared gates: [plan index](README.md).

Entry: interaction/lifecycle contracts establish ownership and retained identity.
Reuse shared KSS parser/formatter/cascade work; do not recreate it in Bend.

Work:

- Specify constraints, content boxes, child placement, scrolling, clipping,
  stacking, grid/row/column distribution, padding/borders and transforms.
  Define finite/nonnegative geometry and overflow behavior where appropriate;
  intentional offscreen positioning must remain legal.
- State deterministic layout and idempotence under unchanged measurement inputs.
  Specify measurement assumptions, floating-point tolerances and observable
  rounding. Prove exact policy properties where possible; label tolerance-based
  image/geometry comparisons as tests.
- Cover KSS token/value/selector contracts, specificity, ordering, presence,
  imports, themes, variants, explicit zero, transparency and no-style content.
  Include parser/formatter round trips and deterministic diagnostics.
- Establish laws for theme overlays and atomic failure recovery; coordinate with
  maintained theme migrations rather than preserving obsolete getters forever.
- Connect shared `.kry` layout/style functions to production proofs. Keep host
  storage, fonts, asset loading and rasterization behind explicit service contracts.
- Validate every supported renderer's degradation behavior and capability
  declarations. Unsupported effects must be reported without changing unrelated
  colors or hiding semantic content.

Acceptance: every layout/style primitive used by supported widgets has a law
owner; pure-policy proofs connect to implementation; renderer/measurement adapters
have bounded contracts and measured integration evidence.

Working checkpoint: one layout or style family at a time, with reproducible
virtual-display captures and theme-switching regressions on affected apps.
Maintain functioning default/no-style and authored-style paths throughout.

Evaluation: inspect both resolved values and actual rendering. A green cascade
proof cannot close incorrect pixels or a missing image.

## Code guidance for implementation tasks

Start with `runtime/layout.kry`, `style.kry`, `style_sheet.kry`,
`kss_parser.kry`, `kss_formatter.kry`, `tests/laws/layout_laws_test.c`,
`tests/style_sheet_policy_test.c` and `plan/STYLE_BRIDGE_LEDGER.md`.
Existing functions include `LayoutMetricsFor`, `LayoutChildBounds` and
`StyleOpacityValue`; inspect their actual domain before declaring a new law.

Example property sketch (not an unconditional law):

```text
finite(bounds, padding) AND padding >= 0 AND declared_domain(bounds)
  => finite(layout(bounds, padding)) AND content_extent >= 0
resolve(explicit_zero, inherited_nonzero) == explicit_zero
format(parse(format(parse(text)))) == format(parse(text))
```

Do not use a nonnegative-coordinate law: valid content can lie outside the view.
Keep content size, clipping and screen placement distinct. State float/NaN rules
and exact-versus-tolerance observations. An explicit zero is a present value,
not absence; preserve presence bits through target mappings.

Small tasks: one geometric rule; one KSS precedence rule; parser/formatter model
connection; renderer adapter trace; then matched captures. Add contradictory
specificity, explicit transparency, no-style text, nested clips and theme failure
fixtures. Never repair a test by overwriting expected pixels without explaining
the contract change.

Run `make kss-parser-test kss-matched-test kss-formatter-test`,
`make style-sheet-policy-test style-pack-source-test` and affected layout laws.
First patch: explicit-zero precedence law with a production connection. KSS
syntax expansion and numeric tolerance definitions require separate review.

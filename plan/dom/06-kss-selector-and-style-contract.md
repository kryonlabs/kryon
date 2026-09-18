# KSS Selector And Style Contract

Goal: KSS styles Kry DOM facts and exports native CSS without generated JS
hand-building style classes.

## Contract

KSS resolves against `webNodeStyleFacts(node)`. The DOM renderer then exposes
matching facts through native annotations such as `data-kry-kind`,
`data-kry-path`, `data-kry-name`, `data-kry-key`, `data-kry-state`, classes,
data attrs, ARIA attrs, and native attrs.

## Steps

1. Keep selectors fact-based, not generated-JS-class based.
2. Ensure every new DOM fact needed by selectors is present in
   `webNodeStyleFacts(...)`.
3. Export CSS selectors that target Kry annotations and native browser
   attributes/pseudos where possible.
4. Keep runtime style resolution and CSS export behavior aligned.
5. Add a focused KSS property or selector test whenever the KSS language grows.
6. Preserve normal CSS features that KSS intentionally supports: keyframes,
   conditional groups, structural selectors, state selectors, and native
   browser pseudo mappings.

## KSS Handoff Notes

- KSS agents should target DOM facts, not direct renderer internals.
- DOM agents should expose facts first, then style support can consume them.
- If a property only exists in browser CSS, keep it in the web CSS contract,
  implemented by shared `.kry` policy consumed by the export adapter. Browser-only
  availability is not a reason to hand-write its language decisions in JavaScript.
- If a property affects native backends too, define its Kry/KSS meaning before
  adding browser-only behavior.

## Evidence

- `parseWebStyleSheet(...)`, `resolveWebStyle(...)`, `traceWebStyle(...)`, and
  `webStyleSheetToCSS(...)` agree on selectors and declarations.
- Mounted `webDOMStyleFacts(...)` and `webDOMStyleTrace(...)` match pre-mount
  style facts and traces.
- Browser tests show installed KSS affects real DOM nodes.

Current progress and remaining divergences are recorded in
[the completion evidence ledger](../../docs/COMPLETION_EVIDENCE.md). Shared
`.kry` now owns ordinary-rule/keyframe declaration expansion and effect recipes.
Inline precedence and mounted-fact alignment remain required before closing
steps 2–4; passing export tests alone does not prove the whole contract.

# Compiler Metadata

Goal: the future web-native compiler emits DOM metadata from `.kry` syntax
instead of letting the runtime guess structure later. The old `k2js` path is
paused and is not a current release target.

## Scope

Compiler metadata applies to:

- named UI blocks, such as `Button save: { ... }`;
- anonymous widget statements;
- widget expressions in declarations, assignments, returns, conditions, and
  guards;
- parenthesized single-widget expressions;
- browser-native alias blocks;
- lexical UI scopes such as `Scroll`, `Canvas`, `TableCell`, `Disabled`, and
  `Popup`;
- declared `.kry` component calls whose children must remap into call-site
  structure.

## Steps

1. Keep `parse_widget_statement(...)` as the front-door whitelist for
   DOM-producing calls.
2. Keep `is_web_native_block_widget(...)` aligned with every native browser
   alias.
3. Emit metadata at the KIR statement or expression that owns the source token.
4. Include `nodeName`, `path`, `parentPath`, `key`, source line/column, and
   source end line/column when available.
5. For composed widgets, remap child paths from definition space into call-site
   space.
6. Add compiler contract tests for every new expression form before relying on
   runtime fallback.

## Done When

- Future generated web output can be inspected and shows metadata attached to
  DOM-producing runtime calls.
- No supported expression-backed DOM node depends on index-only fallback when
  the compiler has enough source information.
- A failing compiler metadata path fails a focused test, not only a browser
  smoke test.

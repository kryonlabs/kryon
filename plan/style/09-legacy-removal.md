# Styling debt: remaining removals

- Remove the independent active C and Go KSS implementations identified in
  `02-kss-language.md`, including the color-token variant logic added in
  `16510c75`. Switch their consumers to generated `.kry` code before deletion;
  do not preserve a second parser behind an alias, adapter, or fallback.

- Inventory remaining theme-catalog/getter and import/export callers in core,
  examples, and apps. Move widget styling to KSS overlays after language and
  downstream support exists, then delete unused loaders and bridges. Keep real
  platform preference services distinct from widget chrome.
- Classify remaining visual-property ratchet entries by semantic content versus
  product decoration. Migrate deprecated decoration to KSS before deleting its
  fields; preserve the canonical Text/Image content props required by the API.
- Remove remaining renderer defaults through `05-runtime-and-backends.md` and
  shrink the corresponding scanner allowances. Do not retain forwarding aliases
  or add another styling surface to conceal old callers.
- Audit documentation/examples for stale theme-first guidance, removed pack IDs,
  obsolete syntax, and old default-style assumptions; update maintained guidance.
  Keep negative tests for rejected syntax, and label design proposals clearly.
- For every remaining bridge, record file, owner, maintained callers, deletion
  condition, and replacement test. Delete it once the callers have migrated.

Completion requires no unclassified styling bridge or deprecated maintained
caller, and gates that prevent their reintroduction. Passing public-name checks
alone does not establish this.

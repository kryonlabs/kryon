# Styling debt: remaining removals

- Complete: C and Go use the generated `.kry` KSS parser and cascade. The
  independent parser removal is covered by `kss-host-ledger-check`.
- Complete: remove the constant `ui_default_style` switch, unreachable legacy
  radio/progress/text-input/tab branches, unused default surface/focus helpers,
  and theme getter snippets from the recorder.

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
  Status: `plan/STYLE_BRIDGE_LEDGER.md` records the current ratchet-allowed
  C/Go/style-data bridges and `scripts/bridge-ledger-check.py` fails if those
  rows drift from the active scanner hits. Downstream app bridge rows and any
  future import/export bridge findings still need to be added as their scans
  are completed.

Completion requires no unclassified styling bridge or deprecated maintained
caller, and gates that prevent their reintroduction. Passing public-name checks
alone does not establish this.

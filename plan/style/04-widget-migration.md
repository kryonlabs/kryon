# Widget styling: remaining work

- Inventory handwritten metrics/paint decisions still in `src/ui/` and
  `go/kryon/runtime.go`; map each to a widget and source location. Move shared
  decisions into the owning `runtime/*.kry` policy, retaining actual host font
  measurement and draw-command emission in their backends.
- Resolve the remaining getter/base allowlist entries in
  `scripts/check-style-gates.py`: theme capture in `src/ui/ui.c`, app background
  in `src/ui/ui_tree.c`, and caller font overrides in `go/kryon/runtime.go`.
  Classify structural/content allowances separately from product decoration;
  remove obsolete entries after migration.
  Status: every remaining entry is now classified in the script itself.
  `src/ui/ui.c` (9) is the legacy theme catalog application pending downstream
  migration off the pre-KSS theme API - its removal is tracked by
  `09-legacy-removal.md`. `src/ui/ui_tree.c` (1) is the documented
  no-pack fallback to the platform theme color. `go/kryon/runtime.go` (4) and
  `src/ui/ui_page.c` (2) are caller-supplied content metrics under the
  documented metrics contract. No unclassified product decoration remains in
  these lists; counts keep ratcheting down with the migrations above.
- Audit every widget subpart against its emitted role/state facts and KSS
  declarations. Add only missing coverage; move any newly discovered decorative
  defaults into the packs without losing explicit zeros.
- For each residual migration, verify shared policy, no-style behavior, and
  native/retained rendering of the affected widget. Coordinate overlapping
  canonical-policy work through its existing plan rather than duplicating it.

Completion evidence: a source ownership inventory with no unclassified
handwritten visual policy, plus focused checks for each changed path.

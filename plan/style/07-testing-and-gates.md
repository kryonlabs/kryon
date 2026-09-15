# Style verification: remaining gaps

- Add ownership/provenance checks proving that KSS parsing and semantic logic
  come from maintained `.kry` source. Fail on handwritten parser, token, overlay,
  variant, or cascade implementations in host languages. Classify minimal I/O
  and rendering services explicitly; wrappers around old parsers do not pass.
- Test the shared parser directly, then run its generated targets on identical
  valid/invalid inputs and compare diagnostics and resolved results. Preserve
  existing consumer behavior while deleting the independent implementations.

- Map requirements to existing tests and record untested widget-role-state and
  backend combinations. Passing fixture subsets do not establish full coverage.
- Extend parser/resolver parity for imports, overlays, options, provenance,
  new typed values, and explicit-zero combinations as those features land.
- Complete matched no-style/content-preservation coverage across native,
  retained, web, KRB, and constrained backends; audit current tests first.
- Extend visual checks to missing widget families, light/dark color variations,
  contrast, and backend degradation. Verify hot reload and compiled style data.
- Shrink classified getter/base/visual-property ratchets in
  `scripts/check-style-gates.py` as their callers migrate. Keep justified
  structural/content cases explicit; a passing allowlist is not zero debt.
- Wire newly added coverage into the relevant build/CI gates, with required
  runtimes failing visibly when unavailable rather than being reported as tested.

For each missing requirement, record its owning implementation, test, supported
backends, and failure/degradation behavior. Remove the entry once verified.

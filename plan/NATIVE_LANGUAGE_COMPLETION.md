# Native language and runtime completion

Requested 2026-09-19. JavaScript remains a future target. Shared behavior belongs
in `.kry`; resource storage, Unicode/font services, rasterization, and OS protocol
adapters remain native. Work is performed on upstream master.

## Acceptance checklist

- [ ] Compiler: validate portable aggregate, array, slice, and callable types;
  replace silent Go `any`/comment fallbacks with source diagnostics; exercise
  supported cases and rejected cases across native targets.
- [ ] Styling: retire obsolete theme bridges and widget appearance defaults
  after migrating maintained callers; keep explicit content metrics and genuine
  platform fallbacks documented in the bridge ledger.
- [x] Editable text: benchmark actual row measurement and painting, bound layout
  reuse, preserve Unicode/selection/wrapping behavior, and record measurements.
- [x] Native Go: render asset images with canonical ImageProps; verify existing
  OS IME integration; support useful text-range selection with shared policy.
- [x] Host organization: split the large Go host into focused modules without
  duplicating policy or changing generated API ownership.
- [ ] Validation: native generated-output guards, C/Go/C++ parity, appropriate
  native tests on a virtual display, and current API/architecture/boundary docs.

## Existing functionality to preserve

KSS parsing/cascade already comes from `.kry`. Native Go already implements an
IBus bridge and bounded read-only text and surface raster caches. Reuse these;
do not introduce alternate parsers, image APIs, or compatibility aliases.

Website and accessibility action work is underway independently in this shared
checkout and is outside this change's ownership.

## Completed batches

- Compiler diagnostics fail closed in native Go. Strict aggregate checks now
  cover malformed shapes, borrowed-slot storage, integer-only indices and
  debug bounds checking on array writes. General slice/callable ownership and
  array values remain part of the language work, not completed by diagnostics.
- Removed obsolete style branches, recorder theme snippets and unused palette
  setters/globals. KSS presence reaches native rasterization without debug
  widget chrome; shared `.kry` policy owns text content fallback.
- Native input, image resources, measured row reuse and host organization are
  committed. Linux input verification runs on a private Xvfb/IBus session.
- Theme role derivation now comes from `.kry`, with shared C/C++/Go fixtures.
  Removed the duplicate C/Go tone and contrast implementations and overwritten
  metric defaults. Live theme catalog callers still require migration.
- Local fixed arrays now have native value semantics: zero initialization,
  positional literals, copies, conditional selection and borrowed callback
  captures. Execution fixtures cover ordering, alias isolation, nested record
  contents and bounds traps on C, C++ and Go.
- Named array bounds normalize across literals, copies and imported record
  fields. The checker rejects invalid sizes and arithmetic overflow before
  target emission. Go constant references and bound names use their emitted
  names consistently.

## Remaining implementation order

1. Complete the direct array parameter/return ABI across strict C, C++ and Go.
   Local construction, copying and checked constant bounds are implemented;
   arrays can still cross function boundaries inside records. A diagnostic for
   an unsupported direct array parameter does not complete its implementation.
2. Define slice bounds, mutation and storage lifetime before accepting slices
   as portable values. Cover valid operations, out-of-bounds access and escaping
   borrowed storage across the three native targets.
3. Extend callable types beyond the existing synchronous borrowed `#slot`
   contract. Specify return values, captured storage and lifetime first; keep
   rejected escape cases explicit until the ownership contract is implemented.
4. Migrate maintained Uku, Krait and Rill theme catalog callers to KSS, making
   reusable changes upstream first and updating only downstream submodule
   pointers. Remove `ApplyCurrentTheme` and its remaining global palette only
   after those callers and Kryon's own initialization stop relying on them.
   Structural font/layout fallbacks and actual OS services remain documented
   exceptions, not appearance policy to duplicate in hosts.
5. Run the native compiler fixtures, generated-runtime parity and applicable
   virtual-display tests after those migrations, then close the acceptance
   checklist. Per-batch checks already pass; this final pass is still pending.

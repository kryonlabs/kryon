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

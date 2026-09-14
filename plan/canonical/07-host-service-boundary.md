# 07 Host Service Boundary

## Goal

Draw a hard line between widget policy that belongs in `.kry` and host services
that should remain native.

## Host Services That May Stay Native

- image cache, loading, decoding, and texture upload
- icon sheet/type lookup
- URL dispatch
- clipboard and platform text services
- font measurement and glyph atlas rendering
- focus registration storage
- paint layers and clipping stacks
- pointer owner storage where retained state is required
- platform windows and OS integration
- terminal PTY/session IO and ANSI parsing
- Game2D physics/audio handles and scene rendering

## Widget Policy That Should Move To `.kry`

- activation and release decisions
- keyboard intent
- default metrics and fallback constants
- layout geometry
- style selector facts and role decisions
- open/close/commit/cancel rules
- row, cell, header, and option selection policy
- scroll, drag, resize, reorder, swipe, and modal lifecycles

## Tasks

1. Add explicit host-service notes to `docs/CANONICAL_WIDGET_SURFACE.md`.
2. When a C branch remains, document why it is host support.
3. When a branch has no host-service reason, move it to `.kry`.
4. Keep Kapsule/app-specific logic out of Kryon.
5. Keep downstream `vendor/kryon` pristine; all changes happen in
   `/mnt/storage/Projects/kryon`.

## Proof

```sh
git status --short
rg -n 'host support|native support|remain native|remain host' docs/CANONICAL_WIDGET_SURFACE.md
sh tests/public_api_names_test.sh
```

## Done When

- Every remaining native widget path is either gone or clearly justified as a
  host service.
- The boundary is understandable without reading the whole C implementation.

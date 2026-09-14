# 05 Rich Text And Drawing

## Goal

Finish separating rich text and drawing policy from host rendering.

## Current State

`.kry` owns many text and primitive policies:

- `Text` style resolution and centered placement
- selectable range/pointer/drag/copy/show decisions
- selection highlight geometry
- paragraph metrics, line gap, line stride, alignment, and selectable offsets
- primitive geometry for `Box`, `Line`, `Circle`, `Ring`, `Triangle`, and
  `Bevel`
- `Icon` bounds/size policy
- `Image` fit and placeholder layout policy

Native support still owns parsing, line-break arrays, icon shaping, atlas
lookup, actual drawing, caches, and platform font services.

## Tasks

1. Audit `src/ui/ui_text.c`, `src/ui/ui_text_layout.c`, and paragraph paths.
2. Move any remaining layout decisions into `runtime/text.kry` or
   `runtime/paragraph.kry`.
3. Keep glyph atlas drawing, font lookup, and text measurement native.
4. Keep low-level `DrawTexture*` calls only inside backend/drawing internals,
   not maintained app `.kry` UI.
5. Preserve `Image(ImageProps)` as the semantic image surface.
6. Add policy tests for every moved text or drawing rule.

## Proof

```sh
make paragraph-policy-test
make text-policy-test
make image-policy-test
make icon-policy-test
sh tests/public_api_names_test.sh
```

Optional scan:

```sh
rg -n '\b(Texture|DrawTexture|DrawTexturePro|DrawTextureRec)\s*\(' src/ui tests docs --glob '!build/**'
```

Remaining hits must be backend/drawing internals or tests, not public widget
surface.

## Done When

- Text/rich-text layout policy is `.kry`.
- Host code owns only parsing/storage/rendering services that `.kry` cannot
  safely provide yet.
- Public docs and examples show `Image`, not texture-era widget names.

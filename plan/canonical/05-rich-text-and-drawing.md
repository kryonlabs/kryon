# Remaining rich text and drawing audit

Text and Paragraph already own selectable-range decisions, highlight geometry,
line metrics/spacing/alignment, and line-selection policy in `.kry`. Primitive,
Icon, and Image geometry policy is present. `Image(ImageProps)` is canonical.

Remaining:

- Classify reflow and line-break decisions in `src/ui/ui_text_layout.c` and
  `src/ui/ui_text.c`; distinguish measurement and array ownership from the
  decision of where and how text wraps.
- Audit selectable paragraph ownership and retained text placement against
  generated text/paragraph policy, including Go and web equivalents.
- Add matched tests for wrap boundaries, empty lines, alignment, selection
  crossing lines, and text/image content inside clipped scopes.

Glyph lookup, atlas drawing, image decoding/upload/cache, and actual font
measurement remain host services. Their presence alone is not migration debt.

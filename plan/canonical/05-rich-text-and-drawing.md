# Remaining rich text and drawing audit

Text and Paragraph already own selectable-range decisions, highlight geometry,
line metrics/spacing/alignment, and line-selection policy in `.kry`. Primitive,
Icon, and Image geometry policy is present. `Image(ImageProps)` is canonical.

Completed read-only reflow slice:

- `runtime/paragraph.kry` owns borrowed token ranges, whitespace/newline rules,
  wrap decisions, empty-line preservation, and line ranges/widths. C's old
  handwritten tokenizer and `ParagraphLineStepFor`, and Go's
  `wrapRuntimeTextMeasured`, are removed.
- C `ui_text_layout.c` and Go `text_layout.go` supply storage and font metrics;
  native Text/paragraph rendering and Go Text/Paragraph/ParagraphText use them.
- `make paragraph-policy-test` executes generated C/C++ policy and the C adapter;
  `TestParagraphLayoutFixtures` runs the same 16 fixtures in Go. Cases include
  exact-fit and oversized words, empty/trailing lines, CRLF, Unicode, nonbreaking
  spaces, shaped candidates, and zero-width content. Go integration covers
  clipping, alignment, paragraph line gaps, and the final Y position.

Remaining:

- Classify editable TextArea visual-row traversal in `src/ui/ui.c` and remaining
  selectable-block decisions in `src/ui/ui_text.c`.
- Audit selectable paragraph ownership and retained text placement against
  generated text/paragraph policy in C and Go; web remains paused.
- Add matched interaction tests for selection crossing lines and text/image
  content inside clipped scopes. Go inline-icon rendering remains unfinished.

Glyph lookup, atlas drawing, image decoding/upload/cache, and actual font
measurement remain host services. Their presence alone is not migration debt.

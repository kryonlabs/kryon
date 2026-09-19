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

Completed follow-up:

- TextArea visual rows and selectable-block wrapping use `text_rows.kry` through
  native storage/measurement adapters. The independent C loops and Go whitespace
  collapsing wrapper are removed. Thirteen source-range fixtures execute in C
  and Go; policy also executes in generated C++.
- Selection and composition painting use shared per-line spans. Soft-wrap caret
  affinity is shared, and Go pointer placement resolves the clicked visual row.
- Go Paragraph built-in inline icons use the same tokenization, spacing and
  wrapping policy as C. Raw texture upload is still renderer support work.
- Canvas tests cover nested cameras and clips, text/image operations inside
  clips and restoration before following content. Generated Scroll/Canvas
  fixtures exercise return, break and continue cleanup.

Glyph lookup, atlas drawing, image decoding/upload/cache, and actual font
measurement remain host services. Their presence alone is not migration debt.

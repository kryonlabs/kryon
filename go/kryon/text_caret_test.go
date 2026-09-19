package kryon

import "testing"

func TestInputMethodCaretUsesWrappedRowsAndScroll(t *testing.T) {
	ensureDefaultTextFont()
	op := FrameOp{Kind: FrameOpTextArea, Bounds: NewRectangle(20, 30, 90, 160),
		ContentOffset: Vector2{X: 10, Y: 8}, FontSize: 16, Wrap: true,
		Text: "Cafe\u0301 日本語 👩‍💻 more text"}
	width := int(TextInput_TextAreaWrapWidthFor(op.Bounds.Width, 10, true, TextInput_TextAreaMinWrapWidth(1)))
	rows := renderTextAreaLines(op.Text, width, 16, 0, true)
	if len(rows) < 2 {
		t.Fatal("test requires wrapped rows")
	}
	op.Cursor = int32(rows[1].start)
	height := textHeight(rows[0].font, 0)
	caret := textInputCaretBounds(op)
	if caret.X != 30 || caret.Y != 38+float32(height) {
		t.Fatalf("soft-wrap caret = %+v", caret)
	}
	op.ScrollY = height
	if scrolled := textInputCaretBounds(op); scrolled.Y != 38 || scrolled.X != caret.X {
		t.Fatalf("scrolled caret = %+v", scrolled)
	}
}

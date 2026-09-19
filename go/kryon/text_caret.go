package kryon

// Candidate windows use the same rows, font metrics and scroll coordinates as
// painting. This is host geometry; shared row policy decides cursor ownership.
func textInputCaretBounds(op FrameOp) Rectangle {
	cursor := clampCursor(op.Text, int(op.Cursor))
	if op.Kind != FrameOpTextArea {
		x := op.Bounds.X + 8 + float32(textAdvance(op.Text, int32(cursor), op.FontSize, op.FontID))
		y := op.Bounds.Y + 5
		return NewRectangle(min(max(x, op.Bounds.X), op.Bounds.X+op.Bounds.Width), y, 1, max(float32(1), op.Bounds.Height-10))
	}
	paddingX := int(round(op.ContentOffset.X))
	paddingY := int(round(op.ContentOffset.Y))
	if paddingX <= 0 {
		paddingX = 8
	}
	if paddingY <= 0 {
		paddingY = 8
	}
	width := TextInput_TextAreaWrapWidthFor(op.Bounds.Width, int32(paddingX), op.Wrap, TextInput_TextAreaMinWrapWidth(1))
	rows := renderTextAreaLines(op.Text, int(width), op.FontSize, op.FontID, op.Wrap)
	y := float32(int(round(op.Bounds.Y)) + paddingY - int(op.ScrollY))
	for _, row := range rows {
		height := textHeight(row.font, op.FontID)
		if TextRows_TextRowHasCursor(int32(row.start), int32(row.end), int32(row.lineEnd), int32(cursor)) {
			x := float32(int(round(op.Bounds.X))+paddingX) + float32(textAdvance(op.Text[row.start:cursor], int32(cursor-row.start), row.font, op.FontID))
			return NewRectangle(min(max(x, op.Bounds.X), op.Bounds.X+op.Bounds.Width),
				min(max(y, op.Bounds.Y), op.Bounds.Y+max(float32(0), op.Bounds.Height-float32(height))), 1, float32(height))
		}
		y += float32(maxInt(1, int(height)+max(int(round(op.Gap)), 0)))
	}
	return NewRectangle(op.Bounds.X, op.Bounds.Y, 1, float32(max(op.FontSize, 1)))
}

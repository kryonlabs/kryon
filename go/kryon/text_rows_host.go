package kryon

import "github.com/clipperhouse/uax29/v2/graphemes"

type textAreaRenderLine struct {
	text                string
	start, end, lineEnd int
	font                int32
}

// Hosts retain strings, Unicode boundaries, and font measurement. Shared row
// policy preserves byte ranges so painting, selection and hit tests agree.
func measuredTextRows(text string, width int, font int32, headings, words bool,
	measure func(string, int32) int) []textAreaRenderLine {
	var rows []textAreaRenderLine
	for offset := int32(0); ; {
		line := TextRows_TextLogicalLineFor(text, offset)
		if !line.Valid {
			return rows
		}
		rowFont := TextRows_TextRowFontFor(text, line.Start, line.End, font, headings)
		start := line.Start
		for {
			state := TextRowBreak{End: start}
			// Measure a logical line once, rather than its entire remaining
			// tail again for each soft-wrapped row.
			fullWidth := width + 1
			if start == line.Start || width <= 0 {
				fullWidth = measure(text[start:line.End], rowFont)
			}
			if width <= 0 || fullWidth <= width || start == line.End {
				state = TextRows_TextRowAdvance(state, start, line.End, line.End, false, float32(fullWidth), float32(width), words)
			} else {
				clusters := graphemes.FromString(text[start:line.End])
				for !state.Done && clusters.Next() {
					next := start + int32(clusters.End())
					measured := measure(text[start:next], rowFont)
					space := text[state.End] == ' ' || text[state.End] == '\t'
					state = TextRows_TextRowAdvance(state, start, next, line.End, space, float32(measured), float32(width), words)
				}
			}
			rows = append(rows, textAreaRenderLine{text: text[start:state.End], start: int(start), end: int(state.End), lineEnd: int(line.End), font: rowFont})
			start = TextRows_TextRowNextStart(text, state.End, line.End, words)
			if start >= line.End {
				break
			}
		}
		offset = line.Next
	}
}

func renderTextAreaLines(text string, wrapWidth int, fontSize int32, fontID uint32, wrap bool) []textAreaRenderLine {
	if !wrap {
		wrapWidth = 0
	}
	key := textRowsKey{text: text, width: wrapWidth, font: fontSize, fontID: fontID}
	return editableRows.layout(key, fontGeneration.Load(),
		func(value string, font int32) int { return runtimeTextWidthWithFont(value, font, fontID) })
}

func (r *runtime) textAreaCursorAtPoint(text string, props TextAreaProps, point Vector2) int {
	focused := r.focusID == props.FocusID || props.Focused != nil && *props.Focused
	style := r.textInputStyle(FrameOpTextArea, focused, r.contentDisabled(), props.ClassName)
	font := r.textInputDefaultFont(FrameOpTextArea, focused, r.contentDisabled(), props.ClassName, Text16)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), styleGapLength(style), font, 10, 8, 0)
	width := TextInput_TextAreaWrapWidthFor(props.Bounds.Width, metrics.PaddingX, props.Wrap, TextInput_TextAreaMinWrapWidth(1))
	rows := renderTextAreaLines(text, int(width), metrics.Font, styleFontID(style), props.Wrap)
	targetY := int32(point.Y-props.Bounds.Y) - metrics.PaddingY
	if props.ScrollY != nil {
		targetY += *props.ScrollY
	}
	targetX := int(point.X-props.Bounds.X) - int(metrics.PaddingX)
	y := int32(0)
	for i, row := range rows {
		height := textHeight(row.font, styleFontID(style)) + metrics.LineGap
		if TextRows_TextRowAtY(y, height, targetY, i == len(rows)-1) {
			pos := row.start
			for pos < row.end && targetX > 0 {
				pos = nextGrapheme(text[:row.end], pos)
				if runtimeTextWidthWithFont(text[row.start:pos], row.font, styleFontID(style)) >= targetX {
					break
				}
			}
			return pos
		}
		y += height
	}
	return len(text)
}

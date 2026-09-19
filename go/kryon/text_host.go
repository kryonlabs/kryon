package kryon

import (
	"fmt"
	"strings"
)

func (r *runtime) Text(props TextProps) {
	r.textWithFont(props, 0)
}

func (r *runtime) textWithFont(props TextProps, fontID uint32) {
	r.recordAccessibleText(props.Text)
	var inheritedFont int32
	var inheritedColor Color
	var inheritedColorSet bool
	var inheritedDisabled bool
	for i := len(r.layout) - 1; i >= 0; i-- {
		context := r.layout[i]
		if context.textFont > 0 || context.textColorSet {
			inheritedFont = context.textFont
			inheritedColor = context.textColor
			inheritedColorSet = context.textColorSet
			inheritedDisabled = context.textDisabled
			break
		}
	}
	props.Disabled = props.Disabled || (r.contentDisabled() && !inheritedDisabled)
	textState := int32(ButtonStateNormal)
	if props.Disabled {
		textState = int32(ButtonStateDisabled)
	}
	style := unpackStyle(ResolveActiveStyle(packStyle(Style{Opacity: 1}),
		StyleSheet_StyleTextFacts(0, props.ClassName, StyleSheet_StyleKindText(), textState),
		textState))
	colorSet := style.Fields&StyleForeground != 0
	requestedFont := int32(0)
	if props.Font > 0 {
		requestedFont = props.Font
	} else if style.Fields&StyleFontSize != 0 {
		requestedFont = int32(style.FontSize)
	}
	if style.Fields&StyleTypeface != 0 {
		if selected := registeredTypeface(style.Typeface); selected != 0 {
			fontID = selected
		}
	}
	letterSpacing := int32(0)
	if style.Fields&StyleLetterSpacing != 0 {
		letterSpacing = styleLength(style.LetterSpacing)
	}
	appearance := Text_ResolveTextStyle(requestedFont, inheritedFont, Text16,
		packRGBA(style.Foreground), packRGBA(inheritedColor), 0xffffffff,
		inheritedColorSet, colorSet, props.Disabled, inheritedDisabled, letterSpacing)
	font, spacing := appearance.Font, appearance.LetterSpacing
	color := unpackRGBA(Surface_Opacity(appearance.Color,
		Style_StyleOpacityValue(uint32(style.Fields), style.Opacity)))
	measure := func(text string) int {
		width := 0
		for _, line := range strings.Split(text, "\n") {
			count := len([]rune(line))
			width = max(width, runtimeTextWidthWithFont(line, font, fontID)+max(count-1, 0)*int(spacing))
		}
		return width
	}
	bounded := props.Bounds.Width > 0
	bounds := props.Bounds
	props.Wrap = TextWrap(Text_TextWrapPolicy(bounds.Width, int32(props.Wrap)))
	var measuredWidth float32
	if !bounded {
		measuredWidth = float32(measure(props.Text))
	}
	bounds.Width = Text_TextExtent(bounds.Width, measuredWidth)
	lines := []string{props.Text}
	var lineWidths []float32
	if bounded && props.Wrap == TextWrapAuto {
		result := r.textLayouts.layout(textLayoutKey{
			text: props.Text, width: bounds.Width, font: font, fontID: fontID, spacing: spacing,
		}, fontGeneration.Load(), measure)
		lines, lineWidths = result.lines, result.widths
	}
	lineGap := Paragraph_ParagraphDefaultLineGap(1)
	lineHeight := float32(Paragraph_ParagraphLineStride(textHeight(font, fontID), lineGap))
	contentHeight := float32(Paragraph_ParagraphLayoutTotalHeight(int32(len(lines)), textHeight(font, fontID), lineGap))
	bounds.Height = Text_TextExtent(bounds.Height, contentHeight)
	if bounds.X == 0 && bounds.Y == 0 {
		bounds = r.layoutRect(bounds)
	}
	startY := bounds.Y + Text_TextAlignmentOffset(bounds.Height, contentHeight, int32(props.VerticalAlign))
	var selectionLines []selectableTextLine
	var selectionStart, selectionEnd int
	if props.Selectable && props.Text != "" && !props.Disabled && !inheritedDisabled {
		selectionLines = selectableLines(props.Text, lines)
		selectionStart, selectionEnd = r.selectTextRange(props, bounds, startY, lineHeight, lines, selectionLines, measure)
	}
	for i, line := range lines {
		y := startY + float32(i)*lineHeight
		// A partially visible line is clipped, not discarded. C draws the
		// same text box even when its font metrics exceed the box height.
		if y >= bounds.Y+bounds.Height {
			break
		}
		var lineWidth float32
		if lineWidths != nil {
			lineWidth = lineWidths[i]
		} else {
			lineWidth = float32(measure(line))
		}
		x := bounds.X + Text_TextAlignmentOffset(bounds.Width, lineWidth, int32(props.Align))
		op := FrameOp{Kind: FrameOpText,
			Bounds: Rectangle{X: x, Y: y, Width: lineWidth, Height: float32(textHeight(font, fontID))},
			Clip:   bounds, HasClip: true, Text: line, Color: color, FontSize: font,
			FontID: fontID, Disabled: props.Disabled || inheritedDisabled, LetterSpacing: spacing}
		if selectionLines != nil && selectionEnd > selectionStart {
			mapped := selectionLines[i]
			for offset, sourceOffset := range mapped.offsets {
				if sourceOffset <= selectionStart {
					op.SelectionStart = int32(offset)
				}
				if sourceOffset < selectionEnd {
					op.SelectionEnd = int32(offset + 1)
				}
			}
			op.SelectionStart = int32(clampCursor(line, int(op.SelectionStart)))
			op.SelectionEnd = int32(clampCursor(line, min(len(line), int(op.SelectionEnd))))
			op.Selected = op.SelectionEnd > op.SelectionStart && selectionStart < mapped.end && selectionEnd > mapped.start
			if !op.Selected {
				op.SelectionStart, op.SelectionEnd = 0, 0
			}
			op.ID = int32(r.selectableText)
			op.SelectionColor = color
			op.SelectionColor.A = Text_TextSelectionDefaultAlpha()
		}
		r.record(op)
	}
}

func (r *runtime) TextFormat(format string, args ...any) string { return fmt.Sprintf(format, args...) }

func (r *runtime) Paragraph(spec ParagraphSpec, x int32, y *int32) {
	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindParagraphText())
	facts.ClassName = spec.ClassName
	style := unpackStyle(ResolveActiveStyle(StyleData{},
		facts,
		int32(ButtonStateNormal)))
	textStyle := defaultTextStyle(Text16)
	textFont, textFontID := styleTextFace(textStyle, Text16)
	font, fontID := styleTextFaceWithFallback(style, textFont, textFontID)
	if spec.Font > 0 {
		font = spec.Font
	}
	color := textStyle.Foreground
	if style.Fields&StyleForeground != 0 {
		color = style.Foreground
	}
	if style.Fields&uint32(StyleOpacity) != 0 && style.Opacity < 1 {
		color = unpackRGBA(Surface_Opacity(packRGBA(color), style.Opacity))
	}
	textY := int32(0)
	if y != nil {
		textY = *y
	}
	fallbackWidth := int32(r.config.Width) - x
	metrics := Paragraph_ParagraphResolveMetrics(font, Text16,
		spec.LineGap, 4, spec.IconSize, spec.Width, fallbackWidth, 0, textY)
	if !Paragraph_ParagraphCanLayout(metrics.Width) {
		return
	}
	measure := func(text string) int { return runtimeTextWidthWithFont(text, metrics.Font, fontID) }
	policy := Paragraph_ParagraphLayoutPolicyFor(int32(measure(" ")), 0, metrics.LineGap, 4, 1)
	layout := layoutParagraph(spec.Text, metrics.Width, metrics.IconSize, spec.IconType != IconNone || spec.Icon.ID != 0, policy, measure)
	lineHeight := textHeight(metrics.Font, fontID)
	metrics.Height = Paragraph_ParagraphLayoutTotalHeight(int32(len(layout.lines)), lineHeight, metrics.LineGap)
	bounds := r.layoutRect(Rectangle{X: float32(x), Y: float32(textY), Width: float32(metrics.Width), Height: float32(metrics.Height)})
	lineY := int32(bounds.Y)
	for index, line := range layout.lines {
		lineX := Paragraph_ParagraphLineXFor(int32(bounds.X), metrics.Width, int32(line.Width), int32(spec.Align))
		r.recordParagraphLine(layout, line, spec, lineX, lineY, metrics.Font, lineHeight, metrics.IconSize, fontID, color, policy)
		lineY = Paragraph_ParagraphNextLineY(lineY, lineHeight, metrics.LineGap, index+1 < len(layout.lines))
	}
	if y != nil {
		*y = textY + metrics.Height
	}
}

func elideText(text string, maxWidth float32, font int32) string {
	return elideTextWithFont(text, maxWidth, font, 0)
}

func elideTextWithFont(text string, maxWidth float32, font int32, fontID uint32) string {
	if maxWidth <= 0 {
		return ""
	}
	runes := []rune(text)
	if runtimeTextWidthWithFont(text, font, fontID) <= int(maxWidth) {
		return text
	}
	ellipsis := "…"
	if runtimeTextWidthWithFont(ellipsis, font, fontID) > int(maxWidth) {
		return ""
	}
	for i := len(runes) - 1; i > 0; i-- {
		short := string(runes[:i]) + ellipsis
		if runtimeTextWidthWithFont(short, font, fontID) <= int(maxWidth) {
			return short
		}
	}
	return ellipsis
}

func runtimeTextWidth(text string, font int32) int {
	return runtimeTextWidthWithFont(text, font, 0)
}

func runtimeTextWidthWithFont(text string, font int32, fontID uint32) int {
	if text == "" {
		return 0
	}
	return int(MeasureTextEx(Font{ID: fontID}, text, float32(font), 1).X)
}

package kryon

import "strings"

type paragraphElement struct {
	text            string
	width           int32
	icon, lineBreak bool
}

type paragraphLayout struct {
	elements []paragraphElement
	lines    []ParagraphLine
}

func layoutParagraph(text string, width, iconSize int32, icons bool, policy ParagraphLayoutPolicy, measure func(string) int) paragraphLayout {
	var layout paragraphLayout
	for offset := int32(0); ; {
		token := Paragraph_ParagraphTokenNext(text, offset, icons)
		if !token.Valid {
			break
		}
		element := paragraphElement{icon: token.Icon, lineBreak: token.LineBreak}
		if token.Icon {
			element.width = iconSize
		} else if !token.LineBreak {
			element.text = text[token.Start:token.End]
			element.width = int32(measure(element.text))
		}
		layout.elements = append(layout.elements, element)
		offset = token.Next
	}
	var line ParagraphLine
	var candidate string
	hasIcon := false
	for index := 0; index <= len(layout.elements); index++ {
		end := index == len(layout.elements)
		var element paragraphElement
		if !end {
			element = layout.elements[index]
		}
		candidateWidth := float32(0)
		if !end && !element.lineBreak {
			if !element.icon && !hasIcon {
				candidate += Paragraph_ParagraphTextSeparator(line) + element.text
				candidateWidth = float32(measure(candidate))
			} else {
				candidateWidth = line.Width + float32(element.width+Paragraph_ParagraphElementSpacing(line.HasContent, element.icon, policy))
			}
		}
		decision := Paragraph_ParagraphLineAdvance(line, int32(index), element.lineBreak, end, float32(element.width), candidateWidth, float32(width))
		if decision.Emit {
			layout.lines = append(layout.lines, decision.Line)
			candidate = ""
			hasIcon = false
			if decision.Next.HasContent && !element.icon {
				candidate = element.text
			}
		}
		line = decision.Next
		hasIcon = hasIcon || element.icon
	}
	return layout
}

func (r *runtime) recordParagraphLine(layout paragraphLayout, line ParagraphLine, spec ParagraphSpec,
	x, y, font, lineHeight, iconSize int32, fontID uint32, color Color, policy ParagraphLayoutPolicy) {
	elements := layout.elements[line.Start:line.End]
	var words []string
	hasIcon := false
	for _, element := range elements {
		if element.icon {
			hasIcon = true
		}
		if element.text != "" {
			words = append(words, element.text)
		}
	}
	if !hasIcon {
		r.record(FrameOp{Kind: FrameOpText, Bounds: NewRectangle(float32(x), float32(y), line.Width, float32(lineHeight)),
			Text: strings.Join(words, " "), FontSize: font, FontID: fontID, Color: color})
		return
	}
	for index, element := range elements {
		x += Paragraph_ParagraphElementSpacing(index > 0, element.icon, policy)
		if element.icon {
			kind := FrameOpIcon
			if spec.Icon.ID != 0 {
				kind = FrameOpImage
			}
			r.record(FrameOp{Kind: kind, Bounds: NewRectangle(float32(x), float32(y), float32(iconSize), float32(iconSize)),
				IconType: spec.IconType, IconSize: float32(iconSize), Color: color})
		} else {
			r.record(FrameOp{Kind: FrameOpText, Bounds: NewRectangle(float32(x), float32(y), float32(element.width), float32(lineHeight)),
				Text: element.text, FontSize: font, FontID: fontID, Color: color})
		}
		x += element.width
	}
}

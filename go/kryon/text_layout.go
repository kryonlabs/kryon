package kryon

// The host owns strings and font measurement. Tokenization, line boundaries,
// separators, and overflow decisions come from runtime/paragraph.kry.
func layoutTextLines(text string, width float32, measure func(string) int) []string {
	var lines []string
	var state ParagraphLine
	var current string
	var offset, index int32
	for {
		token := Paragraph_ParagraphTokenNext(text, offset, false)
		var word, candidate string
		var wordWidth, candidateWidth float32
		if token.Valid && !token.LineBreak {
			word = text[token.Start:token.End]
			candidate = current + Paragraph_ParagraphTextSeparator(state) + word
			wordWidth = float32(measure(word))
			candidateWidth = float32(measure(candidate))
		}
		decision := Paragraph_ParagraphLineAdvance(state, index,
			token.LineBreak, !token.Valid, wordWidth, candidateWidth, width)
		if decision.Emit {
			lines = append(lines, current)
			current = ""
			if decision.Next.HasContent {
				current = word
			}
		} else {
			current = candidate
		}
		state = decision.Next
		if !token.Valid {
			return lines
		}
		offset = token.Next
		index++
	}
}

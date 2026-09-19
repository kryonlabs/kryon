package kryon

import (
	"strings"

	"github.com/clipperhouse/uax29/v2/graphemes"
)

func clampCursor(text string, pos int) int {
	pos = max(0, min(pos, len(text)))
	if pos == 0 || pos == len(text) {
		return pos
	}
	if text[pos-1] < 128 && text[pos] < 128 && !(text[pos-1] == '\r' && text[pos] == '\n') {
		return pos
	}
	start := strings.LastIndexByte(text[:pos], '\n') + 1
	iterator := graphemes.FromString(text[start:])
	for iterator.Next() {
		if start+iterator.End() > pos {
			return start + iterator.Start()
		}
	}
	return len(text)
}

func previousGrapheme(text string, pos int) int {
	pos = max(0, min(pos, len(text)))
	return clampCursor(text, pos-1)
}

func nextGrapheme(text string, pos int) int {
	pos = clampCursor(text, pos)
	iterator := graphemes.FromString(text[pos:])
	if iterator.Next() {
		return pos + iterator.End()
	}
	return len(text)
}

func graphemeCount(text string) int {
	iterator := graphemes.FromString(text)
	count := 0
	for iterator.Next() {
		count++
	}
	return count
}

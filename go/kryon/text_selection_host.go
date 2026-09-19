package kryon

import (
	"fmt"
	"strings"
	"unicode/utf8"
)

type selectableTextState struct {
	anchor, cursor int
	dragging       bool
}

type selectableTextLine struct {
	start, end int
	offsets    []int
}

// Paragraph layout may collapse whitespace. Preserve the original byte offsets
// for selection/clipboard while retaining the existing displayed line contents.
func selectableLines(source string, lines []string) []selectableTextLine {
	result := make([]selectableTextLine, len(lines))
	position := 0
	for i, line := range lines {
		mapped := selectableTextLine{start: position, offsets: make([]int, len(line)+1)}
		for offset, value := range line {
			encoded := string(value)
			found := strings.Index(source[position:], encoded)
			if found < 0 {
				found = 0
			}
			position += found
			if offset == 0 {
				mapped.start = position
			}
			for byteIndex := 0; byteIndex < len(encoded); byteIndex++ {
				mapped.offsets[offset+byteIndex] = min(position+byteIndex, len(source))
			}
			position = min(position+len(encoded), len(source))
			mapped.offsets[offset+len(encoded)] = position
		}
		mapped.end = position
		if line == "" {
			mapped.offsets[0] = position
			if position < len(source) && source[position] == '\n' {
				position++
			}
		}
		result[i] = mapped
	}
	return result
}

func (r *runtime) selectTextRange(props TextProps, bounds Rectangle, startY, stride float32,
	lines []string, mapped []selectableTextLine, measure func(string) int) (int, int) {
	key := Key(fmt.Sprintf("%g:%g:%s", bounds.X, bounds.Y, props.Text))
	if r.selectableText == key {
		for _, tap := range r.taps {
			if !pointInRect(tap.x, tap.y, bounds) {
				r.selectableText = 0
				r.textSelection = selectableTextState{}
				break
			}
		}
	}
	pointOffset := func(point Vector2) int {
		if point.Y < startY {
			return 0
		}
		index := min(max(int((point.Y-startY)/max(stride, 1)), 0), len(lines)-1)
		line := lines[index]
		x := bounds.X + Text_TextAlignmentOffset(bounds.Width, float32(measure(line)), int32(props.Align))
		target := point.X - x
		position := 0
		previousWidth := 0
		for position < len(line) {
			next := nextGrapheme(line, position)
			width := measure(line[:next])
			if target < float32(previousWidth+width)/2 {
				break
			}
			previousWidth, position = width, next
		}
		return clampCursor(props.Text, mapped[index].offsets[position])
	}
	point, pressed := r.consumeTapPosition(bounds)
	pointer := Text_TextSelectionPointerDecisionFor(pressed, false, pressed)
	if pointer.Begin {
		position := pointOffset(point)
		r.setFocus(0)
		r.selectableText = key
		r.textSelection = selectableTextState{anchor: position, cursor: position, dragging: true}
	}
	active := r.selectableText == key
	if active && r.popupKeyboardCaptures() {
		r.textSelection.dragging = false
		return 0, 0
	}
	drag := Text_TextSelectionDragDecisionFor(active, r.textSelection.dragging, r.mouseDown[MouseButtonLeft])
	if drag.Update || drag.Finish {
		r.textSelection.cursor = pointOffset(r.mousePos)
	}
	if drag.Finish {
		r.textSelection.dragging = false
	}
	if !active {
		return 0, 0
	}
	remaining := r.inputEvents[:0]
	for _, event := range r.inputEvents {
		handled := false
		if event.shortcut && event.key == KeyA {
			selected := TextInput_TextSelectionAll(int32(len(props.Text)))
			r.textSelection.anchor, r.textSelection.cursor = int(selected.Anchor), int(selected.Cursor)
			handled = true
		}
		copy := Text_TextSelectionCopyDecisionFor(active, !r.contentDisabled(), event.shortcut, event.key == KeyC)
		if copy.Copy {
			rangeValue := TextInput_TextSelectionRangeFor(int32(r.textSelection.anchor), int32(r.textSelection.cursor))
			if rangeValue.End > rangeValue.Start {
				r.clipboard = props.Text[rangeValue.Start:rangeValue.End]
			}
			handled = true
		}
		if !handled {
			remaining = append(remaining, event)
		}
	}
	r.inputEvents = remaining
	start, end := r.textSelection.anchor, r.textSelection.cursor
	if start > end {
		start, end = end, start
	}
	if !utf8.ValidString(props.Text[start:end]) {
		return 0, 0
	}
	return start, end
}

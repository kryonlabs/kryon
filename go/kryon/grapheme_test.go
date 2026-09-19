package kryon

import (
	"bufio"
	"os"
	"slices"
	"strconv"
	"strings"
	"testing"

	"github.com/clipperhouse/uax29/v2/graphemes"
)

func TestGraphemeUnicodeConformance(t *testing.T) {
	file, err := os.Open("../../tests/fixtures/GraphemeBreakTest-17.0.0.txt")
	if err != nil {
		t.Fatal(err)
	}
	defer file.Close()
	scanner := bufio.NewScanner(file)
	line := 0
	for scanner.Scan() {
		line++
		body, _, _ := strings.Cut(scanner.Text(), "#")
		tokens := strings.Fields(body)
		if len(tokens) == 0 {
			continue
		}
		var text strings.Builder
		var expected []int
		for _, token := range tokens {
			switch token {
			case "\u00f7":
				expected = append(expected, text.Len())
			case "\u00d7":
			default:
				value, err := strconv.ParseInt(token, 16, 32)
				if err != nil {
					t.Fatal(err)
				}
				text.WriteRune(rune(value))
			}
		}
		value := text.String()
		actual := []int{0}
		iterator := graphemes.FromString(value)
		for iterator.Next() {
			actual = append(actual, iterator.End())
		}
		if !slices.Equal(actual, expected) {
			t.Fatalf("line %d: %v, want %v", line, actual, expected)
		}
		for pos := 0; pos <= len(value); pos++ {
			floor, previous, next := 0, 0, len(value)
			for _, boundary := range expected {
				if boundary <= pos {
					floor = boundary
				}
				if boundary < pos {
					previous = boundary
				}
				if boundary > pos {
					next = boundary
					break
				}
			}
			if clampCursor(value, pos) != floor || previousGrapheme(value, pos) != previous || nextGrapheme(value, pos) != next {
				t.Fatalf("line %d offset %d: incorrect grapheme cursor boundary", line, pos)
			}
		}
	}
	if err := scanner.Err(); err != nil {
		t.Fatal(err)
	}
}

func TestTextFieldEditsWholeGraphemes(t *testing.T) {
	for _, cluster := range []string{"e\u0301", "\U0001f469\u200d\U0001f4bb", "\U0001f1f5\U0001f1fe", "\U0001f44d\U0001f3fd", "\u0915\u094d\u0937"} {
		t.Run(cluster, func(t *testing.T) {
			r := New(AppConfig{Width: 400, Height: 200}).(*runtime)
			text := make([]byte, 128)
			copy(text, "A"+cluster+"Z")
			cursor := int32(1 + len(cluster))
			r.SetFocus(901)
			draw := func() {
				r.BeginFrame()
				r.TextField(TextFieldProps{Bounds: NewRectangle(0, 0, 300, 40), Text: text, CursorPosition: &cursor, FocusID: 901})
				r.EndFrame()
			}
			draw()
			r.QueueKey(KeyLeft)
			draw()
			if cursor != 1 {
				t.Fatalf("left split a grapheme: %d", cursor)
			}
			r.QueueKey(KeyDelete)
			draw()
			if CString(text) != "AZ" || cursor != 1 {
				t.Fatalf("delete split a grapheme: %q at %d", CString(text), cursor)
			}
			clear(text)
			copy(text, "A"+cluster+"Z")
			cursor = int32(1 + len(cluster))
			r.SetSelection(901, cursor, cursor)
			r.QueueKey(KeyBackspace)
			draw()
			if CString(text) != "AZ" || cursor != 1 {
				t.Fatalf("backspace split a grapheme: %q at %d", CString(text), cursor)
			}
		})
	}
}

func TestGraphemeSelectionAndPlacement(t *testing.T) {
	text := "Ae\u0301\r\nB\U0001f469\u200d\U0001f4bbZ"
	firstEnd := len("Ae\u0301")
	secondStart := firstEnd + 2
	secondCursor := len(text) - 1
	if textLineEnd(text, 0) != firstEnd || nextGrapheme(text, firstEnd) != secondStart {
		t.Fatal("CRLF must be a single cursor step")
	}
	if got := textMoveVertical(text, firstEnd, 1, 1); got != secondCursor {
		t.Fatalf("down = %d, want %d", got, secondCursor)
	}
	if got := textMoveVertical(text, secondCursor, -1, 1); got != firstEnd {
		t.Fatalf("up = %d, want %d", got, firstEnd)
	}
	for x := float32(0); x < 400; x++ {
		pos := cursorAtTap(text, NewRectangle(0, 0, 400, 40), x)
		if clampCursor(text, pos) != pos {
			t.Fatalf("tap at %v split grapheme at %d", x, pos)
		}
	}
	current := selection{Anchor: 2, Cursor: firstEnd}
	result, pos, _, changed := textDeleteKey(text, firstEnd, current, KeyDelete, false, false)
	if !changed || result != "A\r\nB\U0001f469\u200d\U0001f4bbZ" || pos != 1 {
		t.Fatalf("selection split a grapheme: %q at %d", result, pos)
	}
}

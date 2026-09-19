package kryon

import (
	"encoding/json"
	"os"
	"reflect"
	"testing"
)

func TestTextRowFixtures(t *testing.T) {
	data, err := os.ReadFile("../../tests/fixtures/text_rows.json")
	if err != nil {
		t.Fatal(err)
	}
	var cases []struct {
		Name, Text string
		Width      int
		Words      bool
		Rows       [][3]int
	}
	if err := json.Unmarshal(data, &cases); err != nil {
		t.Fatal(err)
	}
	for _, fixture := range cases {
		t.Run(fixture.Name, func(t *testing.T) {
			rows := measuredTextRows(fixture.Text, fixture.Width, 16, true, fixture.Words,
				func(text string, font int32) int { return graphemeCount(text) })
			var actual [][3]int
			for _, row := range rows {
				actual = append(actual, [3]int{row.start, row.end, int(row.font)})
			}
			if !reflect.DeepEqual(actual, fixture.Rows) {
				t.Fatalf("got %v, want %v", actual, fixture.Rows)
			}
		})
	}
}

func TestTextAreaClickUsesVisualRowAndScroll(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	buf := make([]byte, 64)
	copy(buf, "ab\ncd\nef")
	cursor, scroll := int32(0), int32(0)
	props := TextAreaProps{Bounds: NewRectangle(10, 10, 140, 100), Text: buf,
		CursorPosition: &cursor, FocusID: 903, ScrollY: &scroll}
	style := r.textInputStyle(FrameOpTextArea, false, false, 0)
	font := r.textInputDefaultFont(FrameOpTextArea, false, false, 0, Text16)
	metrics := TextInput_TextInputMetricsFor(style.Fields, 0, int32(style.PaddingX), int32(style.PaddingY), styleGapLength(style), font, 10, 8, 0)
	height := textHeight(metrics.Font, styleFontID(style)) + metrics.LineGap
	for _, scrolled := range []bool{false, true} {
		y := props.Bounds.Y + float32(metrics.PaddingY) + float32(height) + 1
		if scrolled {
			scroll = height
			y -= float32(height)
		}
		r.QueueTap(props.Bounds.X+float32(metrics.PaddingX), y)
		r.BeginFrame()
		r.TextArea(props)
		r.EndFrame()
		if cursor != 3 {
			t.Fatalf("scrolled=%v cursor=%d, want second row start 3", scrolled, cursor)
		}
	}
}

func TestTextAreaSoftWrapHasOneCaret(t *testing.T) {
	rows := measuredTextRows("abcdef", 3, 16, false, false,
		func(text string, font int32) int { return len(text) })
	count := 0
	for _, row := range rows {
		if TextRows_TextRowHasCursor(int32(row.start), int32(row.end), int32(row.lineEnd), 3) {
			count++
		}
	}
	if count != 1 {
		t.Fatalf("soft-wrap boundary has %d carets", count)
	}
}

func TestTextAreaPaintStaysInsideContentClip(t *testing.T) {
	op := FrameOp{Kind: FrameOpTextArea, Bounds: NewRectangle(20, 20, 40, 35),
		FontSize: 16, ContentOffset: Vector2{X: 10, Y: 8}, TextColor: BLACK,
		Color: WHITE, BorderColor: GRAY}
	empty := RenderFrame(100, 100, []FrameOp{op})
	op.Text = "long unwrapped text\nsecond row"
	painted := RenderFrame(100, 100, []FrameOp{op})
	for y := 0; y < 100; y++ {
		for x := 0; x < 100; x++ {
			if x >= 30 && x < 50 && y >= 28 && y < 47 {
				continue
			}
			if empty.RGBAAt(x, y) != painted.RGBAAt(x, y) {
				t.Fatalf("text escaped content clip at %d,%d", x, y)
			}
		}
	}
}

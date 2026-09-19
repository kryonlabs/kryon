package kryon

import (
	"encoding/json"
	"os"
	"reflect"
	"strings"
	"testing"
	"unicode/utf8"
)

// Matches the deterministic measurement host in paragraph_layout_test.c.
// Kerning and zero-width glyphs ensure wrapping uses measured candidates and
// content presence rather than summed word widths or width > 0.
func paragraphTestWidth(text string) int {
	return utf8.RuneCountInString(text) - strings.Count(text, "~") - strings.Count(text, "A V")
}

func TestParagraphLayoutFixtures(t *testing.T) {
	data, err := os.ReadFile("../../tests/fixtures/paragraph_layout.json")
	if err != nil {
		t.Fatal(err)
	}
	var cases []struct {
		Name  string
		Text  string
		Width float32
		Lines []string
	}
	if err := json.Unmarshal(data, &cases); err != nil {
		t.Fatal(err)
	}
	for _, test := range cases {
		t.Run(test.Name, func(t *testing.T) {
			got := layoutTextLines(test.Text, test.Width, paragraphTestWidth)
			if !reflect.DeepEqual(got, test.Lines) {
				t.Fatalf("got %q, want %q", got, test.Lines)
			}
		})
	}
}

func TestWrappedTextClipsAndAlignsSharedLines(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	r := New(AppConfig{Width: 300, Height: 180}).(*runtime)
	font := int32(16)
	word := "oversized"
	width := float32(runtimeTextWidthWithFont(word, font, 0) - 1)
	bounds := Rectangle{X: 20, Y: 20, Width: width, Height: 90}
	r.BeginFrame()
	r.Text(TextProps{Text: word + "\n\nx", Bounds: bounds, Font: font,
		Wrap: TextWrapAuto, Align: TextAlignEnd, Selectable: true})
	r.EndFrame()
	var texts []FrameOp
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpText {
			texts = append(texts, op)
		}
	}
	if len(texts) != 3 || texts[0].Text != word || texts[1].Text != "" || texts[2].Text != "x" {
		t.Fatalf("wrapped text lost or inserted lines: %+v", texts)
	}
	for _, op := range texts {
		if !op.HasClip || op.Clip != bounds {
			t.Fatalf("text escaped its clip: %+v", op)
		}
		if op.Bounds.X != bounds.X+bounds.Width-op.Bounds.Width {
			t.Fatalf("text is not aligned using its measured width: %+v", op)
		}
	}
	if texts[0].Bounds.Y != bounds.Y || texts[2].Bounds.Y <= texts[1].Bounds.Y {
		t.Fatal("oversized words and empty lines must retain their vertical positions")
	}
}

func TestParagraphCallersUseSharedReflow(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.paragraph.lines;
ParagraphText { font-size: 16; gap: 9; }`, "Paragraph Lines", "") {
		t.Fatal("paragraph style did not register")
	}
	for _, semantic := range []bool{false, true} {
		r := New(AppConfig{Width: 300, Height: 180}).(*runtime)
		width := int32(runtimeTextWidth("word", 16))
		y := int32(20)
		r.BeginFrame()
		if semantic {
			r.ParagraphText(ParagraphTextProps{Text: "word word\n\nx",
				Bounds: Rectangle{X: 20, Y: 20, Width: float32(width)}})
		} else {
			r.Paragraph(ParagraphSpec{Text: "word word\n\nx", Width: width,
				Font: 16, LineGap: 4, Align: TextAlignEnd}, 20, &y)
		}
		r.EndFrame()
		var got []string
		var last FrameOp
		for _, op := range r.FrameOps() {
			if op.Kind == FrameOpText {
				if semantic && len(got) > 0 && op.Bounds.Y != last.Bounds.Y+last.Bounds.Height+9 {
					t.Fatalf("paragraph lost its authored line gap: previous=%+v current=%+v", last.Bounds, op.Bounds)
				}
				got = append(got, op.Text)
				last = op
			}
		}
		if !reflect.DeepEqual(got, []string{"word", "word", "", "x"}) {
			t.Fatalf("semantic=%v: paragraph did not reflow: %q", semantic, got)
		}
		if !semantic && (float32(y) != last.Bounds.Y+last.Bounds.Height ||
			last.Bounds.X+last.Bounds.Width != 20+float32(width)) {
			t.Fatalf("paragraph cursor/alignment differs from its laid-out lines: y=%d last=%+v", y, last)
		}
	}
}

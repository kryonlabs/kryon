package kryon

import "testing"

func TestParagraphInlineIconsUseSharedWrapAndSpacing(t *testing.T) {
	policy := Paragraph_ParagraphLayoutPolicyFor(1, 0, 4, 4, 1)
	layout := layoutParagraph("a%i b\n%i", 5, 2, true, policy, func(text string) int { return len(text) })
	if len(layout.elements) != 5 || len(layout.lines) != 3 {
		t.Fatalf("layout = %+v", layout)
	}
	for i, width := range []float32{1, 4, 2} {
		if layout.lines[i].Width != width {
			t.Fatalf("line %d width=%g want=%g", i, layout.lines[i].Width, width)
		}
	}
	r := New(AppConfig{Width: 240, Height: 180}).(*runtime)
	r.BeginFrame()
	y := int32(10)
	r.Paragraph(ParagraphSpec{Text: "before %i after", IconType: IconCheck, IconSize: 20, Width: 220}, 10, &y)
	r.EndFrame()
	icons := 0
	for _, op := range r.ops {
		if op.Kind == FrameOpText && op.Text == "%i" {
			t.Fatal("inline icon was painted as text")
		}
		if op.Kind == FrameOpIcon && op.IconType == IconCheck {
			icons++
		}
	}
	if icons != 1 {
		t.Fatalf("got %d inline icons", icons)
	}
}

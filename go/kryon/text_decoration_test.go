package kryon

import "testing"

func TestStrikethroughFollowsAlignedWrappedText(t *testing.T) {
	r := New(AppConfig{Width: 300, Height: 240}).(*runtime)
	bounds := Rectangle{X: 20, Y: 30, Width: 110, Height: 100}
	r.BeginFrame()
	r.Text(TextProps{Bounds: bounds, Text: "Completed task with a long title", Font: 16,
		Align: TextAlignCenter, VerticalAlign: TextAlignCenter, Strikethrough: true})
	r.EndFrame()
	var previous FrameOp
	lines, strikes := 0, 0
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpText {
			previous = op
			lines++
		} else if op.Kind == FrameOpRect && op.Role == "presentation" {
			strikes++
			if op.Bounds.X != previous.Bounds.X || op.Bounds.Width != previous.Bounds.Width ||
				op.Bounds.Y <= previous.Bounds.Y || op.Bounds.Y >= previous.Bounds.Y+previous.Bounds.Height ||
				!op.HasClip || op.Clip != bounds {
				t.Fatalf("decoration does not follow text: text=%+v strike=%+v", previous, op)
			}
		}
	}
	if lines < 2 || strikes != lines {
		t.Fatalf("lines=%d strikes=%d", lines, strikes)
	}
	r.BeginFrame()
	r.Text(TextProps{Bounds: bounds, Text: "Active"})
	r.EndFrame()
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpRect && op.Role == "presentation" {
			t.Fatal("decoration leaked to ordinary text")
		}
	}
}

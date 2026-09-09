package kryon

import (
	"bytes"
	"testing"
)

func TestScreenScopesDoNotPaintBorders(t *testing.T) {
	background := FrameOp{Kind: FrameOpBackground, Color: Color{9, 32, 57, 255}}
	expected := RenderFrame(120, 60, []FrameOp{background})
	actual := RenderFrame(120, 60, []FrameOp{
		background,
		{Kind: FrameOpScreen, Bounds: Rectangle{Width: 60, Height: 60}, ID: 1},
		{Kind: FrameOpScreen, Bounds: Rectangle{X: 60, Width: 60, Height: 60}, ID: 2},
	})
	if !bytes.Equal(expected.Pix, actual.Pix) {
		t.Fatal("screen composition painted an implicit border")
	}
}

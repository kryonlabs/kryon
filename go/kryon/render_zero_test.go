package kryon

import (
	"image"
	"testing"
)

// Explicit zeros from resolved styles must survive the rasterizer's
// unstyled-value fallbacks (plan/style/05).
func TestRenderPreservesExplicitZeroBorder(t *testing.T) {
	newImg := func() (*image.RGBA, FrameOp) {
		img := image.NewRGBA(image.Rect(0, 0, 40, 20))
		op := FrameOp{
			Kind: FrameOpTextField, Bounds: Rectangle{X: 0, Y: 0, Width: 40, Height: 20},
			Material:    MaterialFlat,
			Color:       Color{R: 17, G: 34, B: 51, A: 255},
			BorderColor: Color{R: 200, G: 120, B: 20, A: 255},
			TextColor:   Color{R: 255, G: 255, B: 255, A: 255},
			FontSize:    Text14,
		}
		return img, op
	}
	paintsBorder := func(img *image.RGBA) bool {
		// The border stroke writes on the top row outside the fill's edge;
		// with a 1px border the pixel at (0,0) differs from the pure fill.
		fill := img.Pix[offsetRGBA(img, 20, 10)]
		edge := img.Pix[offsetRGBA(img, 0, 0)]
		return edge != fill
	}

	img, missing := newImg()
	// Background resolved but border-width/opacity missing: the documented
	// fallback adds a visible border.
	missing.Fields = uint32(StyleBackground)
	renderTextInput(img, missing)
	if !paintsBorder(img) {
		t.Fatal("text field with missing border-width lost its fallback border")
	}

	img, explicit := newImg()
	explicit.Fields = uint32(StyleBorderWidth | StyleOpacity | StyleBackground)
	explicit.BorderWidth = 0 // explicit zero: no border at all
	renderTextInput(img, explicit)
	if paintsBorder(img) {
		t.Fatal("explicit zero border-width was overridden by the fallback")
	}
}

func offsetRGBA(img *image.RGBA, x, y int) int {
	return (y-img.Rect.Min.Y)*img.Stride + (x-img.Rect.Min.X)*4
}

func TestStyleFrameRectOpCarriesPresenceBits(t *testing.T) {
	frame := StyleFrame{Value: StyleData{
		Fields:        uint32(StyleBackground | StyleBorderWidth | StyleOpacity | StyleBackgroundEnd),
		Background:    0x11223300,
		BackgroundEnd: 0x44556600,
		BorderWidth:   0,
		Opacity:       0,
	}}
	op := styleFrameRectOp(Rectangle{X: 1, Y: 2, Width: 3, Height: 4}, Rectangle{}, frame)
	if op.Fields != frame.Value.Fields {
		t.Fatalf("frame op fields = %#x, want %#x", op.Fields, frame.Value.Fields)
	}
	if !op.HasBackgroundEnd {
		t.Fatal("frame op lost explicit background-end presence")
	}
	if op.BorderWidth != 0 || op.Opacity != 0 {
		t.Fatalf("explicit zero border/opacity changed: %+v", op)
	}
}

package kryon

import (
	"bytes"
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
	// Background alone must not invent a border width.
	missing.Fields = uint32(StyleBackground)
	renderTextInput(img, missing)
	if paintsBorder(img) {
		t.Fatal("text field invented a border width")
	}
	img, requested := newImg()
	requested.Fields = uint32(StyleBorderWidth | StyleBackground)
	requested.BorderWidth = 1
	renderTextInput(img, requested)
	if !paintsBorder(img) {
		t.Fatal("explicit border width was lost")
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

func TestRenderTextEditorsWithoutStyleDoNotInventChrome(t *testing.T) {
	background := RenderFrame(96, 60, nil)
	for _, kind := range []FrameOpKind{FrameOpTextField, FrameOpTextArea} {
		op := FrameOp{Kind: kind, Bounds: NewRectangle(2, 2, 80, 42), FontSize: Text14}
		img := RenderFrame(96, 60, []FrameOp{op})
		if !bytes.Equal(img.Pix, background.Pix) {
			t.Fatalf("%s invented a fill or border", kind)
		}
		op.Focused = true
		img = RenderFrame(96, 60, []FrameOp{op})
		if bytes.Equal(img.Pix, background.Pix) {
			t.Fatalf("%s lost its insertion caret", kind)
		}
		if img.RGBAAt(2, 2) != background.RGBAAt(2, 2) {
			t.Fatalf("%s focus invented a border", kind)
		}
	}
}

func TestRenderTextEditorsRespectTransparentContentAndOpacity(t *testing.T) {
	background := RenderFrame(96, 60, nil)
	for _, kind := range []FrameOpKind{FrameOpTextField, FrameOpTextArea} {
		op := FrameOp{Kind: kind, Bounds: NewRectangle(2, 2, 80, 42), FontSize: Text14,
			Text: "abc", Focused: true, Cursor: 3, SelectionEnd: 3, Fields: uint32(StyleForeground | StyleFocus)}
		img := RenderFrame(96, 60, []FrameOp{op})
		if !bytes.Equal(img.Pix, background.Pix) {
			t.Fatalf("%s replaced explicit transparent content", kind)
		}
		op.Fields |= uint32(StyleOpacity | StyleBackground | StyleBorderWidth)
		op.TextColor, op.CursorColor, op.SelectionColor = BLACK, BLACK, BLACK
		op.Color, op.BorderColor, op.BorderWidth = WHITE, BLACK, 1
		img = RenderFrame(96, 60, []FrameOp{op})
		if !bytes.Equal(img.Pix, background.Pix) {
			t.Fatalf("%s ignored explicit opacity zero", kind)
		}
	}
}

func TestRenderTextInputSelectionUsesContentOrRequestedColor(t *testing.T) {
	for _, kind := range []FrameOpKind{FrameOpTextField, FrameOpTextArea} {
		op := FrameOp{Kind: kind, Bounds: NewRectangle(2, 2, 80, 42), FontSize: Text14,
			Text: "abc", SelectionEnd: 3}
		img := RenderFrame(96, 60, []FrameOp{op})
		foundSelection := false
		for y := 5; y < 36; y++ {
			for x := 8; x < 44; x++ {
				pixel := img.RGBAAt(x, y)
				if pixel.R != pixel.G || pixel.G != pixel.B {
					t.Fatalf("%s invented a colored selection: %+v", kind, pixel)
				}
				if pixel.R > 0 && pixel.R < 180 {
					foundSelection = true
				}
			}
		}
		if !foundSelection {
			t.Fatalf("%s lost its neutral selection highlight", kind)
		}
		op.SelectionColor = Color{210, 80, 20, 255}
		img = RenderFrame(96, 60, []FrameOp{op})
		foundSelection = false
		for y := 5; y < 36; y++ {
			for x := 8; x < 44; x++ {
				foundSelection = foundSelection || img.RGBAAt(x, y) == rgba(op.SelectionColor)
			}
		}
		if !foundSelection {
			t.Fatalf("%s lost requested selection color", kind)
		}
	}
}

func TestRenderLayoutScopesAndTransparentSurfaceDoNotPaint(t *testing.T) {
	background := RenderFrame(96, 60, nil)
	for _, kind := range []FrameOpKind{FrameOpColumn, FrameOpRow, FrameOpStack, FrameOpGroup, FrameOpGrid, FrameOpPage, FrameOpSection, FrameOpSurface} {
		img := RenderFrame(96, 60, []FrameOp{{Kind: kind, Bounds: NewRectangle(2, 2, 80, 42), Opacity: 1}})
		if !bytes.Equal(img.Pix, background.Pix) {
			t.Fatalf("%s painted unrequested chrome", kind)
		}
	}
}

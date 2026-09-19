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

func TestRenderTextInputNoStyleDebugAffordance(t *testing.T) {
	bounds := Rectangle{X: 2, Y: 2, Width: 48, Height: 24}

	unfocused := RenderFrame(64, 36, []FrameOp{{
		Kind:     FrameOpTextField,
		Bounds:   bounds,
		FontSize: Text14,
	}})
	if got, want := unfocused.RGBAAt(2, 2), rgba(Color{144, 152, 164, 255}); got != want {
		t.Fatalf("unstyled text field border = %#v, want gray debug border %#v", got, want)
	}
	if got, want := unfocused.RGBAAt(8, 8), rgba(WHITE); got != want {
		t.Fatalf("unstyled text field fill = %#v, want white debug fill %#v", got, want)
	}

	focused := RenderFrame(64, 36, []FrameOp{{
		Kind:     FrameOpTextField,
		Bounds:   bounds,
		FontSize: Text14,
		Focused:  true,
	}})
	if got, want := focused.RGBAAt(2, 2), rgba(Color{29, 96, 196, 255}); got != want {
		t.Fatalf("focused unstyled text field border = %#v, want blue debug border %#v", got, want)
	}
	if got, want := focused.RGBAAt(10, 12), rgba(Color{144, 152, 164, 255}); got != want {
		t.Fatalf("focused unstyled text field caret = %#v, want gray debug caret %#v", got, want)
	}
}

func TestRenderTextInputNoStyleSelectionFallback(t *testing.T) {
	img := RenderFrame(96, 40, []FrameOp{{
		Kind:           FrameOpTextField,
		Bounds:         Rectangle{X: 2, Y: 2, Width: 80, Height: 28},
		Text:           "abc",
		FontSize:       Text14,
		SelectionStart: 0,
		SelectionEnd:   3,
	}})
	want := rgba(Color{58, 110, 190, 255})
	found := false
	for y := 5; y < 26 && !found; y++ {
		for x := 10; x < 30; x++ {
			if img.RGBAAt(x, y) == want {
				found = true
				break
			}
		}
	}
	if !found {
		t.Fatal("unstyled text field selection did not use the blue debug selection color")
	}

	styledSelection := Color{210, 80, 20, 255}
	img = RenderFrame(96, 40, []FrameOp{{
		Kind:           FrameOpTextField,
		Bounds:         Rectangle{X: 2, Y: 2, Width: 80, Height: 28},
		Text:           "abc",
		FontSize:       Text14,
		SelectionStart: 0,
		SelectionEnd:   3,
		SelectionColor: styledSelection,
	}})
	want = rgba(styledSelection)
	found = false
	for y := 5; y < 26 && !found; y++ {
		for x := 10; x < 30; x++ {
			if img.RGBAAt(x, y) == want {
				found = true
				break
			}
		}
	}
	if !found {
		t.Fatal("styled text field selection did not override the debug selection color")
	}
}

func TestRenderTextAreaNoStyleDebugAffordance(t *testing.T) {
	img := RenderFrame(96, 60, []FrameOp{{
		Kind:           FrameOpTextArea,
		Bounds:         Rectangle{X: 2, Y: 2, Width: 80, Height: 42},
		Text:           "abc",
		FontSize:       Text14,
		Cursor:         3,
		Focused:        true,
		SelectionStart: 0,
		SelectionEnd:   3,
	}})
	if got, want := img.RGBAAt(8, 8), rgba(RAYWHITE); got != want {
		t.Fatalf("fully unstyled text area fill = %#v, want untouched frame background %#v", got, want)
	}
	selection := rgba(Color{58, 110, 190, 255})
	cursor := rgba(Color{144, 152, 164, 255})
	foundSelection := false
	foundCursor := false
	for y := 5; y < 36; y++ {
		for x := 8; x < 44; x++ {
			pixel := img.RGBAAt(x, y)
			if pixel == selection {
				foundSelection = true
			}
			if pixel == cursor {
				foundCursor = true
			}
		}
	}
	if !foundSelection {
		t.Fatal("unstyled text area selection did not use the blue debug selection color")
	}
	if !foundCursor {
		t.Fatal("unstyled text area caret did not use the gray debug cursor color")
	}
}

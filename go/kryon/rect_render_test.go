package kryon

import (
	"bytes"
	"image"
	"os"
	"testing"

	xfont "golang.org/x/image/font"
	"golang.org/x/image/math/fixed"
)

func TestFontTextPreservesStraightAlpha(t *testing.T) {
	data, err := os.ReadFile("../../fonts/noto/NotoSans-SemiBold.ttf")
	if err != nil {
		t.Fatal(err)
	}
	id, ok := registerFontData("test-alpha-semibold", ".ttf", data)
	if !ok {
		t.Fatal("could not register test face")
	}
	for _, alpha := range []uint8{0, 64, 128, 255} {
		img := image.NewRGBA(image.Rect(0, 0, 100, 40))
		if !drawFontText(img, "Run", 2, 2, 24, Color{255, 180, 90, alpha}, id) {
			t.Fatal("font text was not drawn")
		}
		visible := false
		for y := 0; y < 40; y++ {
			for x := 0; x < 100; x++ {
				pixel := img.RGBAAt(x, y)
				if pixel.R > pixel.A || pixel.G > pixel.A || pixel.B > pixel.A || pixel.A > alpha {
					t.Fatalf("alpha %d: invalid premultiplied glyph pixel at %d,%d: %v", alpha, x, y, pixel)
				}
				visible = visible || pixel.A > 0
			}
		}
		if visible != (alpha > 0) {
			t.Fatalf("alpha %d: unexpected glyph visibility %v", alpha, visible)
		}
	}
}

func TestTextFrameBlendsFadeIncludingRotation(t *testing.T) {
	for _, rotation := range []float32{0, 30} {
		for _, alpha := range []uint8{0, 64, 128, 255} {
			op := FrameOp{Kind: FrameOpText, Text: "Run", FontSize: 24,
				Bounds: Rectangle{X: 24, Y: 8, Width: 70, Height: 30},
				Color:  Color{255, 255, 255, alpha}, Rotation: rotation}
			img := RenderFrame(120, 80, []FrameOp{{Kind: FrameOpBackground, Color: BLACK}, op})
			visible := false
			for y := 0; y < 80; y++ {
				for x := 0; x < 120; x++ {
					pixel := img.RGBAAt(x, y)
					if pixel.A != 255 || pixel.R > alpha || pixel.G > alpha || pixel.B > alpha {
						t.Fatalf("rotation %v alpha %d: text did not composite onto opaque background: %v", rotation, alpha, pixel)
					}
					visible = visible || pixel.R > 0
				}
			}
			if visible != (alpha > 0) {
				t.Fatalf("rotation %v alpha %d: unexpected text visibility", rotation, alpha)
			}
		}
	}
}

func TestTextTypefaceMeasuresAndPaintsWithoutLeaking(t *testing.T) {
	data, err := os.ReadFile("../../fonts/noto/NotoSans-SemiBold.ttf")
	if err != nil {
		t.Fatal(err)
	}
	id, ok := registerFontData("test-semibold", ".ttf", data)
	if !ok {
		t.Fatal("could not register the semibold test face")
	}
	r := New(AppConfig{Width: 300, Height: 100}).(*runtime)
	r.BeginFrame()
	defer r.EndFrame()
	props := TextProps{Text: "Actions", Font: 26, Typeface: "test-semibold", Wrap: TextWrapNone}
	r.Text(props)
	props.Typeface = "missing-face"
	r.Text(props)
	ops := r.FrameOps()
	selected, fallback := ops[len(ops)-2], ops[len(ops)-1]
	if selected.FontID != id || fallback.FontID != 0 {
		t.Fatal("a per-node typeface must not leak or replace the unknown-name fallback")
	}
	if selected.Bounds.Width != float32(runtimeTextWidthWithFont("Actions", 26, id)) {
		t.Fatal("intrinsic measurement must use the selected typeface")
	}
	got := RenderFrame(300, 100, []FrameOp{selected})
	want := image.NewRGBA(image.Rect(0, 0, 300, 100))
	fillImage(want, RAYWHITE)
	drawText(want, selected.Text, int(selected.Bounds.X), int(selected.Bounds.Y), selected.FontSize, selected.Color, id)
	if !bytes.Equal(got.Pix, want.Pix) {
		t.Fatal("painting must use the same face as measurement")
	}
}

func TestTextLetterSpacingMeasuresCodepointsAndWraps(t *testing.T) {
	r := New(AppConfig{Width: 300, Height: 100}).(*runtime)
	r.BeginFrame()
	natural := runtimeTextWidth("AéB", 18)
	r.Text(TextProps{Text: "AéB", Font: 18, LetterSpacing: 3, Wrap: TextWrapNone})
	ops := r.FrameOps()
	op := ops[len(ops)-1]
	if op.Bounds.Width != float32(natural+6) || op.LetterSpacing != 3 {
		t.Fatalf("tracking must add two gaps, not UTF-8 byte gaps: %+v", op)
	}
	r.Text(TextProps{Bounds: Rectangle{X: 10, Y: 30, Width: 200, Height: 30},
		Text: "AB", Font: 18, LetterSpacing: 4, Align: TextAlignCenter, Wrap: TextWrapNone})
	ops = r.FrameOps()
	op = ops[len(ops)-1]
	wantWidth := float32(runtimeTextWidth("AB", 18) + 4)
	if op.Bounds.X != 10+(200-wantWidth)/2 {
		t.Fatal("center alignment ignored tracking")
	}
	width := float32(runtimeTextWidth("AA BB", 18))
	before := len(ops)
	r.Text(TextProps{Bounds: Rectangle{X: 10, Y: 60, Width: width, Height: 50},
		Text: "AA BB", Font: 18, LetterSpacing: 3, Wrap: TextWrapAuto})
	ops = r.FrameOps()
	if len(ops)-before != 2 || ops[before].Text != "AA" || ops[before+1].Text != "BB" {
		t.Fatal("tracked text did not wrap using its measured width")
	}
	r.EndFrame()
}

func TestTextLetterSpacingPaintMatchesMeasurement(t *testing.T) {
	actual := image.NewRGBA(image.Rect(0, 0, 100, 40))
	expected := image.NewRGBA(image.Rect(0, 0, 100, 40))
	drawText(actual, "AB", 4, 4, 18, White, 0, 4)
	drawText(expected, "A", 4, 4, 18, White, 0)
	drawText(expected, "B", 4+runtimeTextWidth("A", 18)+4, 4, 18, White, 0)
	if !bytes.Equal(actual.Pix, expected.Pix) {
		t.Fatal("painted glyph positions differ from measured tracking")
	}
}

func TestFontAdvanceTruncatesBeforePixelQuantization(t *testing.T) {
	if got := fontUnitAdvance(fixed.I(799), 0.01); got != fixed.I(7) {
		t.Fatalf("7.99-pixel advance became %v instead of 7 pixels", got)
	}
}

func TestTextFrameTruncatesFractionalOriginLikeC(t *testing.T) {
	ensureDefaultUIFont()
	op := FrameOp{Kind: FrameOpText, Bounds: Rectangle{X: 8.75, Y: 5.75, Width: 80, Height: 24},
		Text: "Primary", FontSize: 18, Color: Black}
	actual := RenderFrame(100, 40, []FrameOp{op})
	op.Bounds.X, op.Bounds.Y = 8, 5
	expected := RenderFrame(100, 40, []FrameOp{op})
	if !bytes.Equal(actual.Pix, expected.Pix) {
		t.Fatal("text origins must use C's integer truncation")
	}
}

func TestFontDrawingAndCaretUseSameIntegerAdvances(t *testing.T) {
	ensureDefaultUIFont()
	face := faceForFont(0, 18)
	if face == nil {
		t.Fatal("default test font is missing")
	}
	for _, text := range []string{"AV", "Run", "MAGNETIC LIGHTFIELD"} {
		want := 0
		for _, glyph := range text {
			advance, _ := face.GlyphAdvance(glyph)
			want += advance.Floor()
		}
		measured, ok := measureFontText(text, 18, 0)
		caret, caretOK := fontTextAdvance(text, int32(len(text)), 18, 0)
		if !ok || !caretOK || measured.X != float32(want) || caret != want {
			t.Fatalf("measurement/caret for %q must share C's integer glyph advances", text)
		}
	}
}

func TestButtonTextCenterRoundsHalfPixelLikeC(t *testing.T) {
	ensureDefaultUIFont()
	face := faceForFont(0, 18)
	if face == nil {
		t.Fatal("default test font is missing")
	}
	bounds, _ := xfont.BoundString(face, "Run")
	top := bounds.Min.Y.Floor() + fontAscent(face)
	height := bounds.Max.Y.Ceil() - bounds.Min.Y.Floor()
	for extra := 20; extra <= 23; extra++ {
		want := 160 + (extra+1)/2 - top
		if got := fontTextBaseline("Run", 160, height+extra, 18, 0); got != want {
			t.Fatalf("extra height %d: baseline %d, want %d", extra, got, want)
		}
	}
}

func TestTextClipsPartiallyVisibleLineInsteadOfDroppingIt(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	r.Text(TextProps{Bounds: Rectangle{X: 2, Y: 2, Width: 100, Height: 8}, Text: "Heading", Font: 28, Wrap: TextWrapNone})
	found := false
	for _, op := range r.FrameOps() {
		if op.Kind == FrameOpText && op.Text == "Heading" {
			found = true
			if !op.HasClip || op.Clip.Height != 8 {
				t.Fatal("partial text needs its original clip")
			}
		}
	}
	if !found {
		t.Fatal("partially visible line was discarded")
	}
}

func TestRectBorderIsNotGradient(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.BeginFrame()
	r.Rect(0, 0, 24, 24, White)
	r.Rect(4, 4, 16, 16, RED, BLUE)
	img := RenderFrame(24, 24, r.FrameOps())
	if img.RGBAAt(12, 12).R != RED.R || img.RGBAAt(5, 12) != img.RGBAAt(18, 12) {
		t.Fatal("border color must not become a gradient across the face")
	}
	if img.RGBAAt(4, 12).B != BLUE.B {
		t.Fatal("rectangle border is missing")
	}
	r.Rect(6, 6, 12, 12, Color{})
	after := RenderFrame(24, 24, r.FrameOps())
	if after.RGBAAt(12, 12) != img.RGBAAt(12, 12) {
		t.Fatal("transparent rectangle must preserve its backdrop")
	}
}

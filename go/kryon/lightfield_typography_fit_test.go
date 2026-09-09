package kryon

import (
	"image"
	"image/draw"
	"image/png"
	"math"
	"os"
	"testing"
)

// Read-only diagnostic: report native text parameters, never rewrite a golden
// image or change the example automatically. Enable explicitly for design work.
func TestFitLightfieldTypographyDiagnostic(t *testing.T) {
	if os.Getenv("KRYON_FIT_TYPOGRAPHY") != "1" {
		t.Skip("opt-in reference analysis")
	}
	file, err := os.Open("../../design/widget-proposals/08-magnetic-lightfield.png")
	if err != nil {
		t.Fatal(err)
	}
	defer file.Close()
	decoded, err := png.Decode(file)
	if err != nil {
		t.Fatal(err)
	}
	if decoded.Bounds().Dx() != 1536 || decoded.Bounds().Dy() != 1024 {
		t.Fatal("the original reference must not be resized")
	}
	reference := image.NewRGBA(decoded.Bounds())
	draw.Draw(reference, reference.Bounds(), decoded, decoded.Bounds().Min, draw.Src)
	ensureDefaultUIFont()
	face := registeredTypeface("semibold")
	if face == 0 {
		t.Fatal("bundled semibold typeface is required")
	}
	for _, light := range []bool{false, true} {
		name, offset, label := "dark", 0, "DARK THEME"
		caption := "LIGHT CREATES DEPTH"
		background, foreground := Color{0, 25, 52, 255}, ThemeDefaultDark().Colors.Text
		if light {
			name, offset, label = "light", 768, "LIGHT THEME"
			caption = "DEPTH THROUGH LIGHT"
			background, foreground = Color{250, 252, 254, 255}, ThemeDefaultLight().Colors.Text
		}
		for _, sample := range []struct {
			text    string
			box     image.Rectangle
			y       int
			caption bool
		}{
			{label, image.Rect(16, 80, 355, 106), 81, false},
			{"SIZES", image.Rect(16, 476, 200, 503), 476, false},
			{"ICON VARIANTS", image.Rect(16, 678, 350, 709), 680, false},
			{"FULL WIDTH EXAMPLES", image.Rect(16, 870, 440, 900), 873, false},
			{caption, image.Rect(16, 108, 355, 126), 108, true},
		} {
			best := uint64(math.MaxUint64)
			var bestSize, bestSpacing, bestX, bestY int
			bestColor := "text"
			type colorCandidate struct {
				name  string
				value Color
			}
			colors := []colorCandidate{{"text", foreground}}
			minSize, maxSize, maxSpacing, selectedFace := 23, 29, 3, face
			if sample.caption {
				minSize, maxSize, maxSpacing, selectedFace = 12, 16, 7, 0
				theme := ThemeDefaultDark()
				if light {
					theme = ThemeDefaultLight()
				}
				colors = append(colors,
					colorCandidate{"icon", theme.Colors.Icon},
					colorCandidate{"link", theme.Colors.Link},
					colorCandidate{"link_hover", theme.Colors.LinkHover})
			}
			for _, color := range colors {
				for size := minSize; size <= maxSize; size++ {
					for spacing := 0; spacing <= maxSpacing; spacing++ {
						for x := 24; x <= 29; x++ {
							for y := sample.y - 2; y <= sample.y+2; y++ {
								candidate := RenderFrame(sample.box.Dx(), sample.box.Dy(), []FrameOp{
									{Kind: FrameOpBackground, Color: background},
									{Kind: FrameOpText, Text: sample.text, FontSize: int32(size),
										LetterSpacing: int32(spacing), FontID: selectedFace, Color: color.value,
										Bounds: Rectangle{X: float32(x - sample.box.Min.X), Y: float32(y - sample.box.Min.Y)}},
								})
								var errorSum uint64
								for py := 0; py < sample.box.Dy(); py++ {
									for px := 0; px < sample.box.Dx(); px++ {
										a := candidate.PixOffset(px, py)
										b := reference.PixOffset(px+sample.box.Min.X+offset, py+sample.box.Min.Y)
										for channel := 0; channel < 3; channel++ {
											difference := int(candidate.Pix[a+channel]) - int(reference.Pix[b+channel])
											errorSum += uint64(difference * difference)
										}
									}
								}
								if errorSum < best {
									best = errorSum
									bestSize, bestSpacing, bestX, bestY = size, spacing, x, y
									bestColor = color.name
								}
							}
						}
					}
				}
			}
			rmse := math.Sqrt(float64(best) / float64(sample.box.Dx()*sample.box.Dy()*3))
			t.Logf("%s %q: size=%d spacing=%d x=%d y=%d color=%s candidate_rmse=%.3f",
				name, sample.text, bestSize, bestSpacing, bestX, bestY, bestColor, rmse)
		}
	}
}

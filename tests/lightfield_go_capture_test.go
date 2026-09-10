package lightfield

import (
	"bytes"
	"image"
	"image/png"
	"math"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/waozixyz/kryon/go/kryon"
)

// Compiled beside the transpiled example by lightfield-go-capture. This runs
// the actual .kry screen through native Go, without a C bridge or a window.
func colorValue(value uint32) kryon.Color {
	return kryon.Color{R: uint8(value >> 24), G: uint8(value >> 16), B: uint8(value >> 8), A: uint8(value)}
}

func TestCaptureLightfield(t *testing.T) {
	directory := os.Getenv("KRYON_LIGHTFIELD_CAPTURE_DIR")
	if directory == "" {
		t.Fatal("KRYON_LIGHTFIELD_CAPTURE_DIR is required")
	}
	if err := os.MkdirAll(directory, 0755); err != nil {
		t.Fatal(err)
	}
	for _, theme := range []struct {
		name string
		mode kryon.ThemeMode
	}{
		{"light", kryon.ThemeModeLight}, {"dark", kryon.ThemeModeDark},
	} {
		epoch := time.Unix(1, 0)
		captureTime := epoch
		runtime := kryon.New(kryon.AppConfig{Width: 768, Height: 1024,
			FrameClock: func() time.Time { return captureTime }})
		kryon.SetRuntime(runtime)
		kryon.SetThemeMode(theme.mode)
		input := runtime.(interface {
			QueueMouseMove(float32, float32)
			QueueMouseButtonDown(int32, float32, float32)
			QueueMouseButtonUp(int32, float32, float32)
			SetFocus(int32)
		})
		state := M02ButtonsState{}
		drawFrame := func(frame int) {
			captureTime = epoch.Add(time.Duration(frame) * time.Second / 60)
			kryon.BeginFrame()
			if frame == 63 || frame == 123 {
				input.SetFocus(0)
			}
			if frame == 73 {
				input.SetFocus(3000)
			}
			if frame == 133 {
				input.SetFocus(5000)
			}
			M02Buttons_ButtonsExample(&state, kryon.Rectangle{Width: 768, Height: 1024})
			kryon.EndFrame()
		}
		input.QueueMouseMove(-100, -100)
		for frame := 0; frame <= 23; frame++ {
			drawFrame(frame)
		}
		heading := false
		variantHeadings := map[string]bool{
			"Primary": false, "Secondary": false, "Outline": false, "Ghost": false,
			"Danger": false, "Success": false, "Warning": false, "Link": false,
		}
		rowLabels := map[string]float32{
			"Normal": 169, "Hover": 215, "Pressed": 265,
			"Focus": 316, "Disabled": 370, "Loading": 419,
		}
		if theme.name == "light" {
			rowLabels["Disabled"] = 371
		}
		sizeCaptions := map[string]float32{"Small": 518, "Medium": 561, "Large": 617}
		if theme.name == "light" {
			sizeCaptions["Medium"] = 562
		}
		iconCaptions := map[string]float32{
			"Text only": 30, "Icon leading": 114, "Icon trailing": 209, "Icon only": 294,
			"Pill": 402, "Square": 481, "Split button": 557, "Dropdown": 671,
		}
		if theme.name == "light" {
			iconCaptions["Text only"] = 33
			iconCaptions["Icon leading"] = 111
			iconCaptions["Icon trailing"] = 206
			iconCaptions["Icon only"] = 293
			iconCaptions["Square"] = 480
			iconCaptions["Dropdown"] = 670
		}
		caption := false
		captionLabel := "LIGHT CREATES DEPTH"
		captionColor := kryon.ThemeDefaultDark().Colors.LinkHover
		if theme.name == "light" {
			captionLabel = "DEPTH THROUGH LIGHT"
			captionColor = kryon.ThemeDefaultLight().Colors.LinkHover
		}
		footer := false
		footerLabel := "INTELLIGENT DETAILS. LASTING IMPACT."
		if theme.name == "light" {
			footerLabel = "KRYON / 2040 / BUILD WHAT'S NEXT"
		}
		columnX := [...]float32{81, 166, 250, 334, 419, 503, 587, 671}
		rowY := [...]float32{161, 206, 254, 305, 359, 409}
		rowHeight := [...]float32{34, 38, 36, 38, 38, 40}
		if theme.name == "light" {
			columnX = [...]float32{81, 166, 251, 336, 421, 506, 591, 674}
			rowY[0], rowY[2] = 160, 253
			rowHeight[1] = 36
		}
		stateButtons := 0
		sizeButtons := 0
		fullWidthButtons := 0
		iconButtons := 0
		iconBounds := map[int32]kryon.Rectangle{
			3000: {X: 24, Y: 747, Width: 70, Height: 40},
			3020: {X: 24, Y: 801, Width: 70, Height: 40},
			3001: {X: 104, Y: 747, Width: 85, Height: 41},
			3021: {X: 104, Y: 801, Width: 83, Height: 40},
			3002: {X: 201, Y: 747, Width: 82, Height: 41},
			3022: {X: 200, Y: 800, Width: 82, Height: 43},
			3004: {X: 360, Y: 747, Width: 99, Height: 41},
			3024: {X: 360, Y: 800, Width: 100, Height: 43},
			3003: {X: 296.5, Y: 746.5, Width: 43, Height: 43},
			3023: {X: 297, Y: 800, Width: 43, Height: 43},
			3005: {X: 479, Y: 747, Width: 42, Height: 42},
			3025: {X: 479, Y: 800, Width: 43, Height: 43},
			3006: {X: 540, Y: 748, Width: 58, Height: 40},
			3007: {X: 598, Y: 748, Width: 40, Height: 40},
			3008: {X: 657, Y: 748, Width: 88, Height: 40},
			3026: {X: 540, Y: 801, Width: 58, Height: 40},
			3027: {X: 598, Y: 801, Width: 40, Height: 40},
			3028: {X: 657, Y: 801, Width: 88, Height: 40},
		}
		if theme.name == "light" {
			iconBounds[3000] = kryon.Rectangle{X: 23, Y: 747, Width: 70, Height: 41}
			iconBounds[3020] = kryon.Rectangle{X: 23, Y: 801, Width: 70, Height: 40}
			iconBounds[3001] = kryon.Rectangle{X: 101, Y: 747, Width: 84, Height: 41}
			iconBounds[3021] = kryon.Rectangle{X: 101, Y: 801, Width: 84, Height: 40}
			iconBounds[3002] = kryon.Rectangle{X: 198, Y: 747, Width: 83, Height: 41}
			iconBounds[3022] = kryon.Rectangle{X: 200, Y: 802, Width: 81, Height: 39}
			iconBounds[3024] = kryon.Rectangle{X: 360, Y: 801, Width: 99, Height: 41}
			iconBounds[3003] = kryon.Rectangle{X: 297, Y: 748, Width: 40, Height: 40}
			iconBounds[3023] = kryon.Rectangle{X: 296, Y: 802, Width: 40, Height: 40}
			iconBounds[3005] = kryon.Rectangle{X: 479, Y: 748, Width: 40, Height: 40}
			iconBounds[3025] = kryon.Rectangle{X: 479, Y: 802, Width: 40, Height: 40}
			iconBounds[3006] = kryon.Rectangle{X: 538, Y: 747, Width: 58, Height: 40}
			iconBounds[3007] = kryon.Rectangle{X: 596, Y: 747, Width: 40, Height: 40}
			iconBounds[3008] = kryon.Rectangle{X: 656, Y: 747, Width: 88, Height: 40}
			iconBounds[3026] = kryon.Rectangle{X: 538, Y: 801, Width: 58, Height: 40}
			iconBounds[3027] = kryon.Rectangle{X: 596, Y: 801, Width: 40, Height: 40}
			iconBounds[3028] = kryon.Rectangle{X: 656, Y: 801, Width: 88, Height: 40}
		}
		sizeFont := [...]int32{15, 18, 18}
		sizeLabelX := [...]float32{-1, 0, 0}
		if theme.name == "light" {
			sizeFont[1], sizeFont[2] = 19, 19
			sizeLabelX[1], sizeLabelX[2] = -1, -1
		}
		seenButtonIDs := make(map[int32]bool)
		for _, op := range runtime.(interface{ FrameOps() []kryon.FrameOp }).FrameOps() {
			if op.Kind == kryon.FrameOpButton {
				if seenButtonIDs[op.ID] {
					t.Fatalf("%s: button ID %d is shared by separate controls", theme.name, op.ID)
				}
				seenButtonIDs[op.ID] = true
				switch op.ID {
				case 1003, 1013, 1023, 1033, 2003, 2013, 2023, 1053:
					want := kryon.ThemeDefaultDark().Colors.Text
					if theme.name == "light" {
						want = kryon.ThemeDefaultLight().Colors.Link
						if op.Button.Props.Loading {
							want = kryon.ThemeDefaultLight().Colors.Accent
						}
					}
					if colorValue(op.Button.Appearance.Value.Foreground) != want {
						t.Fatalf("%s ghost %d: ink %+v, want %+v", theme.name, op.ID, colorValue(op.Button.Appearance.Value.Foreground), want)
					}
				}
			}
			if expected, ok := iconBounds[op.ID]; ok && op.Kind == kryon.FrameOpButton {
				if op.Bounds != expected {
					t.Fatalf("icon-row button %d: bounds=%+v, want %+v", op.ID, op.Bounds, expected)
				}
				if op.ID == 3003 || op.ID == 3023 || op.ID == 3005 || op.ID == 3025 {
					wantIconSize := float32(18)
					if op.ID == 3023 {
						wantIconSize = 20
					}
					if op.Button.Appearance.Value.IconSize != wantIconSize {
						t.Fatalf("icon-row button %d: icon size=%g, want %g", op.ID, op.Button.Appearance.Value.IconSize, wantIconSize)
					}
				}
				iconButtons++
			}
			if op.Kind == kryon.FrameOpButton && (op.ID == 5000 || op.ID == 5001) {
				font, offsetX, top, height := int32(18), float32(-13), float32(901), float32(34)
				if theme.name == "light" {
					font, offsetX = 19, -3
				}
				if op.ID == 5001 {
					font, offsetX, top, height = 20, -15, 945, 38
					if theme.name == "light" {
						offsetX = -8
					}
				}
				if op.Button.Font != font || (kryon.Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}) != (kryon.Vector2{X: offsetX, Y: -1}) ||
					op.Bounds != (kryon.Rectangle{X: 24, Y: top, Width: 720, Height: height}) {
					t.Fatalf("full-width content lost its reference layout: id=%d font=%d offset=%+v bounds=%+v",
						op.ID, op.Button.Font, (kryon.Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}), op.Bounds)
				}
				fullWidthButtons++
			}
			if op.Kind == kryon.FrameOpButton && op.ID >= 2000 && op.ID <= 2027 {
				row, column := (op.ID-2000)/10, (op.ID-2000)%10
				if column >= 8 {
					t.Fatalf("unexpected size-grid column for %d", op.ID)
				}
				large := [8]kryon.Rectangle{
					{X: 81, Y: 599, Width: 75, Height: 49}, {X: 166, Y: 599, Width: 75, Height: 49},
					{X: 250, Y: 599, Width: 75, Height: 49}, {X: 333, Y: 599, Width: 75, Height: 49},
					{X: 418, Y: 599, Width: 74, Height: 49}, {X: 502, Y: 599, Width: 74, Height: 49},
					{X: 585, Y: 599, Width: 74, Height: 49}, {X: 669, Y: 599, Width: 75, Height: 49},
				}
				if theme.name == "light" {
					large = [8]kryon.Rectangle{
						{X: 86, Y: 600, Width: 72, Height: 51}, {X: 166, Y: 601, Width: 74, Height: 48},
						{X: 252, Y: 599, Width: 74, Height: 51}, {X: 336, Y: 601, Width: 74, Height: 49},
						{X: 421, Y: 601, Width: 74, Height: 49}, {X: 506, Y: 601, Width: 74, Height: 48},
						{X: 590, Y: 601, Width: 74, Height: 48}, {X: 674, Y: 601, Width: 74, Height: 49},
					}
				}
				want := large[column]
				if row == 0 {
					small := [8]kryon.Rectangle{
						{X: 91, Y: 511, Width: 56, Height: 24}, {X: 175, Y: 511, Width: 57, Height: 25},
						{X: 260, Y: 511, Width: 57, Height: 24}, {X: 343, Y: 511, Width: 56, Height: 25},
						{X: 428, Y: 511, Width: 56, Height: 24}, {X: 512, Y: 511, Width: 56, Height: 24},
						{X: 594, Y: 511, Width: 56, Height: 24}, {X: 677, Y: 511, Width: 56, Height: 25},
					}
					if theme.name == "light" {
						small = [8]kryon.Rectangle{
							{X: 94, Y: 511, Width: 56, Height: 24}, {X: 176, Y: 511, Width: 56, Height: 24},
							{X: 262, Y: 511, Width: 57, Height: 25}, {X: 345, Y: 511, Width: 56, Height: 25},
							{X: 430, Y: 511, Width: 56, Height: 25}, {X: 514, Y: 511, Width: 56, Height: 25},
							{X: 598, Y: 511, Width: 56, Height: 25}, {X: 681, Y: 511, Width: 56, Height: 25},
						}
					}
					want = small[column]
				}
				if row == 1 {
					medium := [8]kryon.Rectangle{
						{X: 83, Y: 548, Width: 71, Height: 38}, {X: 168, Y: 548, Width: 71, Height: 39},
						{X: 253, Y: 548, Width: 70, Height: 38}, {X: 335, Y: 548, Width: 71, Height: 39},
						{X: 420, Y: 548, Width: 70, Height: 39}, {X: 504, Y: 548, Width: 70, Height: 38},
						{X: 587, Y: 548, Width: 70, Height: 38}, {X: 671, Y: 548, Width: 70, Height: 39},
					}
					if theme.name == "light" {
						medium = [8]kryon.Rectangle{
							{X: 87, Y: 548, Width: 70, Height: 41}, {X: 168, Y: 549, Width: 70, Height: 38},
							{X: 254, Y: 550, Width: 71, Height: 38}, {X: 338, Y: 549, Width: 70, Height: 38},
							{X: 423, Y: 549, Width: 70, Height: 39}, {X: 508, Y: 549, Width: 70, Height: 39},
							{X: 592, Y: 549, Width: 71, Height: 39}, {X: 676, Y: 549, Width: 70, Height: 38},
						}
					}
					want = medium[column]
				}
				labelY := float32(0)
				if theme.name == "dark" && row == 2 {
					labelY = 1
				}
				if op.Bounds != want || op.Button.Font != sizeFont[row] || (kryon.Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}) != (kryon.Vector2{X: sizeLabelX[row], Y: labelY}) {
					t.Fatalf("size button %d: bounds=%+v font=%d offset=%+v; want bounds=%+v font=%d offsetX=%g", op.ID, op.Bounds, op.Button.Font, (kryon.Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}), want, sizeFont[row], sizeLabelX[row])
				}
				sizeButtons++
			}
			if _, variant := variantHeadings[op.Text]; variant && op.Kind == kryon.FrameOpText {
				variantHeadings[op.Text] = true
				if op.FontSize != 17 {
					t.Fatalf("%s heading uses font %d, want 17", op.Text, op.FontSize)
				}
				if op.Text == "Warning" {
					want := kryon.ThemeDefaultDark().Colors.Warning
					if theme.name == "light" {
						want = kryon.ThemeDefaultLight().Colors.OnWarning
					}
					if op.Color != want {
						t.Fatalf("%s Warning heading: color %+v, want %+v", theme.name, op.Color, want)
					}
				}
			}
			if y, rowLabel := rowLabels[op.Text]; rowLabel && op.Kind == kryon.FrameOpText {
				if op.FontSize != 17 || op.Bounds.Y != y {
					t.Fatalf("%s row label: font %d, y %g; want 17, %g", op.Text, op.FontSize, op.Bounds.Y, y)
				}
				delete(rowLabels, op.Text)
			}
			if x, iconCaption := iconCaptions[op.Text]; iconCaption && op.Kind == kryon.FrameOpText {
				if op.FontSize != 16 || op.Bounds.X != x || op.Bounds.Y != 720 {
					t.Fatalf("%s caption: font %d bounds %+v; want font 16 at %g,720", op.Text, op.FontSize, op.Bounds, x)
				}
				delete(iconCaptions, op.Text)
			}
			if y, sizeCaption := sizeCaptions[op.Text]; sizeCaption && op.Kind == kryon.FrameOpText {
				if op.FontSize != 17 || op.Bounds.X != 24 || op.Bounds.Y != y {
					t.Fatalf("%s size caption: font %d bounds %+v; want font 17 at 24,%g", op.Text, op.FontSize, op.Bounds, y)
				}
				delete(sizeCaptions, op.Text)
			}
			if op.Kind == kryon.FrameOpText && op.Text == captionLabel {
				caption = true
				if op.FontSize != 13 || op.LetterSpacing != 5 || op.Color != captionColor || op.FontID != 0 {
					t.Fatal("caption lost its theme styling or inherited the heading typeface")
				}
			}
			if op.Kind == kryon.FrameOpText && op.Text == footerLabel {
				footer = true
				if theme.name == "light" && op.Bounds.X < 350 {
					t.Fatal("light footer must be aligned to the right edge")
				}
			}
			if op.Kind == kryon.FrameOpText && (op.Text == "LIGHT THEME" || op.Text == "DARK THEME") {
				heading = true
				if op.FontID == 0 {
					t.Fatal("theme heading did not resolve its semibold typeface")
				}
			}
			if op.Kind == kryon.FrameOpButton && op.ID >= 1000 && op.ID <= 1057 {
				font, offsetY := int32(19), float32(0)
				if theme.name == "light" {
					font, offsetY = 17, 1
				} else if op.ID >= 1010 && op.ID <= 1017 && op.ID != 1013 && op.ID != 1017 {
					offsetY = -1
				}
				if op.Button.Font != font || (kryon.Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}) != (kryon.Vector2{Y: offsetY}) {
					t.Fatalf("button %d lost its shared label style: font=%d offset=%+v", op.ID, op.Button.Font, (kryon.Vector2{X: op.Button.Appearance.Value.OffsetX, Y: op.Button.Appearance.Value.OffsetY}))
				}
				row, column := (op.ID-1000)/10, (op.ID-1000)%10
				if column >= 8 {
					t.Fatalf("unexpected state-grid column for %d", op.ID)
				}
				want := kryon.Rectangle{X: columnX[column], Y: rowY[row], Width: 72, Height: rowHeight[row]}
				if theme.name == "light" {
					if row == 0 && (column == 0 || column == 2) {
						want.Y = 161
					}
					if row == 1 && column == 0 {
						want.Height = 39
					}
					if row == 1 && column == 2 {
						want.Y = 205
						want.Height = 38
					}
				} else {
					if row == 1 && column == 3 {
						want.Height = 36
					}
					if row == 1 && column == 7 {
						want.X = 672
						want.Width = 70
						want.Height = 36
					}
				}
				if row == 3 {
					focused := [8]kryon.Rectangle{
						{X: 80, Y: 305, Width: 75, Height: 38}, {X: 166, Y: 305, Width: 72, Height: 38},
						{X: 250, Y: 305, Width: 73, Height: 38}, {X: 334, Y: 305, Width: 72, Height: 38},
						{X: 419, Y: 305, Width: 73, Height: 38}, {X: 503, Y: 305, Width: 73, Height: 38},
						{X: 587, Y: 305, Width: 73, Height: 39}, {X: 671, Y: 305, Width: 73, Height: 38},
					}
					if theme.name == "light" {
						focused = [8]kryon.Rectangle{
							{X: 80, Y: 305, Width: 74, Height: 38}, {X: 166, Y: 305, Width: 72, Height: 38},
							{X: 252, Y: 305, Width: 72, Height: 38}, {X: 336, Y: 305, Width: 72, Height: 38},
							{X: 421, Y: 305, Width: 72, Height: 38}, {X: 506, Y: 305, Width: 72, Height: 38},
							{X: 590, Y: 305, Width: 74, Height: 38}, {X: 674, Y: 305, Width: 72, Height: 38},
						}
					}
					want = focused[column]
				}
				if op.Bounds != want {
					t.Fatalf("button %d bounds: %+v", op.ID, op.Bounds)
				}
				stateButtons++
			}
		}
		if !footer {
			t.Fatalf("%s theme footer is missing", theme.name)
		}
		if len(iconCaptions) != 0 {
			t.Fatalf("missing icon captions: %v", iconCaptions)
		}
		if len(rowLabels) != 0 {
			t.Fatalf("missing state labels: %v", rowLabels)
		}
		if len(sizeCaptions) != 0 {
			t.Fatalf("missing size captions: %v", sizeCaptions)
		}
		for label, found := range variantHeadings {
			if !found {
				t.Fatalf("%s variant heading is missing", label)
			}
		}
		if !heading {
			t.Fatal("theme heading was dropped from the native Go frame")
		}
		if !caption {
			t.Fatalf("%s theme caption is missing", theme.name)
		}
		if stateButtons != 48 {
			t.Fatalf("state grid contains %d buttons, want 48", stateButtons)
		}
		if sizeButtons != 24 {
			t.Fatalf("size grid contains %d buttons, want 24", sizeButtons)
		}
		if fullWidthButtons != 2 {
			t.Fatalf("full-width section contains %d buttons, want 2", fullWidthButtons)
		}
		if iconButtons != len(iconBounds) {
			t.Fatalf("icon geometry witnesses: got %d, want %d", iconButtons, len(iconBounds))
		}
		ops := runtime.(interface{ FrameOps() []kryon.FrameOp }).FrameOps()
		if os.Getenv("KRYON_FIT_BUTTON_LABELS") == "1" {
			fitButtonLabels(t, theme.name, ops)
		}
		if os.Getenv("KRYON_FIT_BUTTON_GEOMETRY") == "1" {
			fitButtonGeometry(t, theme.name, ops)
		}
		saveLightfieldCapture(t, directory, theme.name, ops)
		stages := map[int]string{
			23: "",
			24: "-hover-start", 28: "-hover-middle", 33: "-hover-end",
			34: "-press-start", 40: "-press-end", 50: "-release-end",
			51: "-exit-start", 62: "-exit-end", 63: "-focus-exit-start",
			72: "-focus-exit-end", 73: "-focus-start", 77: "-focus-middle", 82: "-focus-end",
		}
		for frame := 24; frame <= 142; frame++ {
			sequenceFrame := frame
			name := theme.name
			x, y := float32(58), float32(768)
			if frame >= 83 {
				sequenceFrame -= 60
				name += "-wide"
				x, y = 384, 918
			}
			if sequenceFrame >= 24 && sequenceFrame < 51 {
				input.QueueMouseMove(x, y)
			} else {
				input.QueueMouseMove(-100, -100)
			}
			if sequenceFrame == 34 {
				input.QueueMouseButtonDown(kryon.MouseButtonLeft, x, y)
			}
			if sequenceFrame == 41 {
				input.QueueMouseButtonUp(kryon.MouseButtonLeft, x, y)
			}
			drawFrame(frame)
			if stage, ok := stages[sequenceFrame]; ok {
				ops = runtime.(interface{ FrameOps() []kryon.FrameOp }).FrameOps()
				saveLightfieldCapture(t, directory, name+stage, ops)
			}
		}
	}
}

func TestCaptureLightfieldSplit(t *testing.T) {
	directory := os.Getenv("KRYON_LIGHTFIELD_CAPTURE_DIR")
	epoch := time.Unix(1, 0)
	captureTime := epoch
	runtime := kryon.New(kryon.AppConfig{Width: 1536, Height: 1024,
		FrameClock: func() time.Time { return captureTime }})
	kryon.SetRuntime(runtime)
	kryon.SetThemeMode(kryon.ThemeModeSystem)
	state := M02ButtonsState{}
	for frame := 0; frame < 24; frame++ {
		captureTime = epoch.Add(time.Duration(frame) * time.Second / 60)
		kryon.BeginFrame()
		M02Buttons_ButtonsExample(&state, kryon.Rectangle{Width: 1536, Height: 1024})
		kryon.EndFrame()
	}
	ops := runtime.(interface{ FrameOps() []kryon.FrameOp }).FrameOps()
	buttons := map[int32]kryon.FrameOp{}
	for _, op := range ops {
		if op.Kind == kryon.FrameOpButton {
			if _, exists := buttons[op.ID]; exists {
				t.Fatalf("split panels share button ID %d", op.ID)
			}
			buttons[op.ID] = op
		}
	}
	left, right := buttons[1000], buttons[11000]
	if left.Bounds.X != 81 || right.Bounds.X != 849 || left.Button.Material.Ambient == right.Button.Material.Ambient {
		t.Fatalf("split panels lost position or theme: left %+v right %+v", left, right)
	}
	if kryon.GetThemeMode() != kryon.ThemeModeSystem {
		t.Fatal("split panel rendering replaced the system theme preference")
	}
	headerColumns := 0
	darkPanelEdge := false
	for _, op := range ops {
		if op.Kind == kryon.FrameOpSurface && op.Bounds.X == 4 && op.Bounds.Y == 74 {
			want := kryon.ThemeDefaultDark().Colors.Link
			want.A = 80
			if op.BorderColor != want {
				t.Fatalf("dark panel lost its translucent theme edge: %+v", op.BorderColor)
			}
			darkPanelEdge = true
		}
		if op.Kind != kryon.FrameOpSurface || op.Bounds.Y != 0 || op.Bounds.Width != 1 {
			continue
		}
		if op.Bounds.X != float32(headerColumns) || op.Bounds.Height != 74 || !op.HasBackgroundEnd {
			t.Fatalf("discontinuous header column %d: %+v", headerColumns, op)
		}
		if headerColumns == 0 && (op.Color != (kryon.Color{0, 25, 54, 255}) ||
			op.BackgroundEnd != (kryon.Color{0, 24, 52, 255})) {
			t.Fatal("header lost its dark endpoint")
		}
		if headerColumns == 1535 && (op.Color != (kryon.Color{18, 58, 101, 255}) ||
			op.BackgroundEnd != (kryon.Color{12, 49, 89, 255})) {
			t.Fatal("header lost its light endpoint")
		}
		headerColumns++
	}
	if headerColumns != 1536 {
		t.Fatalf("header has %d columns, want 1536", headerColumns)
	}
	if !darkPanelEdge {
		t.Fatal("split view lost its dark panel surface")
	}
	output, err := os.Create(filepath.Join(directory, "split.png"))
	if err != nil {
		t.Fatal(err)
	}
	err = png.Encode(output, kryon.RenderFrame(1536, 1024, ops))
	closeErr := output.Close()
	if err != nil || closeErr != nil {
		t.Fatalf("write split capture: %v %v", err, closeErr)
	}
	input := runtime.(interface {
		QueueMouseMove(float32, float32)
		QueueMouseButtonDown(int32, float32, float32)
		QueueMouseButtonUp(int32, float32, float32)
		SetFocus(int32)
	})
	frame := 24
	draw := func(count int) map[int32]kryon.FrameOp {
		for i := 0; i < count; i++ {
			captureTime = epoch.Add(time.Duration(frame) * time.Second / 60)
			frame++
			kryon.BeginFrame()
			M02Buttons_ButtonsExample(&state, kryon.Rectangle{Width: 1536, Height: 1024})
			kryon.EndFrame()
		}
		result := map[int32]kryon.FrameOp{}
		for _, op := range runtime.(interface{ FrameOps() []kryon.FrameOp }).FrameOps() {
			if op.Kind == kryon.FrameOpButton {
				result[op.ID] = op
			}
		}
		return result
	}
	for _, activeID := range []int32{3000, 13000} {
		otherID := int32(16000) - activeID
		input.QueueMouseMove(-100, -100)
		input.SetFocus(0)
		resting := draw(24)
		active := resting[activeID]
		x, y := active.Bounds.X+active.Bounds.Width/2, active.Bounds.Y+active.Bounds.Height/2
		// Isolate each control's actual material raster from moving spinners.
		raster := func(op kryon.FrameOp) []byte {
			op.Bounds.X, op.Bounds.Y = 12, 12
			return kryon.RenderFrame(96, 64, []kryon.FrameOp{
				{Kind: kryon.FrameOpBackground, Color: colorValue(op.Button.Material.Ambient)}, op,
			}).Pix
		}
		otherPixels := raster(resting[otherID])
		checkOther := func(stage string, current map[int32]kryon.FrameOp) {
			t.Helper()
			other := current[otherID]
			if other.Hovered || other.Pressed || other.Focused || other.Button.Material.Hover != 0 ||
				other.Button.Material.Press != 0 || other.Button.Material.Focus != 0 ||
				!bytes.Equal(otherPixels, raster(other)) {
				t.Fatalf("%s on %d changed the other panel's control %d", stage, activeID, otherID)
			}
		}
		input.QueueMouseMove(x, y)
		hovered := draw(12)
		if !hovered[activeID].Hovered || hovered[activeID].Button.Material.Hover <= 0 ||
			bytes.Equal(raster(active), raster(hovered[activeID])) {
			t.Fatalf("hover did not animate split control %d", activeID)
		}
		checkOther("hover", hovered)
		input.QueueMouseButtonDown(kryon.MouseButtonLeft, x, y)
		pressed := draw(8)
		if !pressed[activeID].Pressed || bytes.Equal(raster(hovered[activeID]), raster(pressed[activeID])) {
			t.Fatalf("press did not animate split control %d", activeID)
		}
		checkOther("press", pressed)
		input.QueueMouseButtonUp(kryon.MouseButtonLeft, x, y)
		input.QueueMouseMove(-100, -100)
		input.SetFocus(activeID)
		focused := draw(24)
		if !focused[activeID].Focused || bytes.Equal(raster(active), raster(focused[activeID])) {
			t.Fatalf("focus did not animate split control %d", activeID)
		}
		checkOther("focus", focused)
		input.SetFocus(0)
		released := draw(24)
		if !bytes.Equal(raster(active), raster(released[activeID])) {
			t.Fatalf("split control %d did not return to rest", activeID)
		}
		checkOther("release", released)
	}
}

func saveLightfieldCapture(t *testing.T, directory, name string, ops []kryon.FrameOp) {
	t.Helper()
	output, err := os.Create(filepath.Join(directory, name+".png"))
	if err != nil {
		t.Fatal(err)
	}
	err = png.Encode(output, kryon.RenderFrame(768, 1024, ops))
	closeErr := output.Close()
	if err != nil {
		t.Fatal(err)
	}
	if closeErr != nil {
		t.Fatal(closeErr)
	}
}

// Coordinate-descent diagnostic, constrained to a few pixels around the
// declared layout. Every candidate is rendered as a real button over the
// example's real surfaces, including its unchanged text and lighting.
func fitButtonGeometry(t *testing.T, theme string, ops []kryon.FrameOp) {
	t.Helper()
	file, err := os.Open(os.Getenv("KRYON_LIGHTFIELD_REFERENCE"))
	if err != nil {
		t.Fatal(err)
	}
	defer file.Close()
	reference, err := png.Decode(file)
	if err != nil {
		t.Fatal(err)
	}
	if reference.Bounds() != image.Rect(0, 0, 1536, 1024) {
		t.Fatal("geometry fitting requires the original unscaled reference")
	}
	offset := 0
	if theme == "light" {
		offset = 768
	}
	group := os.Getenv("KRYON_LIGHTFIELD_GEOMETRY_GROUP")
	firstID, lastID := int32(1000), int32(1037)
	if group == "focus" {
		firstID, lastID = 1030, 1037
	} else if group == "sizes" {
		firstID, lastID = 2000, 2027
	} else if group == "large" {
		firstID, lastID = 2020, 2027
	} else if group == "icons" {
		firstID, lastID = 3000, 3024
	} else if group != "" && group != "states" {
		t.Fatalf("unknown geometry group %q: use states, focus, sizes, large, or icons", group)
	}
	for _, original := range ops {
		if original.Kind != kryon.FrameOpButton || original.ID < firstID || original.ID > lastID || (original.ID-firstID)%10 >= 8 {
			continue
		}
		if group == "icons" {
			column := (original.ID - firstID) % 20
			// Fit independent rectangular controls only. Circular controls and
			// joined segments need their shared shape constraints preserved.
			if column != 0 && column != 1 && column != 2 && column != 4 {
				continue
			}
		}
		width, height := int(original.Bounds.Width)+24, int(original.Bounds.Height)+24
		originX, originY := original.Bounds.X-12, original.Bounds.Y-12
		var surfaces []kryon.FrameOp
		for _, op := range ops {
			if op.Kind == kryon.FrameOpBackground || op.Kind == kryon.FrameOpSurface {
				op.Bounds.X -= originX
				op.Bounds.Y -= originY
				op.Clip.X -= originX
				op.Clip.Y -= originY
				surfaces = append(surfaces, op)
			}
		}
		score := func(bounds kryon.Rectangle) float64 {
			op := original
			op.Bounds = bounds
			op.Bounds.X -= originX
			op.Bounds.Y -= originY
			op.HasClip = false
			op.SurfaceBounds = kryon.Rectangle{}
			actual := kryon.RenderFrame(width, height, append(surfaces, op))
			var squared float64
			for y := 0; y < height; y++ {
				for x := 0; x < width; x++ {
					ar, ag, ab, _ := actual.At(x, y).RGBA()
					rr, rg, rb, _ := reference.At(int(originX)+x+offset, int(originY)+y).RGBA()
					for _, delta := range []float64{float64(ar) - float64(rr), float64(ag) - float64(rg), float64(ab) - float64(rb)} {
						squared += delta * delta / (257 * 257)
					}
				}
			}
			return math.Sqrt(squared / float64(width*height*3))
		}
		bestBounds := original.Bounds
		baseline := score(bestBounds)
		best := baseline
		for pass := 0; pass < 3; pass++ {
			for dimension := 0; dimension < 4; dimension++ {
				center := bestBounds
				limit := float32(3)
				if dimension == 0 {
					limit = 6
				}
				for delta := -limit; delta <= limit; delta++ {
					candidate := center
					switch dimension {
					case 0:
						candidate.X = original.Bounds.X + delta
					case 1:
						candidate.Y = original.Bounds.Y + delta
					case 2:
						candidate.Width = original.Bounds.Width + delta
					case 3:
						candidate.Height = original.Bounds.Height + delta
					}
					if value := score(candidate); value < best {
						best, bestBounds = value, candidate
					}
				}
			}
		}
		t.Logf("%s geometry id=%d: RMSE %.3f -> %.3f; bounds=(%g,%g,%g,%g)", theme, original.ID, baseline, best, bestBounds.X, bestBounds.Y, bestBounds.Width, bestBounds.Height)
	}
}

// Read-only diagnostic using the actual example's button paint operations.
// Candidate labels run through the real renderer; the reference is never
// rewritten and the normal capture always keeps the unmodified operations.
func fitButtonLabels(t *testing.T, theme string, ops []kryon.FrameOp) {
	t.Helper()
	referencePath := os.Getenv("KRYON_LIGHTFIELD_REFERENCE")
	if referencePath == "" {
		t.Fatal("KRYON_LIGHTFIELD_REFERENCE is required for opt-in label fitting")
	}
	file, err := os.Open(referencePath)
	if err != nil {
		t.Fatal(err)
	}
	defer file.Close()
	reference, err := png.Decode(file)
	if err != nil {
		t.Fatal(err)
	}
	if reference.Bounds() != image.Rect(0, 0, 1536, 1024) {
		t.Fatal("label fitting requires the original unscaled reference")
	}
	group := os.Getenv("KRYON_LIGHTFIELD_LABEL_GROUP")
	firstID, lastID, columns, expectedCount := int32(1000), int32(1041), int32(2), 10
	switch group {
	case "", "states":
	case "all-states":
		firstID, lastID, columns, expectedCount = 1000, 1047, 8, 40
	case "small":
		firstID, lastID, columns, expectedCount = 2000, 2007, 8, 8
	case "medium":
		firstID, lastID, columns, expectedCount = 2010, 2017, 8, 8
	case "large":
		firstID, lastID, columns, expectedCount = 2020, 2027, 8, 8
	case "full-run":
		firstID, lastID, columns, expectedCount = 5000, 5000, 1, 1
	case "full-delete":
		firstID, lastID, columns, expectedCount = 5001, 5001, 1, 1
	case "icon-text":
		firstID, lastID, columns, expectedCount = 3000, 3020, 1, 2
	case "icon-leading":
		firstID, lastID, columns, expectedCount = 3001, 3021, 1, 2
	case "icon-trailing":
		firstID, lastID, columns, expectedCount = 3002, 3022, 1, 2
	case "pill":
		firstID, lastID, columns, expectedCount = 3004, 3024, 1, 2
	default:
		t.Fatalf("unknown label group %q", group)
	}
	var buttons []kryon.FrameOp
	for _, op := range ops {
		if op.Kind == kryon.FrameOpButton && op.ID >= firstID && op.ID <= lastID && (op.ID-firstID)%10 < columns {
			buttons = append(buttons, op)
		}
	}
	if len(buttons) != expectedCount {
		t.Fatalf("label fitting group %q needs %d buttons, got %d", group, expectedCount, len(buttons))
	}
	offset := 0
	if theme == "light" {
		offset = 768
	}
	score := func(font int32, dx, dy float32, face uint32) float64 {
		var squared float64
		var count int
		for _, original := range buttons {
			op := original
			cropStart, cropEnd := 16, int(op.Bounds.Width)-16
			if op.Bounds.Width > 256 {
				cropStart, cropEnd = int(op.Bounds.Width)/2-64, int(op.Bounds.Width)/2+64
			}
			op.Bounds.X, op.Bounds.Y = 8-float32(cropStart), 8
			op.SurfaceBounds = kryon.Rectangle{}
			op.HasClip = false
			op.Button.Font = font
			op.FontID = face
			op.Button.Appearance.Value.OffsetX, op.Button.Appearance.Value.OffsetY = dx, dy
			actual := kryon.RenderFrame(cropEnd-cropStart+16, int(op.Bounds.Height)+16, []kryon.FrameOp{
				{Kind: kryon.FrameOpBackground, Color: colorValue(op.Button.Material.Ambient)}, op,
			})
			// Hold geometry/material constant, measuring the central label area
			// across resting, hover, pressed, focused and disabled appearances.
			for y := 4; y < int(op.Bounds.Height)-4; y++ {
				for x := cropStart; x < cropEnd; x++ {
					ar, ag, ab, _ := actual.At(x-cropStart+8, y+8).RGBA()
					rr, rg, rb, _ := reference.At(int(original.Bounds.X)+x+offset, int(original.Bounds.Y)+y).RGBA()
					for _, delta := range []float64{float64(ar) - float64(rr), float64(ag) - float64(rg), float64(ab) - float64(rb)} {
						squared += delta * delta / (257 * 257)
						count++
					}
				}
			}
		}
		return math.Sqrt(squared / float64(count))
	}
	defaultFont := buttons[0].Button.Font
	defaultOffset := (kryon.Vector2{X: buttons[0].Button.Appearance.Value.OffsetX, Y: buttons[0].Button.Appearance.Value.OffsetY})
	defaultFace := buttons[0].FontID
	faces := []uint32{defaultFace}
	if os.Getenv("KRYON_FIT_BUTTON_TYPEFACES") == "1" {
		for _, op := range ops {
			if op.Kind == kryon.FrameOpText && (op.Text == "LIGHT THEME" || op.Text == "DARK THEME") && op.FontID != defaultFace {
				faces = append(faces, op.FontID)
				break
			}
		}
		if len(faces) != 2 {
			t.Fatal("typeface fitting requires the registered semibold heading face")
		}
	}
	// Opt-in family comparison uses explicit local files, never host font
	// discovery or a replacement of the application's active/default typeface.
	for _, path := range filepath.SplitList(os.Getenv("KRYON_FIT_BUTTON_FONT_FILES")) {
		data, err := os.ReadFile(path)
		if err != nil {
			t.Fatal(err)
		}
		font := kryon.LoadFontFromMemory(filepath.Ext(path), data, 0, nil)
		if font.ID == 0 {
			t.Fatalf("cannot load comparison font %s", path)
		}
		faces = append(faces, font.ID)
		t.Logf("comparison face=%d file=%s", font.ID, path)
	}
	baseline := score(defaultFont, defaultOffset.X, defaultOffset.Y, defaultFace)
	best := baseline
	bestFont, bestX, bestY := defaultFont, defaultOffset.X, defaultOffset.Y
	bestFace := defaultFace
	maxOffsetX := float32(2)
	if firstID >= 5000 {
		maxOffsetX = 24
	}
	for _, face := range faces {
		for font := defaultFont - 2; font <= defaultFont+2; font++ {
			t.Logf("%s centered label font=%d face=%d RMSE %.4f", theme, font, face, score(font, 0, 0, face))
			for dx := -maxOffsetX; dx <= maxOffsetX; dx++ {
				for dy := float32(-2); dy <= 2; dy++ {
					candidate := score(font, dx, dy, face)
					if candidate < best {
						best, bestFont, bestX, bestY = candidate, font, dx, dy
						bestFace = face
					}
				}
			}
		}
	}
	t.Logf("%s %s label diagnostic: baseline RMSE %.4f; best %.4f, font=%d face=%d offset=(%g,%g)", theme, group, baseline, best, bestFont, bestFace, bestX, bestY)
}

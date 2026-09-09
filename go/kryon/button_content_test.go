package kryon

import (
	"bytes"
	"image"
	"testing"
)

func TestButtonMeasurementContract(t *testing.T) {
	request := ButtonMeasure{MinimumHeight: 40, LabelWidth: 24, LabelHeight: 27,
		PaddingX: 16, PaddingY: 20, IconSize: 8.5, Gap: 8, HasIcon: true}
	check := func(width, height float32) {
		t.Helper()
		if got := Button_MeasureSize(request); got.Width != width || got.Height != height {
			t.Fatalf("measurement=%+v want %g x %g; request=%+v", got, width, height, request)
		}
	}
	check(72.5, 67)
	request.Width, request.Height = 120, 24
	check(120, 24)
	request.Circle = true
	check(24, 24)
	request.Circle, request.Width, request.FullWidth, request.AvailableWidth = false, 0, true, 300
	check(300, 24)
	request.AvailableWidth = 10
	check(24, 24)
	request.IconOnly, request.Height = true, 0
	check(40, 40)
}

func TestButtonChildrenUseResolvedStylePadding(t *testing.T) {
	r := New(AppConfig{Width: 400, Height: 200}).(*runtime)
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStateNormal} {
		r.BeginFrame()
		r.BeginButton(ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 200, Height: 100}, ID: 901, State: state,
			Style: ControlStyle{
				Normal: Style{Fields: StylePaddingX | StylePaddingY, PaddingX: 25, PaddingY: 4},
				Hover:  Style{Fields: StylePaddingX | StylePaddingY, PaddingX: 0, PaddingY: 0},
			}})
		r.Column(ColumnProps{})
		r.End()
		r.End()
		r.EndFrame()
		want := Rectangle{X: 45, Y: 24, Width: 150, Height: 92}
		if state == ButtonStateHover {
			want = Rectangle{X: 20, Y: 20, Width: 200, Height: 100}
		}
		found := false
		for _, op := range r.FrameOps() {
			if op.Kind == FrameOpColumn {
				found = true
				if op.Bounds != want {
					t.Fatalf("state %v: child bounds %+v, want %+v", state, op.Bounds, want)
				}
			}
		}
		if !found {
			t.Fatal("missing composed column")
		}
	}
}

func TestSharedStyleContentBoundsClampEmptyArea(t *testing.T) {
	if got := Style_ContentBounds(20, 10, 30, 40); got != (ContentBox{X: 30, Y: 40}) {
		t.Fatalf("oversized insets must leave an empty content area: %+v", got)
	}
	if got := Style_ContentBounds(20, 10, -1, -2); got != (ContentBox{Width: 20, Height: 10}) {
		t.Fatalf("negative padding must not expand the face: %+v", got)
	}
}

func TestButtonCustomIconSizesRemainExplicit(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	for _, size := range []float32{-1, 0, 0.5, 1, 8.5, 18} {
		props := r.resolveButtonProps(ButtonProps{Label: "Run", IconType: UIIconTypePlay,
			Style: ControlStyle{Normal: Style{Fields: StyleIconSize, IconSize: size}}})
		frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
		if frame.IconSize != size {
			t.Fatalf("style icon size %v was rounded to %v before rendering", size, frame.IconSize)
		}
		labelWidth := float32(runtimeTextWidth("Run", frame.FontSize))
		content := Button_ContentLayout(props.Bounds.Width, props.Bounds.Height, labelWidth,
			size, frame.Gap, true, false, false, 0, 0)
		if content.IconSize != max(float32(0), size) {
			t.Fatalf("custom icon size %v replaced by %v", size, content.IconSize)
		}
		if size <= 0 {
			if props.Bounds.Width != labelWidth+2*defaultThemeMetrics().ControlPaddingMedium {
				t.Fatalf("hidden icon must not reserve a size or gap: width %v, text %v", props.Bounds.Width, labelWidth)
			}
			frame.IconType = UIIconTypeNone
			expected := RenderFrame(160, 60, []FrameOp{frame})
			for _, icon := range []int32{UIIconTypePlay, UIIconTypeText} {
				frame.IconType = icon
				actual := RenderFrame(160, 60, []FrameOp{frame})
				if !bytes.Equal(actual.Pix, expected.Pix) {
					t.Fatal("zero-sized vector and fallback icons must paint no pixels")
				}
			}
			if ring := Surface_LoadingRing(72, 40, size, 0, 0xffffffff, 0xffffffff); Surface_LoadingPaintRadius(ring) != 0 {
				t.Fatal("zero-sized loading indicator must paint no pixels")
			}
		}
	}
}

func TestButtonNaturalHeightFitsStyledText(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	for _, test := range []struct {
		requested, padding, font, want float32
	}{{0, 8, 18, 40}, {0, 20, 27, 67}, {0, -10, 60, 60}, {24, 20, 27, 24}} {
		props := r.resolveButtonProps(ButtonProps{Label: "Run", Bounds: Rectangle{Height: test.requested},
			Style: ControlStyle{Normal: Style{Fields: StylePaddingY | StyleFontSize,
				PaddingY: test.padding, FontSize: test.font}}})
		if props.Bounds.Height != test.want {
			t.Fatalf("%+v: resolved height %v", test, props.Bounds.Height)
		}
	}
}

func TestButtonMeasurementClampsPaddingLikeContentBounds(t *testing.T) {
	for _, padding := range []float32{-50, -1, 0, 12} {
		measured := Button_MeasureContent(40, 24, 18, 8, padding, true, false)
		content := Style_ContentBounds(measured.Width, 40, padding, 0)
		if content.Width != 50 {
			t.Fatalf("padding %v: measured width %v leaves %v for 50 pixels of content", padding, measured.Width, content.Width)
		}
	}
}

func TestButtonTextTruncatesFractionalOriginLikeC(t *testing.T) {
	ensureDefaultUIFont()
	width := float32(runtimeTextWidth("Run", 18)) + 21
	op := FrameOp{Kind: FrameOpButton, Bounds: Rectangle{X: 4.25, Y: 3.25, Width: width, Height: 40},
		Text: "Run", FontSize: 18, TextColor: White, Opacity: 1}
	actual := image.NewRGBA(image.Rect(0, 0, 100, 50))
	expected := image.NewRGBA(actual.Bounds())
	renderButton(actual, op)
	baseline := fontTextBaseline("Run", 3, 40, 18, 0)
	drawText(expected, "Run", 14, baseline, 18, White, 0)
	if !bytes.Equal(actual.Pix, expected.Pix) {
		t.Fatal("button text must truncate its 14.75-pixel origin to C's 14-pixel origin")
	}
}

func TestButtonMeasurementMatchesPlacedContent(t *testing.T) {
	for _, height := range []float32{8, 24, 40, 48} {
		for _, label := range []float32{0, 24} {
			for _, icon := range []float32{0, 8, 18, 64} {
				for _, hasIcon := range []bool{false, true} {
					for _, iconOnly := range []bool{false, true} {
						measured := Button_MeasureContent(height, label, icon, 8, 11, hasIcon, iconOnly)
						placed := Button_ContentLayout(measured.Width, height, label, icon, 8,
							hasIcon, iconOnly, false, 0, 0)
						if placed.IconX != 11 || placed.IconSize != measured.IconSize || placed.TextWidth != measured.TextWidth {
							t.Fatalf("measurement and placement disagree: %+v %+v", measured, placed)
						}
						if (!hasIcon || iconOnly || label == 0) && measured.Gap != 0 {
							t.Fatal("missing content must not reserve an inter-item gap")
						}
					}
				}
			}
		}
	}
	r := New(AppConfig{}).(*runtime)
	props := ButtonProps{IconType: UIIconTypePlay,
		Style: ControlStyle{Normal: Style{Fields: StylePaddingX | StyleIconSize | StyleGap,
			PaddingX: 11, IconSize: 64, Gap: 8}}}
	measured := r.resolveButtonProps(props)
	if measured.Bounds.Width != measured.Bounds.Height-12+22 {
		t.Fatalf("empty-label icon width must use its fitted size and no gap: %+v", measured.Bounds)
	}
}

func TestButtonContentSharedGeometry(t *testing.T) {
	leading := Button_ContentLayout(100, 40, 24, 18, 8, true, false, false, 0, 0)
	if leading.IconX != 25 || leading.IconY != 11 || leading.TextX != 51 || leading.TextWidth != 24 {
		t.Fatalf("leading content: %+v", leading)
	}
	trailing := Button_ContentLayout(100, 40, 24, 18, 8, true, false, true, 0, 0)
	if trailing.TextX != 25 || trailing.IconX != 57 {
		t.Fatalf("trailing content: %+v", trailing)
	}
	zeroGap := Button_ContentLayout(100, 40, 24, 18, 0, true, false, false, 0, 0)
	if zeroGap.TextX-zeroGap.IconX != 18 {
		t.Fatalf("explicit zero gap ignored: %+v", zeroGap)
	}
	iconOnly := Button_ContentLayout(40, 40, 100, 18, 8, true, true, false, 2, -3)
	if iconOnly.IconX != 13 || iconOnly.IconY != 8 || iconOnly.TextWidth != 0 {
		t.Fatalf("icon-only content: %+v", iconOnly)
	}
	small := Button_ContentLayout(56, 24, 24, 18, 8, true, false, false, 0, 0)
	if small.IconSize != 12 {
		t.Fatalf("small control icon exceeds content height: %+v", small)
	}
}

func TestDefaultAndExplicitButtonSizes(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.SetTheme(ThemeDefaultLight())
	for _, test := range []struct {
		size   ControlSize
		height float32
		font   int32
	}{{ControlSizeMedium, 40, 18}, {ControlSizeSmall, 32, 16}, {ControlSizeLarge, 48, 20}} {
		props := r.resolveButtonProps(ButtonProps{Label: "Run", Size: test.size})
		frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
		if props.Bounds.Height != test.height || props.Font != 0 || frame.FontSize != test.font {
			t.Fatalf("size %v: got height %v/request %v/painted font %v", test.size, props.Bounds.Height, props.Font, frame.FontSize)
		}
	}
	if props := r.resolveButtonProps(ButtonProps{Label: "Run"}); props.Bounds.Height != 40 || props.Font != 0 {
		t.Fatalf("zero-value props did not select medium: %+v", props)
	}
}

func TestButtonZeroPaddingAndGapRemainExplicit(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.SetTheme(ThemeDefaultLight())
	props := r.resolveButtonProps(ButtonProps{Label: "Run", IconType: UIIconTypePlay,
		Style: ControlStyle{Normal: Style{Fields: StylePaddingX | StyleGap, PaddingX: 0, Gap: 0}}})
	frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
	want := float32(runtimeTextWidth("Run", frame.FontSize)) + 18
	if props.Bounds.Width != want {
		t.Fatalf("zero padding/gap was replaced by defaults: got %v, want %v", props.Bounds.Width, want)
	}
}

func TestRenderedButtonTextHasNoExtraPadding(t *testing.T) {
	img := RenderFrame(100, 40, []FrameOp{{Kind: FrameOpButton,
		Bounds: Rectangle{Width: 100, Height: 40}, Text: "Run", FontSize: 18,
		Color: Black, BorderColor: Black, TextColor: White, Opacity: 1, Radius: 8}})
	left, right := 100, -1
	for y := 8; y < 32; y++ {
		for x := 10; x < 90; x++ {
			pixel := img.RGBAAt(x, y)
			if pixel.R > 180 && pixel.G > 180 && pixel.B > 180 {
				left = min(left, x)
				right = max(right, x)
			}
		}
	}
	if right < left || (left+right)/2 < 47 || (left+right)/2 > 52 {
		t.Fatalf("text ink is not centered: left=%d right=%d", left, right)
	}
}

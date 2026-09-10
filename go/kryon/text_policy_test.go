package kryon

import (
	"bytes"
	"testing"
	"time"
)

func TestSharedFontRequestPrecedence(t *testing.T) {
	for _, test := range []struct{ requested, styled, fallback, want int32 }{
		{13, 27, 18, 13}, {0, 27, 18, 27}, {-1, 27, 18, 27},
		{0, 0, 18, 18}, {0, -1, 18, 18}, {0, 0, 0, 16},
	} {
		if got := Style_ResolveFont(test.requested, test.styled, test.fallback); got != test.want {
			t.Fatalf("font request %+v resolved to %d", test, got)
		}
	}
}

func TestLiveButtonStyleResolvesFontAtPaintTime(t *testing.T) {
	for _, explicit := range []int32{0, 13} {
		now := time.Unix(1, 0)
		r := New(AppConfig{Width: 400, Height: 200, FrameClock: func() time.Time { return now }}).(*runtime)
		props := ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 180, Height: 60}, Label: "Measured", ID: 911, Font: explicit,
			Style: ControlStyle{
				Normal: Style{Fields: StyleFontSize, FontSize: 17},
				Hover:  Style{Fields: StyleFontSize, FontSize: 27},
			}}
		for _, hovered := range []bool{false, true, false} {
			if hovered {
				r.QueueMouseMove(25, 25)
			} else {
				r.QueueMouseMove(300, 150)
			}
			for i := 0; i < 20; i++ {
				now = now.Add(20 * time.Millisecond)
				r.BeginFrame()
				r.Button(props)
				r.EndFrame()
			}
			want := int32(17)
			if hovered {
				want = 27
			}
			if explicit > 0 {
				want = explicit
			}
			frame := r.FrameOps()[0]
			if frame.Button.Font != want || frame.Bounds != props.Bounds {
				t.Fatalf("hover %v explicit %d: got font %d bounds %+v, want font %d bounds %+v", hovered, explicit, frame.Button.Font, frame.Bounds, want, props.Bounds)
			}
		}
	}
}

func TestComposedTextUsesAnimatedButtonFrame(t *testing.T) {
	for _, theme := range []Theme{ThemeDefaultDark(), ThemeDefaultLight()} {
		now := time.Unix(1, 0)
		r := New(AppConfig{Width: 320, Height: 160, FrameClock: func() time.Time { return now }}).(*runtime)
		r.SetTheme(theme)
		props := ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 200, Height: 100}, ID: 991,
			Style: ControlStyle{
				Normal: Style{Fields: StyleForeground | StyleOpacity, Foreground: Color{200, 40, 20, 0}, Opacity: 0.5},
				Hover:  Style{Fields: StyleForeground | StyleOpacity, Foreground: Color{20, 100, 240, 255}, Opacity: 1},
			}}
		draw := func(delta time.Duration) FrameOp {
			now = now.Add(delta)
			r.BeginFrame()
			r.BeginButton(props)
			r.Column(ColumnProps{Bounds: Rectangle{Width: 180, Height: 60}})
			r.Text(TextProps{Text: "Inherited"})
			r.End()
			r.End()
			r.EndFrame()
			var button FrameOp
			buttons, texts := 0, 0
			for _, op := range r.FrameOps() {
				if op.Kind == FrameOpButton {
					button = op
					buttons++
				}
				if op.Kind == FrameOpText {
					want := unpackRGBA(Surface_Opacity(packRGBA(unpackRGBA(button.Button.Appearance.Value.Foreground)), button.Button.Appearance.Value.Opacity))
					if op.Color != want {
						t.Fatalf("nested text differs from its animated surface: got %+v, want %+v", op.Color, want)
					}
					texts++
				}
			}
			if buttons != 1 || texts != 1 {
				t.Fatalf("expected one surface and child, got %d and %d", buttons, texts)
			}
			withoutText := make([]FrameOp, 0, len(r.FrameOps()))
			for _, op := range r.FrameOps() {
				if op.Kind != FrameOpText {
					withoutText = append(withoutText, op)
				}
			}
			painted := RenderFrame(320, 160, r.FrameOps())
			background := RenderFrame(320, 160, withoutText)
			visible := !bytes.Equal(painted.Pix, background.Pix)
			if visible != (Surface_Opacity(packRGBA(unpackRGBA(button.Button.Appearance.Value.Foreground)), button.Button.Appearance.Value.Opacity)&255 > 0) {
				t.Fatal("composed text pixels do not follow the resolved fade alpha")
			}
			return button
		}
		r.QueueMouseMove(300, 150)
		normal := draw(0)
		r.QueueMouseMove(80, 50)
		middle := draw(35 * time.Millisecond)
		if middle.Button.Material.Hover != 0.578125 || unpackRGBA(middle.Button.Appearance.Value.Foreground) == unpackRGBA(normal.Button.Appearance.Value.Foreground) || unpackRGBA(middle.Button.Appearance.Value.Foreground) == props.Style.Hover.Foreground {
			t.Fatalf("hover must advance exactly once and produce an intermediate style: %+v", middle)
		}
		settled := draw(140 * time.Millisecond)
		if unpackRGBA(settled.Button.Appearance.Value.Foreground) != props.Style.Hover.Foreground || settled.Button.Appearance.Value.Opacity != 1 {
			t.Fatalf("hover did not reach its endpoint: %+v", settled)
		}
		r.QueueMouseMove(300, 150)
		draw(35 * time.Millisecond)
		returned := draw(140 * time.Millisecond)
		if unpackRGBA(returned.Button.Appearance.Value.Foreground) != unpackRGBA(normal.Button.Appearance.Value.Foreground) || returned.Button.Appearance.Value.Opacity != normal.Button.Appearance.Value.Opacity {
			t.Fatal("hover exit did not restore the original transparent style")
		}
	}
}

func TestSharedTextAppearanceAndLayout(t *testing.T) {
	inherited := Text_ResolveTextStyle(0, 27, 16, 0, 0x11223380, 0xffffffff, true, false, false, false, -2)
	if inherited.Font != 27 || inherited.Color != 0x11223380 || inherited.LetterSpacing != 0 {
		t.Fatalf("inherited appearance: %+v", inherited)
	}
	explicit := Text_ResolveTextStyle(19, 27, 16, 0x445566ff, 0x11223380, 0xffffffff, true, true, true, true, 3)
	if explicit.Font != 19 || explicit.Color != 0x44556672 || explicit.LetterSpacing != 3 {
		t.Fatalf("explicit appearance must win and disabled opacity applies once: %+v", explicit)
	}
	for _, present := range []bool{false, true} {
		style := Text_ResolveTextStyle(0, 27, 16, 0, 0x11223300, 0xffffffff, present, false, false, false, 0)
		want := uint32(0xffffffff)
		if present {
			want = 0x11223300
		}
		if style.Color != want {
			t.Fatalf("inherited color presence %v: got %#x, want %#x", present, style.Color, want)
		}
	}
	if Text_TextExtent(24, 100) != 24 || Text_TextExtent(0, 100) != 100 ||
		Text_TextWrapPolicy(0, 0) != 1 || Text_TextWrapPolicy(100, 0) != 0 ||
		Text_TextAlignmentOffset(100, 40, 1) != 30 || Text_TextAlignmentOffset(100, 40, 2) != 60 ||
		Text_TextAlignmentOffset(20, 41, 1) != -10.5 {
		t.Fatal("shared text sizing, wrapping or overflow alignment changed")
	}
}

func TestTextSharedStyle(t *testing.T) {
	for _, alpha := range []uint8{0, 128, 255} {
		for _, disabled := range []bool{false, true} {
			r := New(AppConfig{Width: 240, Height: 100}).(*runtime)
			foreground := Color{0, 0, 0, alpha}
			r.Text(TextProps{Text: "Styled", Font: 13, Color: Color{255, 0, 0, 255}, Disabled: disabled,
				Style: Style{Fields: StyleForeground | StyleFontSize | StyleOpacity,
					Foreground: foreground, FontSize: 27, Opacity: 0.5}})
			want := uint32(alpha)
			if disabled {
				want = uint32(float32(want) * 0.45)
			}
			want = Surface_Opacity(want, 0.5)
			op := r.FrameOps()[0]
			if op.FontSize != 27 || packRGBA(op.Color) != want {
				t.Fatalf("alpha %d disabled %v: font %d color %#x, want 27/%#x", alpha, disabled, op.FontSize, packRGBA(op.Color), want)
			}
			if alpha == 0 && !bytes.Equal(RenderFrame(240, 100, r.FrameOps()).Pix, RenderFrame(240, 100, nil).Pix) {
				t.Fatal("explicit transparent text painted visible pixels")
			}
		}
	}
}

func TestNestedTextStyleKeepsExplicitTransparency(t *testing.T) {
	r := New(AppConfig{Width: 240, Height: 140}).(*runtime)
	r.BeginFrame()
	r.BeginButton(ButtonProps{Bounds: Rectangle{Width: 220, Height: 120}, ID: 923,
		Style: ControlStyle{Normal: Style{Fields: StyleForeground | StyleOpacity,
			Foreground: Color{17, 34, 51, 128}, Opacity: 0.5}}})
	r.Text(TextProps{Text: "Transparent", Style: Style{Fields: StyleForeground}})
	r.Text(TextProps{Text: "Half", Style: Style{Fields: StyleOpacity, Opacity: 0.5}})
	r.Text(TextProps{Text: "Hidden", Style: Style{Fields: StyleOpacity}})
	r.End()
	r.EndFrame()
	texts := 0
	for _, op := range r.FrameOps() {
		if op.Kind != FrameOpText {
			continue
		}
		want := uint32(0)
		if op.Text == "Half" {
			want = 0x11223320
		}
		if op.Text == "Hidden" {
			want = 0x11223300
		}
		if packRGBA(op.Color) != want {
			t.Fatalf("%s: got %#x, want %#x", op.Text, packRGBA(op.Color), want)
		}
		texts++
	}
	if texts != 3 {
		t.Fatalf("got %d text nodes, want 3", texts)
	}
}

func TestExplicitButtonStateMeasuresItsResolvedFont(t *testing.T) {
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed, ButtonStateFocus,
		ButtonStateDisabled, ButtonStateLoading, ButtonStateSelected} {
		for _, explicit := range []int32{0, 13} {
			style := Style{Fields: StyleFontSize | StylePaddingX, FontSize: 27, PaddingX: 19}
			props := ButtonProps{Label: "Measured", State: state, Font: explicit, ID: 11,
				Style: ControlStyle{Normal: Style{Fields: StyleFontSize | StylePaddingX, FontSize: 17, PaddingX: 8},
					Hover: style, Pressed: style, Focused: style, Disabled: style, Loading: style, Selected: style}}
			r := New(AppConfig{Width: 400, Height: 200}).(*runtime)
			r.Button(props)
			wantFont, padding := int32(27), float32(19)
			if state == ButtonStateNormal {
				wantFont, padding = 17, 8
			}
			if explicit > 0 {
				wantFont = explicit
			}
			ops := r.FrameOps()
			if len(ops) != 1 || ops[0].Button.Font != wantFont ||
				ops[0].Bounds.Width != float32(runtimeTextWidth(props.Label, wantFont))+padding*2 {
				t.Fatalf("state %v must measure and paint the same font %d: %+v", state, wantFont, ops)
			}
		}
	}
}

func TestTransparentTextInheritanceAcrossDisabledScope(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 160}).(*runtime)
	r.BeginFrame()
	r.BeginButton(ButtonProps{Bounds: Rectangle{Width: 200, Height: 100}, Font: 27, ID: 992,
		Style: ControlStyle{Normal: Style{Fields: StyleForeground, Foreground: Color{17, 34, 51, 0}}}})
	r.BeginDisabled(true)
	r.Column(ColumnProps{Bounds: Rectangle{Width: 180, Height: 60}})
	r.Text(TextProps{Text: "Transparent", Wrap: TextWrapNone})
	r.End()
	r.EndDisabled()
	r.End()
	r.Text(TextProps{Text: "Outside", Font: 13, Color: Color{68, 85, 102, 255}})
	r.EndFrame()
	count := 0
	for _, op := range r.FrameOps() {
		if op.Kind != FrameOpText {
			continue
		}
		count++
		if op.Text == "Transparent" {
			if op.Color.A != 0 || op.FontSize != 27 {
				t.Fatalf("disabled scope lost transparent inherited text style: %+v", op)
			}
		} else if op.Color != (Color{68, 85, 102, 255}) || op.FontSize != 13 {
			t.Fatalf("inherited style or disabled scope leaked into following text: %+v", op)
		}
	}
	if count != 2 {
		t.Fatalf("got %d text nodes, want 2", count)
	}
}

func TestComposedDisabledTextIsNotFadedTwice(t *testing.T) {
	for _, scoped := range []bool{false, true} {
		r := New(AppConfig{Width: 240, Height: 140}).(*runtime)
		r.BeginFrame()
		r.BeginDisabled(scoped)
		r.BeginButton(ButtonProps{Bounds: Rectangle{Width: 220, Height: 120}, ID: 963, Disabled: !scoped,
			Style: ControlStyle{Disabled: Style{Fields: StyleForeground, Foreground: Color{17, 34, 51, 128}}}})
		r.Text(TextProps{Text: "Inherited"})
		r.Text(TextProps{Text: "Explicit", Color: Color{68, 85, 102, 128}})
		r.Text(TextProps{Text: "Disabled inherited", Disabled: true})
		r.End()
		r.EndDisabled()
		r.EndFrame()
		texts := 0
		for _, op := range r.FrameOps() {
			if op.Kind != FrameOpText {
				continue
			}
			want := uint32(0x11223380)
			if op.Text == "Explicit" {
				want = 0x44556639
			}
			if packRGBA(op.Color) != want || !op.Disabled {
				t.Errorf("scope %v %s: color %#x disabled %v, want %#x/true", scoped, op.Text, packRGBA(op.Color), op.Disabled, want)
			}
			texts++
		}
		if texts != 3 {
			t.Fatalf("got %d texts, want 3", texts)
		}
	}
}

func TestTextChildInheritsFontBeforeMeasurement(t *testing.T) {
	r := New(AppConfig{Width: 320, Height: 160}).(*runtime)
	r.BeginButton(ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 200, Height: 100}, Font: 27, ID: 991,
		Style: ControlStyle{Normal: Style{Fields: StyleForeground, Foreground: Color{17, 34, 51, 0}}}})
	r.Text(TextProps{Text: "Inherited", Wrap: TextWrapNone})
	r.Column(ColumnProps{Bounds: Rectangle{Width: 180, Height: 60}})
	r.Text(TextProps{Text: "Nested", Wrap: TextWrapNone})
	r.Text(TextProps{Text: "Explicit", Font: 13, Color: Color{68, 85, 102, 255}, Wrap: TextWrapNone})
	r.End()
	r.End()
	count := 0
	for _, op := range r.ops {
		if op.Kind != FrameOpText {
			continue
		}
		want := int32(27)
		wantColor := Color{17, 34, 51, 0}
		if op.Text == "Explicit" {
			want = 13
			wantColor = Color{68, 85, 102, 255}
		}
		if op.Color != wantColor {
			t.Fatalf("child %q lost inherited or explicit color: got %+v, want %+v", op.Text, op.Color, wantColor)
		}
		if op.FontSize != want || op.Bounds.Width != float32(runtimeTextWidthWithFont(op.Text, want, op.FontID)) {
			t.Fatalf("text was measured before font inheritance: %+v", op)
		}
		count++
	}
	if count != 3 {
		t.Fatalf("got %d text children, want 3", count)
	}
}

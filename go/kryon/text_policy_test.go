package kryon

import (
	"bytes"
	"fmt"
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
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.fonts;
Button.measured-font { font-size: 17; }
Button.measured-font:hover { font-size: 27; }`, "Button Fonts", "") {
		t.Fatal("button font style pack did not register")
	}
	now := time.Unix(1, 0)
	r := New(AppConfig{Width: 400, Height: 200, FrameClock: func() time.Time { return now }}).(*runtime)
	props := ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 180, Height: 60}, Label: "Measured", ID: 911,
		ClassName: StyleClassID("measured-font")}
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
		frame := r.FrameOps()[0]
		if frame.Button.Font != want || frame.Bounds != props.Bounds {
			t.Fatalf("hover %v: got font %d bounds %+v, want font %d bounds %+v", hovered, frame.Button.Font, frame.Bounds, want, props.Bounds)
		}
	}
}

func TestComposedTextUsesAnimatedButtonFrame(t *testing.T) {
	for _, theme := range []Theme{ThemeDefaultDark(), ThemeDefaultLight()} {
		ClearStylePacks()
		if !RegisterStylePackSource(`@pack test.button.composed;
Button.composed { foreground: #c8281400; opacity: 0.5; }
Button.composed:hover { foreground: #1464f0; opacity: 1; }`, "Button Composed", "") {
			t.Fatal("button composed style pack did not register")
		}
		hoverForeground := Color{20, 100, 240, 255}
		now := time.Unix(1, 0)
		r := New(AppConfig{Width: 320, Height: 160, FrameClock: func() time.Time { return now }}).(*runtime)
		r.SetTheme(theme)
		props := ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 200, Height: 100}, ID: 991,
			ClassName: StyleClassID("composed")}
		draw := func(delta time.Duration) FrameOp {
			now = now.Add(delta)
			r.BeginFrame()
			r.ButtonScope(props)
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
		if middle.Button.Material.Hover != 0.578125 || unpackRGBA(middle.Button.Appearance.Value.Foreground) == unpackRGBA(normal.Button.Appearance.Value.Foreground) || unpackRGBA(middle.Button.Appearance.Value.Foreground) == hoverForeground {
			t.Fatalf("hover must advance exactly once and produce an intermediate style: %+v", middle)
		}
		settled := draw(140 * time.Millisecond)
		if unpackRGBA(settled.Button.Appearance.Value.Foreground) != hoverForeground || settled.Button.Appearance.Value.Opacity != 1 {
			t.Fatalf("hover did not reach its endpoint: %+v", settled)
		}
		r.QueueMouseMove(300, 150)
		draw(35 * time.Millisecond)
		returned := draw(140 * time.Millisecond)
		if unpackRGBA(returned.Button.Appearance.Value.Foreground) != unpackRGBA(normal.Button.Appearance.Value.Foreground) || returned.Button.Appearance.Value.Opacity != normal.Button.Appearance.Value.Opacity {
			t.Fatal("hover exit did not restore the original transparent style")
		}
		ClearStylePacks()
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
			ClearStylePacks()
			r := New(AppConfig{Width: 240, Height: 100}).(*runtime)
			if !RegisterStylePackSource(fmt.Sprintf(`
@pack test.text_shared_%d_%t;
Text.shared {
  foreground: #000000%02x;
  font-size: 27;
  letter-spacing: 2;
  opacity: 0.5;
}
`, alpha, disabled, alpha), "Text Shared", "") {
				t.Fatal("style pack did not register")
			}
			r.Text(TextProps{Text: "Styled", ClassName: StyleClassID("shared"), Disabled: disabled})
			want := uint32(alpha)
			if disabled {
				want = uint32(float32(want) * 0.45)
			}
			want = Surface_Opacity(want, 0.5)
			op := r.FrameOps()[0]
			if op.FontSize != 27 || op.LetterSpacing != 2 || packRGBA(op.Color) != want {
				t.Fatalf("alpha %d disabled %v: font %d spacing %d color %#x, want 27/2/%#x", alpha, disabled, op.FontSize, op.LetterSpacing, packRGBA(op.Color), want)
			}
			if alpha == 0 && !bytes.Equal(RenderFrame(240, 100, r.FrameOps()).Pix, RenderFrame(240, 100, nil).Pix) {
				t.Fatal("explicit transparent text painted visible pixels")
			}
		}
	}
	ClearStylePacks()
}

func TestUnstyledTextFallbackIgnoresThemeTextColor(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	r := New(AppConfig{Width: 240, Height: 100}).(*runtime)
	theme := ThemeDefaultDark()
	theme.Colors.Text = Color{1, 2, 3, 255}
	r.SetTheme(theme)
	r.Text(TextProps{Text: "unstyled", Wrap: TextWrapNone})

	op := r.FrameOps()[0]
	if op.Color != (Color{255, 255, 255, 255}) {
		t.Fatalf("unstyled text used theme styling fallback: %+v", op.Color)
	}
}

func TestNestedTextStyleKeepsExplicitTransparency(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterStylePackSource(`
@pack test.nested_text_style;
Text.transparent { foreground: #00000000; }
Text.half { opacity: 0.5; }
Text.hidden { opacity: 0; }
Button.text-parent { foreground: #11223380; opacity: 0.5; }
`, "Nested Text Style", "") {
		t.Fatal("style pack did not register")
	}
	r := New(AppConfig{Width: 240, Height: 140}).(*runtime)
	r.BeginFrame()
	r.ButtonScope(ButtonProps{Bounds: Rectangle{Width: 220, Height: 120}, ID: 923,
		ClassName: StyleClassID("text-parent")})
	r.Text(TextProps{Text: "Transparent", ClassName: StyleClassID("transparent")})
	r.Text(TextProps{Text: "Half", ClassName: StyleClassID("half")})
	r.Text(TextProps{Text: "Hidden", ClassName: StyleClassID("hidden")})
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
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.state.fonts;
Button.state-font { font-size: 17; padding-x: 8; }
Button.state-font:hover { font-size: 27; padding-x: 19; }
Button.state-font:pressed { font-size: 27; padding-x: 19; }
Button.state-font:focus { font-size: 27; padding-x: 19; }
Button.state-font:disabled { font-size: 27; padding-x: 19; }
Button.state-font:loading { font-size: 27; padding-x: 19; }
Button.state-font:selected { font-size: 27; padding-x: 19; }`, "Button State Fonts", "") {
		t.Fatal("button state font style pack did not register")
	}
	for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed, ButtonStateFocus,
		ButtonStateDisabled, ButtonStateLoading, ButtonStateSelected} {
		props := ButtonProps{Label: "Measured", State: state, ID: 11,
			ClassName: StyleClassID("state-font")}
		r := New(AppConfig{Width: 400, Height: 200}).(*runtime)
		r.Button(props)
		wantFont, padding := int32(27), float32(19)
		if state == ButtonStateNormal {
			wantFont, padding = 17, 8
		}
		ops := r.FrameOps()
		if len(ops) != 1 || ops[0].Button.Font != wantFont ||
			ops[0].Bounds.Width != float32(runtimeTextWidth(props.Label, wantFont))+padding*2 {
			t.Fatalf("state %v must measure and paint the same font %d: %+v", state, wantFont, ops)
		}
	}
}

func TestTransparentTextInheritanceAcrossDisabledScope(t *testing.T) {
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.transparent.text;
Button.transparent-text { foreground: #11223300; font-size: 27; }
Text.outside-text { foreground: #445566; font-size: 13; }`, "Button Transparent Text", "") {
		t.Fatal("button transparent text style pack did not register")
	}
	r := New(AppConfig{Width: 320, Height: 160}).(*runtime)
	r.BeginFrame()
	r.ButtonScope(ButtonProps{Bounds: Rectangle{Width: 200, Height: 100}, ID: 992,
		ClassName: StyleClassID("transparent-text")})
	r.BeginDisabled(true)
	r.Column(ColumnProps{Bounds: Rectangle{Width: 180, Height: 60}})
	r.Text(TextProps{Text: "Transparent", Wrap: TextWrapNone})
	r.End()
	r.EndDisabled()
	r.End()
	r.Text(TextProps{Text: "Outside", ClassName: StyleClassID("outside-text")})
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
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.disabled.text;
Button.disabled-text:disabled { foreground: #11223380; }
Text.explicit-disabled { foreground: #44556680; }`, "Button Disabled Text", "") {
		t.Fatal("button disabled text style pack did not register")
	}
	for _, scoped := range []bool{false, true} {
		r := New(AppConfig{Width: 240, Height: 140}).(*runtime)
		r.BeginFrame()
		r.BeginDisabled(scoped)
		r.ButtonScope(ButtonProps{Bounds: Rectangle{Width: 220, Height: 120}, ID: 963, Disabled: !scoped,
			ClassName: StyleClassID("disabled-text")})
		r.Text(TextProps{Text: "Inherited"})
		r.Text(TextProps{Text: "Explicit", ClassName: StyleClassID("explicit-disabled")})
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
	ClearStylePacks()
	t.Cleanup(ClearStylePacks)
	if !RegisterStylePackSource(`@pack test.button.inherit.text;
Button.inherit-text { foreground: #11223300; font-size: 27; }
Text.explicit-text { foreground: #445566; font-size: 13; }`, "Button Inherit Text", "") {
		t.Fatal("button inherit text style pack did not register")
	}
	r := New(AppConfig{Width: 320, Height: 160}).(*runtime)
	r.ButtonScope(ButtonProps{Bounds: Rectangle{X: 20, Y: 20, Width: 200, Height: 100}, ID: 991,
		ClassName: StyleClassID("inherit-text")})
	r.Text(TextProps{Text: "Inherited", Wrap: TextWrapNone})
	r.Column(ColumnProps{Bounds: Rectangle{Width: 180, Height: 60}})
	r.Text(TextProps{Text: "Nested", Wrap: TextWrapNone})
	r.Text(TextProps{Text: "Explicit", ClassName: StyleClassID("explicit-text"), Wrap: TextWrapNone})
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

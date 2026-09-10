package kryon

import (
	"reflect"
	"testing"
)

func TestSharedControlSizeSelection(t *testing.T) {
	for _, test := range []struct {
		size ControlSize
		want float32
	}{{ControlSizeMedium, 40}, {ControlSizeSmall, 32}, {ControlSizeLarge, 48}, {-1, 40}, {99, 40}} {
		if got := Style_SizeValue(int32(test.size), 32, 40, 48); got != test.want {
			t.Fatalf("size %d: got %g, want %g", test.size, got, test.want)
		}
	}
}

func TestTypefaceStylePresenceAndButtonMeasurement(t *testing.T) {
	base := Style{Fields: StyleTypeface, Typeface: "semibold"}
	if got := mergeStyle(base, Style{Typeface: "ignored"}); got.Typeface != "semibold" {
		t.Fatal("absent typeface override replaced the inherited face")
	}
	if got := mergeStyle(base, Style{Fields: StyleTypeface}); got.Typeface != "" {
		t.Fatal("explicit empty typeface must restore the default face")
	}
	r := New(AppConfig{Width: 500, Height: 100}).(*runtime)
	r.BeginFrame()
	for _, name := range []string{"semibold", "", "unknown-face"} {
		props := ButtonProps{Label: "Measure this label", State: ButtonStateNormal,
			Style: ControlStyle{Normal: Style{Fields: StyleTypeface, Typeface: name}}}
		frame, _ := r.surfaceButtonFrame(props, Rectangle{}, false)
		fontID := registeredTypeface(name)
		if frame.FontID != fontID {
			t.Fatalf("typeface %q did not reach rendering", name)
		}
		style := resolveButtonStyle(r.theme(), r.effectiveDark(), r.activeTheme, props, ButtonStateNormal)
		want := float32(runtimeTextWidthWithFont(props.Label, frame.Button.Font, fontID)) + 2*style.PaddingX
		if frame.Bounds.Width != want {
			t.Fatalf("typeface %q measurement=%g want=%g", name, frame.Bounds.Width, want)
		}
	}
	r.EndFrame()
}

func TestSharedInteractionStatePrecedence(t *testing.T) {
	for explicit := int32(0); explicit <= 7; explicit++ {
		for flags := 0; flags < 64; flags++ {
			disabled, loading := flags&1 != 0, flags&2 != 0
			pressed, hovered := flags&4 != 0, flags&8 != 0
			focused, selected := flags&16 != 0, flags&32 != 0
			want := int32(int32(ButtonStateNormal))
			for _, candidate := range []struct {
				active bool
				state  int32
			}{
				{selected, int32(ButtonStateSelected)},
				{focused, int32(ButtonStateFocus)},
				{hovered, int32(ButtonStateHover)},
				{pressed, int32(ButtonStatePressed)},
				{explicit != int32(ButtonStateAuto), explicit},
				{loading, int32(ButtonStateLoading)},
				{disabled, int32(ButtonStateDisabled)},
			} {
				if candidate.active {
					want = candidate.state
				}
			}
			got := Style_ResolveState(explicit, disabled, loading, pressed, hovered, focused, selected)
			if got != want {
				t.Fatalf("explicit %d flags %d: state %d, want %d", explicit, flags, got, want)
			}
			interaction := Style_ResolveInteraction(explicit, disabled, loading, pressed, hovered, focused, selected)
			expected := InteractionState{State: want, Hovered: hovered, Pressed: pressed, Focused: focused}
			if explicit != int32(ButtonStateAuto) {
				expected.Hovered = want == int32(ButtonStateHover)
				expected.Pressed = want == int32(ButtonStatePressed)
				expected.Focused = want == int32(ButtonStateFocus)
			}
			if interaction != expected {
				t.Fatalf("explicit %d flags %d: interaction %+v, want %+v", explicit, flags, interaction, expected)
			}
		}
	}
}

func TestSharedExplicitStateFlags(t *testing.T) {
	for state := int32(0); state <= 7; state++ {
		for bits := 0; bits < 8; bits++ {
			got := Style_ResolveFlags(state, bits&1 != 0, bits&2 != 0, bits&4 != 0)
			want := StateFlags{
				Disabled: bits&1 != 0 || state == int32(ButtonStateDisabled),
				Loading:  bits&2 != 0 || state == int32(ButtonStateLoading),
				Selected: bits&4 != 0 || state == int32(ButtonStateSelected),
			}
			if got != want {
				t.Fatalf("state %d bits %d: flags %+v, want %+v", state, bits, got, want)
			}
		}
	}
}

func TestStyleFrameKeepsValuesAndGradientPresenceTogether(t *testing.T) {
	layout := StyleData{PaddingX: 11, FontSize: 19}
	normal := StyleData{Fields: StyleBackgroundEnd, Background: 0x112233ff, BackgroundEnd: 0x12345600}
	hover := StyleData{Background: 0x44556680}
	press := StyleData{}
	focus := StyleData{Fields: StyleBackgroundEnd, Background: 0x77889900}
	for _, amounts := range [][3]float32{{0, 0, 0}, {0.5, 0.25, 0.75}, {1, 1, 1}} {
		h, p, f := amounts[0], amounts[1], amounts[2]
		frame := Style_TransitionFrame(layout, normal, hover, press, focus, h, p, f)
		want := Style_TransitionValues(layout, normal, hover, press, focus, h, p, f)
		if frame.Value != want || frame.Value.PaddingX != 11 || frame.Value.FontSize != 19 {
			t.Fatalf("style frame changed value policy: %+v", frame)
		}
		fill := frame.Fill
		if !fill.Normal || fill.Hover || fill.Press || !fill.Focus ||
			fill.NormalEnd != 0x12345600 || fill.FocusEnd != 0 || fill.HoverStart != hover.Background ||
			fill.PressStart != 0 || fill.HoverAmount != h || fill.PressAmount != p || fill.FocusAmount != f {
			t.Fatalf("style frame lost gradient presence or transition amounts: %+v", fill)
		}
	}
}

func TestSharedStyleTransitionPreservesLayoutAndStatePrecedence(t *testing.T) {
	layout := StyleData{Fields: StyleBackgroundEnd, BackgroundEnd: 0x12345600,
		PaddingX: 11, PaddingY: 13, FontSize: 19, IconSize: 18, Gap: 7}
	normal := StyleData{Background: 0x112233ff, Radius: 2}
	hover := StyleData{Background: 0x44556680, Radius: 4}
	focus := StyleData{Background: 0x77889900, Radius: 6}
	press := StyleData{Background: 0, Radius: 8}
	mixed := Style_TransitionValues(layout, normal, hover, press, focus, 0.5, 0.5, 0.5)
	if mixed.Radius != 6 || mixed.Fields != layout.Fields || mixed.BackgroundEnd != layout.BackgroundEnd ||
		mixed.PaddingX != 11 || mixed.PaddingY != 13 || mixed.FontSize != 19 || mixed.IconSize != 18 || mixed.Gap != 7 {
		t.Fatalf("shared transition changed layout or lost interaction precedence: %+v", mixed)
	}
	for _, test := range []struct {
		h, p, f float32
		want    uint32
	}{
		{0, 0, 0, normal.Background},
		{0, 0, 1, focus.Background},
		{1, 0, 1, hover.Background},
		{1, 1, 1, 0},
	} {
		got := Style_TransitionValues(layout, normal, hover, press, focus, test.h, test.p, test.f)
		if got.Background != test.want {
			t.Fatalf("interaction endpoint %+v: got %#x", test, got.Background)
		}
	}
}

func TestDarkPrimaryFocusAndDisabledFaces(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	theme := ThemeDefaultDark()
	r.SetTheme(theme)
	props := ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisFilled}
	focus := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateFocus)
	disabled := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateDisabled)
	if focus.Background != theme.Colors.AccentPressed {
		t.Fatal("focus must use the deeper primary face")
	}
	if disabled.Background.B >= focus.Background.B || disabled.Background.B <= theme.Colors.Surface.B {
		t.Fatal("disabled primary must retain only a subdued tint")
	}
	if focus.Focus != theme.Colors.Focus {
		t.Fatal("deeper face must not remove the theme's focus indicator")
	}
}

func TestFocusPolicyAndExplicitOverrides(t *testing.T) {
	for _, dark := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		theme := ThemeDefaultLight()
		if dark {
			theme = ThemeDefaultDark()
		}
		r.SetTheme(theme)
		for _, emphasis := range []ButtonEmphasis{ButtonEmphasisFilled, ButtonEmphasisSoft, ButtonEmphasisOutline, ButtonEmphasisGhost, ButtonEmphasisLink} {
			props := ButtonProps{Tone: ButtonToneSuccess, Emphasis: emphasis}
			paint := resolveButtonStyle(r.theme(), dark, r.activeTheme, props, ButtonStateFocus)
			expected := theme.Colors.Focus
			if dark {
				expected = unpackRGBA(Surface_ChromaColor(packRGBA(theme.Colors.Success), 255))
				expected.A = theme.Colors.Focus.A
			} else {
				expected.R, expected.G, expected.B = theme.Colors.Success.R, theme.Colors.Success.G, theme.Colors.Success.B
				if emphasis == ButtonEmphasisSoft || emphasis == ButtonEmphasisGhost || emphasis == ButtonEmphasisLink {
					expected.A = uint8(uint32(expected.A) * 18 / 100)
				}
			}
			if paint.Focus != expected {
				t.Fatalf("dark=%v emphasis=%v: focus=%+v want %+v", dark, emphasis, paint.Focus, expected)
			}
			for _, custom := range []Color{{}, {R: 210, G: 20, B: 170, A: 128}} {
				props.Style.Focused = Style{Fields: StyleFocus, Focus: custom}
				paint = resolveButtonStyle(r.theme(), dark, r.activeTheme, props, ButtonStateFocus)
				if paint.Focus != custom {
					t.Fatal("default focus policy overrode an explicit style")
				}
			}
		}
	}
	if Button_ButtonFocus(int32(ButtonToneSuccess), int32(ButtonEmphasisFilled), 0xffffffff, 0x006cff00, 0x00856aff)&255 != 0 {
		t.Fatal("semantic hue must preserve transparent theme focus")
	}
}

func TestDarkSemanticFocusSeparatesOuterEdgeFromMaterial(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	for _, alpha := range []uint8{0, 128, 255} {
		theme := ThemeDefaultDark()
		theme.Colors.Focus.A = alpha
		r.SetTheme(theme)
		for _, test := range []struct {
			tone  ButtonTone
			color Color
		}{
			{ButtonToneDanger, theme.Colors.Focus},
			{ButtonToneSuccess, Color{0, 255, 195, alpha}},
			{ButtonToneWarning, theme.Colors.Focus},
		} {
			props := ButtonProps{Tone: test.tone}
			paint := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateFocus)
			if paint.Focus != test.color {
				t.Fatalf("semantic focus tone=%d: got %v want %v", test.tone, paint.Focus, test.color)
			}
			props.Style.Focused = Style{Fields: StyleFocus, Focus: Color{100, 80, 200, 96}}
			paint = resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateFocus)
			if paint.Focus != props.Style.Focused.Focus {
				t.Fatal("semantic focus must not recolor an explicit style")
			}
		}
	}
}

func TestFocusVolumeRetainsMaterialAndClipsInward(t *testing.T) {
	for _, material := range []uint32{0xff0017ff, 0x00ffc3ff, 0xffb100ff} {
		for _, alpha := range []uint32{0, 128, 255} {
			focus := uint32(0x409cff00) | alpha
			volume := Surface_LightfieldLayer(6, 72, 40, 8, 1,
				0x808080ff, material, material, focus, 0, 0, 1, false, 1, 0x092039ff)
			want := Surface_DepthColor(Surface_DepthColor(material&0xffffff00|alpha, 1), 0.4)
			if volume.Color != want || volume.InnerBlur != 12 || volume.Blur != 0 {
				t.Fatalf("material=%08x alpha=%d: inner volume=%+v want color=%08x", material, alpha, volume, want)
			}
			if Surface_InnerBlurCoverage(-1, 20, 72, 40, 8, volume.InnerBlur) != 0 {
				t.Fatal("inner focus volume leaked outside the surface")
			}
			edge := Surface_LightfieldLayer(7, 72, 40, 8, 1,
				0x808080ff, material, material, focus, 0, 0, 1, false, 1, 0x092039ff)
			if edge.Color != Surface_GradientColor(focus, 0xffffff00|alpha, 0.50) {
				t.Fatal("material hue replaced the outer focus edge")
			}
		}
	}
}

func TestDarkLoadingMaterialUsesOneSemanticTint(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	for _, alpha := range []uint8{0, 128, 255} {
		theme := ThemeDefaultDark()
		theme.Colors.Surface.A = alpha
		theme.Colors.Danger.A = alpha
		theme.Colors.Success.A = alpha
		theme.Colors.Warning.A = alpha
		r.SetTheme(theme)
		for _, test := range []struct {
			tone  ButtonTone
			color Color
		}{
			{ButtonToneDanger, theme.Colors.Danger},
			{ButtonToneSuccess, theme.Colors.Success},
			{ButtonToneWarning, theme.Colors.Warning},
		} {
			props := ButtonProps{Tone: test.tone, Loading: true}
			paint := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateAuto)
			want := Button_MixColor(packRGBA(theme.Colors.Surface), packRGBA(test.color), 14)
			if packRGBA(paint.Background) != want || paint.Background.A != alpha {
				t.Fatalf("tone=%d alpha=%d: background=%v want=%08x", test.tone, alpha, paint.Background, want)
			}
			props.Style.Loading = Style{Fields: StyleBackground, Background: Color{R: 20, G: 40, B: 60}}
			paint = resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateAuto)
			if paint.Background != props.Style.Loading.Background {
				t.Fatal("loading default replaced an explicit transparent background")
			}
		}
	}
}

func TestLightPrimaryStateContrast(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	theme := ThemeDefaultLight()
	r.SetTheme(theme)
	props := ButtonProps{Tone: ButtonToneAccent, Emphasis: ButtonEmphasisFilled}
	for _, state := range []ButtonState{ButtonStateHover, ButtonStatePressed, ButtonStateFocus} {
		paint := resolveButtonStyle(r.theme(), false, r.activeTheme, props, state)
		if paint.Foreground != theme.Colors.Link {
			t.Fatalf("light primary state %d should use blue ink, got %+v", state, paint.Foreground)
		}
	}
	for _, state := range []ButtonState{ButtonStateFocus, ButtonStateDisabled, ButtonStateLoading} {
		paint := resolveButtonStyle(r.theme(), false, r.activeTheme, props, state)
		if paint.Background.R < 220 || paint.Background.G < 230 {
			t.Fatalf("light primary state %d should have a pale face: %+v", state, paint.Background)
		}
	}
}

func TestButtonUsesDeclaredThemeSurfaces(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	theme := ThemeDefaultDark()
	theme.Colors.Surface = Color{R: 12, G: 23, B: 34, A: 0}
	theme.Colors.SurfaceRaised = Color{R: 45, G: 56, B: 67, A: 89}
	r.SetTheme(theme)
	props := ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisFilled}
	got := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateNormal)
	if got.Background != theme.Colors.SurfaceRaised {
		t.Fatalf("declared raised surface replaced: %+v", got.Background)
	}
	props.Emphasis = ButtonEmphasisOutline
	got = resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateNormal)
	if got.Background != theme.Colors.Surface {
		t.Fatalf("transparent theme surface replaced: %+v", got.Background)
	}
}

func TestPlainAppUsesDefaultThemeFamily(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	if r.themeStyle != ThemeStyleDefault || r.GetThemeFamily().Name != "Default" {
		t.Fatal("plain app did not select the shared default style and family")
	}
	for _, mode := range []ThemeMode{ThemeModeLight, ThemeModeDark} {
		r.SetThemeMode(mode)
		want := ThemeDefaultLight()
		if mode == ThemeModeDark {
			want = ThemeDefaultDark()
		}
		if r.GetThemeBackground() != want.Colors.Background || r.GetTheme().Colors != want.Colors {
			t.Fatalf("default family failed to follow mode %d", mode)
		}
	}
}

func TestStyleAdaptersPreserveEveryField(t *testing.T) {
	value := StyleData{Fields: 32767, Material: 1, Background: 0x10203000, BackgroundEnd: 0x18385800, Foreground: 0x40506080,
		Border: 0x708090ff, Focus: 0xa0b0c040, Radius: 3, BorderWidth: 1.5,
		Opacity: 0.7, PaddingX: 12, PaddingY: 8, Gap: 6, FontSize: 18,
		IconSize: 16, OffsetX: -3, OffsetY: 2}
	if got := packStyle(unpackStyle(value)); got != value {
		t.Fatalf("style adapter lost values: got %+v, want %+v", got, value)
	}
	base := unpackStyle(value)
	fields := []string{"Background", "Foreground", "Border", "Focus", "Radius",
		"BorderWidth", "Opacity", "PaddingX", "PaddingY", "Gap", "FontSize",
		"IconSize", "ContentOffset", "BackgroundEnd", "Material"}
	for index, field := range fields {
		t.Run(field, func(t *testing.T) {
			want := base
			v := reflect.ValueOf(&want).Elem().FieldByName(field)
			v.Set(reflect.Zero(v.Type()))
			got := mergeStyle(base, Style{Fields: uint32(1) << index})
			if got != want {
				t.Fatalf("explicit zero did not replace only %s: got %+v, want %+v", field, got, want)
			}
		})
	}
	if got := mergeStyle(base, Style{Radius: 99}); got != base {
		t.Fatal("a value without a presence flag must not override the base")
	}
}

func TestStyleStateSelection(t *testing.T) {
	style := func(radius float32) Style { return Style{Fields: StyleRadius, Radius: radius} }
	control := ControlStyle{Normal: style(1), Hover: style(2), Pressed: style(3),
		Focused: style(4), Disabled: style(5), Loading: style(6), Selected: style(7)}
	for state := ButtonStateNormal; state <= ButtonStateSelected; state++ {
		got := resolveControlStyle(Style{Fields: StyleGap, Gap: 6}, control, state)
		if got.Radius != float32(state) || got.Gap != 6 {
			t.Fatalf("state %d resolved incorrectly: %+v", state, got)
		}
	}
}

func TestDarkSecondaryHoverCatchesCoolLight(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	r.SetThemeMode(ThemeModeDark)
	props := ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft}
	normal := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateNormal)
	hover := resolveButtonStyle(r.theme(), true, r.activeTheme, props, ButtonStateHover)
	// The body keeps a restrained tint; the shared material adds the brighter
	// lower reflection. Do not restore the old saturated-blue hover face.
	if normal.Background != (Color{19, 52, 83, 255}) ||
		hover.Background != (Color{19, 62, 104, 255}) {
		t.Fatalf("secondary hover lost its restrained cool reflection: normal=%+v hover=%+v",
			normal.Background, hover.Background)
	}
}

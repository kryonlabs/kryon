package kryon

import (
	"bufio"
	"encoding/json"
	"os"
	"testing"
)

func TestButtonAppearanceResolvesDefaultsAndOverridesWithOneState(t *testing.T) {
	styles := StyleStates{
		Normal:   StyleData{Fields: StyleRadius | StyleForeground, Radius: 1, Foreground: 0x11223300},
		Hover:    StyleData{Fields: StyleRadius, Radius: 2},
		Pressed:  StyleData{Fields: StyleRadius, Radius: 3},
		Focused:  StyleData{Fields: StyleRadius, Radius: 4},
		Disabled: StyleData{Fields: StyleRadius, Radius: 5},
		Loading:  StyleData{Fields: StyleRadius, Radius: 6},
		Selected: StyleData{Fields: StyleRadius, Radius: 7},
	}
	for _, dark := range []bool{false, true} {
		palette, metrics := Theme_DefaultPalette(dark), Theme_DefaultMetrics()
		for state := int32(0); state <= 7; state++ {
			for flags := 0; flags < 8; flags++ {
				disabled, loading, selected := flags&1 != 0, flags&2 != 0, flags&4 != 0
				effective := state
				if disabled || state == int32(ButtonStateDisabled) {
					effective = 5
				} else if loading || state == int32(ButtonStateLoading) {
					effective = 6
				} else if effective == 0 {
					effective = 1
					if selected {
						effective = 7
					}
				}
				actual := Button_ResolveAppearance(1, 0, state, 0, false, false,
					disabled, loading, selected, palette, metrics, styles)
				defaults := Button_DefaultButtonStyle(1, 0, effective, 0, false, false, palette, metrics)
				if actual.Radius != float32(effective) || actual.Background != defaults.Background || actual.Foreground != 0x11223300 {
					t.Fatalf("state %d flags %d: defaults and overrides diverged: %+v", state, flags, actual)
				}
			}
		}
	}
}

func TestFallbackButtonSemanticsUseSharedThemeDefaults(t *testing.T) {
	for _, dark := range []bool{false, true} {
		r := New(AppConfig{}).(*runtime)
		mode := int32(0)
		if dark {
			mode = 1
		}
		r.SetCurrentTheme(0, mode)
		if r.activeTheme != nil {
			t.Fatal("test must exercise the catalog/system fallback")
		}
		defaults := Theme_DefaultPalette(dark)
		for _, test := range []struct {
			tone  ButtonTone
			color uint32
		}{{ButtonToneDanger, defaults.Danger}, {ButtonToneSuccess, defaults.Success},
			{ButtonToneWarning, defaults.Warning}} {
			style := resolveButtonStyle(r.theme(), dark, nil, ButtonProps{Tone: test.tone}, ButtonStateLoading)
			if packRGBA(style.Foreground) != test.color {
				t.Fatalf("fallback semantic color is not from .kry defaults: %+v", style.Foreground)
			}
		}
		amount := uint32(90)
		if Button_LightSurface(packRGBA(r.theme().surface)) {
			amount = 35
		}
		want := Button_MixColor(defaults.TextDisabled, packRGBA(r.theme().text), amount)
		disabled := resolveButtonStyle(r.theme(), dark, nil, ButtonProps{}, ButtonStateDisabled)
		if packRGBA(disabled.Foreground) != want {
			t.Fatal("fallback disabled text must use the shared theme color and alpha")
		}
	}
}

func TestNeutralHoverRimUsesThemeAccent(t *testing.T) {
	for _, theme := range []Theme{ThemeDefaultDark(), ThemeDefaultLight()} {
		theme.Colors.Accent = Color{170, 45, 220, 255}
		r := New(AppConfig{}).(*runtime)
		r.SetTheme(theme)
		props := ButtonProps{Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisSoft}
		dark := r.effectiveDark()
		normal := resolveButtonStyle(r.theme(), dark, r.activeTheme, props, ButtonStateNormal)
		hover := resolveButtonStyle(r.theme(), dark, r.activeTheme, props, ButtonStateHover)
		if dark {
			want := unpackRGBA(Button_MixColor(packRGBA(theme.Colors.SurfaceRaised), packRGBA(theme.Colors.Accent), 45))
			if hover.Border != want || hover.Border == normal.Border {
				t.Fatalf("dark hover must catch the current accent: got %+v, want %+v", hover.Border, want)
			}
		} else {
			want := unpackRGBA(Button_MixColor(packRGBA(theme.Colors.Surface), packRGBA(theme.Colors.Accent), 14))
			if hover.Border != normal.Border || normal.Border != want {
				t.Fatal("light neutral material must keep a quiet accent-reflecting edge")
			}
			semantic := props
			semantic.Tone = ButtonToneDanger
			danger := resolveButtonStyle(r.theme(), dark, r.activeTheme, semantic, ButtonStateNormal)
			wantDanger := unpackRGBA(Button_MixColor(packRGBA(theme.Colors.Surface), packRGBA(theme.Colors.Danger), 14))
			if danger.Border != wantDanger {
				t.Fatal("semantic soft material must retain its own edge hue")
			}
		}
		props.Style.Hover = Style{Fields: StyleBorder, Border: Color{17, 34, 51, 0}}
		custom := resolveButtonStyle(r.theme(), dark, r.activeTheme, props, ButtonStateHover)
		if custom.Border != props.Style.Hover.Border {
			t.Fatal("explicit hover border must override the theme default, including transparency")
		}
	}
}

func TestLightPressedAccentEdgeKeepsFocusDistinct(t *testing.T) {
	theme := ThemeDefaultLight()
	theme.Colors.Accent = Color{170, 45, 220, 255}
	r := New(AppConfig{}).(*runtime)
	r.SetTheme(theme)
	for _, emphasis := range []ButtonEmphasis{ButtonEmphasisFilled, ButtonEmphasisOutline} {
		props := ButtonProps{Tone: ButtonToneAccent, Emphasis: emphasis}
		normal := resolveButtonStyle(r.theme(), false, r.activeTheme, props, ButtonStateNormal)
		pressed := resolveButtonStyle(r.theme(), false, r.activeTheme, props, ButtonStatePressed)
		focused := resolveButtonStyle(r.theme(), false, r.activeTheme, props, ButtonStateFocus)
		want := unpackRGBA(Button_MixColor(packRGBA(theme.Colors.Surface), packRGBA(theme.Colors.Accent), 25))
		if pressed.Border != want || normal.Border != theme.Colors.Accent || focused.Border != normal.Border {
			t.Fatalf("pressed edge must recede without changing resting/focused edges: %+v %+v %+v", normal, pressed, focused)
		}
		props.Style.Pressed = Style{Fields: StyleBorder, Border: Color{17, 34, 51, 0}}
		custom := resolveButtonStyle(r.theme(), false, r.activeTheme, props, ButtonStatePressed)
		if custom.Border != props.Style.Pressed.Border {
			t.Fatal("explicit pressed border must win, including transparency")
		}
	}
}

func TestLightSemanticInkPreservesToneAlphaAndOverrides(t *testing.T) {
	for _, alpha := range []uint8{0, 128, 255} {
		theme := ThemeDefaultLight()
		theme.Colors.Success = Color{20, 160, 100, alpha}
		theme.Colors.Warning = Color{200, 120, 20, alpha}
		r := New(AppConfig{}).(*runtime)
		r.SetTheme(theme)
		for _, test := range []struct {
			tone ButtonTone
			ink  Color
		}{{ButtonToneSuccess, Color{10, 80, 50, alpha}},
			{ButtonToneWarning, Color{80, 48, 8, alpha}}} {
			for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover,
				ButtonStatePressed, ButtonStateFocus, ButtonStateSelected} {
				props := ButtonProps{Tone: test.tone, State: state}
				style := resolveButtonStyle(r.theme(), false, r.activeTheme, props, state)
				if style.Foreground != test.ink {
					t.Fatalf("tone %v state %v alpha %d: got %+v, want %+v",
						test.tone, state, alpha, style.Foreground, test.ink)
				}
				props.Style.Normal = Style{Fields: StyleForeground, Foreground: Color{17, 34, 51, 0}}
				custom := resolveButtonStyle(r.theme(), false, r.activeTheme, props, state)
				if custom.Foreground != props.Style.Normal.Foreground {
					t.Fatal("explicit foreground must replace semantic ink, including transparent ink")
				}
			}
		}
	}
}

func TestDarkPressedWarningInkPreservesOpacityAndOverrides(t *testing.T) {
	for _, alpha := range []uint8{0, 128, 255} {
		theme := ThemeDefaultDark()
		theme.Colors.OnWarning.A = alpha
		r := New(AppConfig{}).(*runtime)
		r.SetTheme(theme)
		props := ButtonProps{Tone: ButtonToneWarning}
		for _, state := range []ButtonState{ButtonStateNormal, ButtonStateHover, ButtonStatePressed, ButtonStateFocus} {
			want := theme.Colors.OnWarning
			if state == ButtonStatePressed {
				warm := theme.Colors.Warning
				warm.A = alpha
				want = unpackRGBA(Button_MixColor(packRGBA(want), packRGBA(warm), 15))
			}
			style := resolveButtonStyle(r.theme(), false, r.activeTheme, props, state)
			if style.Foreground != want {
				t.Fatalf("state %v alpha %d: got %+v, want %+v", state, alpha, style.Foreground, want)
			}
		}
		props.Style.Pressed = Style{Fields: StyleForeground, Foreground: Color{17, 34, 51, 0}}
		custom := resolveButtonStyle(r.theme(), false, r.activeTheme, props, ButtonStatePressed)
		if custom.Foreground != props.Style.Pressed.Foreground {
			t.Fatal("explicit pressed foreground must replace the warm ink")
		}
	}
}

// The fixture is emitted by the real C style resolver in the same build.
// No checked-in golden can hide a difference between the two adapters.
func TestButtonStyleParityWithC(t *testing.T) {
	path := os.Getenv("KRYON_BUTTON_STYLE_FIXTURE")
	if path == "" {
		t.Skip("run make button-style-parity-test for the C/Go matrix")
	}
	file, err := os.Open(path)
	if err != nil {
		t.Fatal(err)
	}
	defer file.Close()
	r := New(AppConfig{}).(*runtime)
	scanner := bufio.NewScanner(file)
	count := 0
	for scanner.Scan() {
		var expected []float64
		if err := json.Unmarshal(scanner.Bytes(), &expected); err != nil {
			t.Fatal(err)
		}
		if len(expected) != 22 {
			t.Fatalf("malformed C style record: %v", expected)
		}
		theme := ThemeDefaultLight()
		if expected[0] != 0 {
			theme = ThemeDefaultDark()
		}
		r.SetTheme(theme)
		props := ButtonProps{Tone: ButtonTone(expected[1]), Emphasis: ButtonEmphasis(expected[2]), Size: ControlSize(expected[4])}
		style := resolveButtonStyle(r.theme(), expected[0] != 0, r.activeTheme, props, ButtonState(expected[3]))
		actual := []float64{expected[0], expected[1], expected[2], expected[3], expected[4],
			float64(style.Fields), float64(packRGBA(style.Background)), float64(packRGBA(style.Foreground)),
			float64(packRGBA(style.Border)), float64(packRGBA(style.Focus)),
			float64(style.Radius), float64(style.BorderWidth), float64(style.Opacity),
			float64(style.PaddingX), float64(style.PaddingY), float64(style.Gap),
			float64(style.FontSize), float64(style.IconSize), float64(style.ContentOffset.X), float64(style.ContentOffset.Y),
			float64(packRGBA(style.BackgroundEnd)), float64(style.Material)}
		for index := range actual {
			if actual[index] != expected[index] {
				t.Fatalf("C/Go style mismatch for dark/tone/emphasis/state/size %v, field %d: Go=%v C=%v",
					expected[:5], index, actual[index], expected[index])
			}
		}
		count++
	}
	if err := scanner.Err(); err != nil {
		t.Fatal(err)
	}
	if count != 1050 {
		t.Fatalf("checked %d cases, want 1050", count)
	}
}

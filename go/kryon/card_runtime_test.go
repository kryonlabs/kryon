package kryon

import "testing"

func TestCardButtonPropsHasNoVisualDefaults(t *testing.T) {
	card := CardProps{
		Bounds:    Rectangle{X: 8, Y: 9, Width: 120, Height: 48},
		ID:        77,
		Clickable: true,
		Tone:      ButtonToneNeutral,
		Emphasis:  ButtonEmphasisFilled,
		Style: ControlStyle{
			Normal: Style{Fields: StyleRadius | StyleBackground,
				Radius: 3, Background: Color{1, 2, 3, 4}},
			Hover: Style{Fields: StyleBorder, Border: Color{5, 6, 7, 8}},
		},
	}

	button := Card_CardButtonProps(card, 12, 1, 16, 10,
		Color{32, 40, 48, 255}, Color{180, 90, 40, 220})

	if button.ID != card.ID || button.Bounds != card.Bounds {
		t.Fatal("card button props must preserve clickable identity and bounds")
	}
	if button.Emphasis != ButtonEmphasisFilled {
		t.Fatal("explicit style overrides should keep requested emphasis")
	}
	if button.Style.Normal.Radius != 3 ||
		button.Style.Normal.Background != (Color{1, 2, 3, 4}) {
		t.Fatalf("normal overrides did not win: %+v", button.Style.Normal)
	}
	if button.Style.Normal.PaddingX != 0 || button.Style.Normal.PaddingY != 0 {
		t.Fatal("card must not provide hidden padding defaults")
	}
	if button.Style.Normal.Material != 0 || button.Style.Hover.Material != 0 {
		t.Fatal("card must not provide hidden material defaults")
	}
	if button.Style.Hover.Border != (Color{5, 6, 7, 8}) {
		t.Fatal("hover override was not preserved")
	}

	plain := Card_CardButtonProps(CardProps{
		Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisFilled,
	}, 12, 1, 16, 10, Color{32, 40, 48, 255}, Color{180, 90, 40, 220})
	if plain.Emphasis != ButtonEmphasisFilled || plain.Style.Normal.Fields != 0 {
		t.Fatal("plain card should preserve semantics and carry no visual style")
	}
}

func TestCardRuntimeUsesCardStyleFacts(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterBuiltInStylePacks() {
		t.Fatal("built-in style packs did not register")
	}

	r := New(AppConfig{}).(*runtime)
	button := Card_CardButtonProps(CardProps{
		Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisFilled,
	}, 12, 1, 16, 10, Color{32, 40, 48, 255}, Color{180, 90, 40, 220})
	cardStyle := resolveButtonStyleForKind(r.theme(), r.effectiveDark(),
		r.activeTheme, button, ButtonStateNormal, StyleSheet_StyleKindCard())
	buttonStyle := resolveButtonStyleForKind(r.theme(), r.effectiveDark(),
		r.activeTheme, button, ButtonStateNormal, StyleSheet_StyleKindButton())

	if cardStyle.Background != (Color{0x1a, 0x1f, 0x29, 0xff}) {
		t.Fatalf("card did not resolve Material Card background: %+v", cardStyle.Background)
	}
	if buttonStyle.Background == cardStyle.Background {
		t.Fatal("card runtime should use Card style facts, not Button facts")
	}
}

func TestCardRuntimeColorExternsPreserveAlpha(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
	base := Color{64, 96, 128, 77}
	if got := r.LightenColor(base, 20); got.A != base.A || got == base {
		t.Fatalf("LightenColor must adjust lightness and preserve alpha: %+v", got)
	}
	if got := r.DarkenColor(base, 20); got.A != base.A || got == base {
		t.Fatalf("DarkenColor must adjust lightness and preserve alpha: %+v", got)
	}
}

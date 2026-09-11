package kryon

import "testing"

func TestCardButtonPropsUsesRuntimeStyleDefaults(t *testing.T) {
	r := New(AppConfig{}).(*runtime)
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

	button := r.Card_CardButtonProps(card, 12, 1, 16, 10,
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
	if button.Style.Normal.PaddingX != 16 || button.Style.Normal.PaddingY != 10 {
		t.Fatal("card defaults must still provide unset padding")
	}
	if button.Style.Normal.Material != MaterialLightfield ||
		button.Style.Hover.Material != MaterialLightfield {
		t.Fatal("card defaults must use the lightfield material")
	}
	if button.Style.Hover.Border != (Color{5, 6, 7, 8}) {
		t.Fatal("hover override did not win over generated defaults")
	}

	plain := r.Card_CardButtonProps(CardProps{
		Tone: ButtonToneNeutral, Emphasis: ButtonEmphasisFilled,
	}, 12, 1, 16, 10, Color{32, 40, 48, 255}, Color{180, 90, 40, 220})
	if plain.Emphasis != ButtonEmphasisSoft {
		t.Fatal("plain neutral filled cards should default to soft button styling")
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

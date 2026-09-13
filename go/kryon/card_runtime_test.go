package kryon

import "testing"

func TestCardButtonPropsHasNoVisualDefaults(t *testing.T) {
	card := CardProps{
		Bounds:    Rectangle{X: 8, Y: 9, Width: 120, Height: 48},
		ID:        77,
		ClassName: StyleClassID("elevated"),
		Clickable: true,
	}

	button := Card_CardButtonProps(card)

	if button.ID != card.ID || button.Bounds != card.Bounds {
		t.Fatal("card button props must preserve clickable identity and bounds")
	}
	if button.ClassName != card.ClassName {
		t.Fatal("card button props must preserve KSS class identity")
	}
	if button.Tone != 0 || button.Emphasis != 0 {
		t.Fatal("card button props should carry no visual tone/emphasis")
	}
	plain := Card_CardButtonProps(CardProps{})
	if plain.Tone != 0 || plain.Emphasis != 0 {
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
	button := Card_CardButtonProps(CardProps{})
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

func TestCardRuntimeResolvesClassSelectors(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	selector := StyleSheet_StyleDefaultSelector()
	selector.Kind = StyleSheet_StyleKindCard()
	selector.ClassName = StyleClassID("primary")
	if !RegisterStylePack(StylePack{
		ID:    "card-class",
		Label: "Card class",
		Sheet: []StyleRule{{
			Selector: selector,
			State:    StyleSheet_StyleStateAny(),
			Style: StyleData{
				Fields:     uint32(StyleBackground),
				Background: 0x123456ff,
			},
		}},
	}) {
		t.Fatal("style pack did not register")
	}

	r := New(AppConfig{}).(*runtime)
	button := Card_CardButtonProps(CardProps{
		ClassName: StyleClassID("primary"),
	})
	style := resolveButtonStyleForKind(r.theme(), r.effectiveDark(),
		r.activeTheme, button, ButtonStateNormal, StyleSheet_StyleKindCard())

	if style.Background != (Color{0x12, 0x34, 0x56, 0xff}) {
		t.Fatalf("card class selector did not resolve: %+v", style.Background)
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

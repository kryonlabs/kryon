package kryon

import "testing"

func TestStyleSheetCascadeInGo(t *testing.T) {
	button := StyleSheet_StyleDefaultSelector()
	button.Kind = StyleSheet_StyleKindButton()

	primary := StyleSheet_StyleDefaultSelector()
	primary.ClassName = 3

	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	facts.ClassName = 3

	base := StyleData{Fields: uint32(StyleOpacity), Opacity: 1}
	rules := []StyleRule{
		{
			Selector: button,
			State:    StyleSheet_StyleStateAny(),
			Style: StyleData{
				Fields:     uint32(StyleBackground),
				Background: 0x111111ff,
			},
		},
		{
			Selector: primary,
			State:    StyleSheet_StyleStateAny(),
			Order:    1,
			Style: StyleData{
				Fields:     uint32(StyleBackground | StylePaddingX),
				Background: 0x222222ff,
				PaddingX:   12,
			},
		},
		{
			Selector: primary,
			State:    int32(ButtonStateHover),
			Order:    2,
			Style: StyleData{
				Fields:     uint32(StyleBackground),
				Background: 0x333333ff,
			},
		},
	}

	cascade := StyleSheet_BeginStyleCascade(base)
	for _, rule := range rules {
		cascade = StyleSheet_ApplyStyleRule(cascade, rule, facts, int32(ButtonStateHover))
	}
	result := StyleSheet_FinishStyleCascade(cascade)

	if result.Background != 0x333333ff {
		t.Fatalf("hover class rule did not win: 0x%08x", result.Background)
	}
	if result.PaddingX != 12 {
		t.Fatalf("normal class metric was not retained: %v", result.PaddingX)
	}
	if result.Opacity != 1 {
		t.Fatalf("base opacity was not retained: %v", result.Opacity)
	}
}

func TestStylePackRegistryInGo(t *testing.T) {
	button := StyleSheet_StyleDefaultSelector()
	button.Kind = StyleSheet_StyleKindButton()

	vanilla := []StyleRule{{
		Selector: button,
		State:    StyleSheet_StyleStateAny(),
		Style: StyleData{
			Fields:     uint32(StyleBackground),
			Background: 0x111111ff,
		},
	}}
	glow := []StyleRule{{
		Selector: button,
		State:    StyleSheet_StyleStateAny(),
		Style: StyleData{
			Fields:     uint32(StyleBackground),
			Background: 0x222222ff,
		},
	}}

	ClearStylePacks()
	defer ClearStylePacks()
	version := StylePackVersion()
	base := StyleData{Fields: uint32(StyleOpacity), Opacity: 1}
	facts := StyleSheet_StyleControlFacts(StyleSheet_StyleKindButton(), 0, 0,
		int32(ButtonToneNeutral), int32(ButtonEmphasisSoft),
		int32(ControlSizeMedium), int32(ButtonStateNormal))

	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0 || resolved.Opacity != 1 {
		t.Fatalf("unstyled active resolution changed base: %#v", resolved)
	}
	if RegisterStylePack(StylePack{}) {
		t.Fatal("empty style pack registered")
	}
	if StylePackVersion() != version {
		t.Fatal("failed registration changed version")
	}
	if !RegisterStylePack(StylePack{ID: "vanilla", Label: "Vanilla", Sheet: vanilla}) {
		t.Fatal("vanilla pack did not register")
	}
	if GetActiveStylePackID() != "vanilla" {
		t.Fatalf("first pack was not active: %q", GetActiveStylePackID())
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x111111ff {
		t.Fatalf("vanilla did not resolve: 0x%08x", resolved.Background)
	}
	if !RegisterStylePack(StylePack{ID: "glow", Label: "Glow", Sheet: glow}) {
		t.Fatal("glow pack did not register")
	}
	if !SetActiveStylePack("glow") {
		t.Fatal("glow pack did not activate")
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x222222ff {
		t.Fatalf("glow did not resolve: 0x%08x", resolved.Background)
	}
	options := GetStylePackOptions()
	if len(options) != 2 || options[0].Active || !options[1].Active {
		t.Fatalf("bad options: %#v", options)
	}
	if !RegisterStylePack(StylePack{ID: "glow", Label: "Glow Updated", Sheet: vanilla}) {
		t.Fatal("replacement glow pack did not register")
	}
	if resolved := ResolveActiveStyle(base, facts, int32(ButtonStateNormal)); resolved.Background != 0x111111ff {
		t.Fatalf("replacement active pack did not resolve: 0x%08x", resolved.Background)
	}
}

func TestStylePickerEmptyRegistryInGo(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()

	r := New(AppConfig{}).(*runtime)
	if r.StylePicker(StylePickerProps{Bounds: NewRectangle(0, 0, 120, 28), ID: 42}) {
		t.Fatal("empty style picker reported a change")
	}
}

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

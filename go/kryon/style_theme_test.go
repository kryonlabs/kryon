package kryon

import "testing"

const themeTestSource = `@pack themed;
tokens { color { accent: #111111; } }
@theme light { accent: #eeeeee; }
@theme dark { accent: #222222; }
Button { background: accent; }
@variant glow "Glow" { Button { radius: 12; } }
`

func TestStyleThemeSourceRegistry(t *testing.T) {
	ClearStylePacks()
	defer ClearStylePacks()
	if !RegisterBuiltInStylePacks() || !RegisterStylePackSource(themeTestSource, "Themed", "Theme test") {
		t.Fatal("register source packs")
	}
	if !RegisterStylePackVariant("palette", themeTestSource, "Palette", []StyleColorToken{{Name: "accent", Color: 0x00ff00ff}}) {
		t.Fatal("register explicit palette")
	}
	if !SetActiveStylePack("themed.glow") {
		t.Fatal("select variant")
	}
	count := GetStylePackCount()
	facts := StyleSheet_StyleDefaultFacts(StyleSheet_StyleKindButton())
	for _, step := range []struct {
		theme string
		color uint32
	}{{"dark", 0x222222ff}, {"light", 0xeeeeeeff}, {"", 0x111111ff}, {"dark", 0x222222ff}} {
		version := StylePackVersion()
		if !SetStyleTheme(step.theme) || StylePackVersion() <= version {
			t.Fatalf("switch to %q", step.theme)
		}
		if GetActiveStylePackID() != "themed.glow" || GetStylePackCount() != count {
			t.Fatal("theme switch changed selection or registry membership")
		}
		value := ResolveActiveStyle(StyleData{}, facts, 0)
		if value.Background != step.color || value.Radius != 12 {
			t.Fatalf("theme %q lost overlay or variant: %+v", step.theme, value)
		}
		palette := FindStylePack("palette")
		if palette.Sheet[0].Style.Background != 0x00ff00ff {
			t.Fatal("theme switch changed explicit color substitutions")
		}
		for _, id := range []string{"material", "classic", "lightfield"} {
			if pack := FindStylePack(id); pack == nil || len(pack.Sheet) == 0 {
				t.Fatalf("lost built-in %s", id)
			}
		}
	}
	version := StylePackVersion()
	if !SetStyleTheme("dark") || StylePackVersion() != version {
		t.Fatal("reapplying the same theme must not invalidate the registry")
	}
	if !RegisterStylePackSource(themeTestSource, "Updated", "") {
		t.Fatal("replace source under active theme")
	}
	if FindStylePack("themed").Sheet[0].Style.Background != 0x222222ff {
		t.Fatal("registration ignored the active theme")
	}
	// Directly replacing a source pack removes its retained-source ownership.
	if !RegisterStylePack(StylePack{ID: "themed", Sheet: []StyleRule{{Style: StyleData{Background: 42}}}}) || !SetStyleTheme("light") {
		t.Fatal("replace source with typed sheet")
	}
	if FindStylePack("themed").Sheet[0].Style.Background != 42 {
		t.Fatal("theme switch resurrected a replaced source")
	}
	ClearStylePacks()
	if !SetStyleTheme("dark") || GetStylePackCount() != 0 || !RegisterStylePackSource(themeTestSource, "", "") {
		t.Fatal("theme before registration")
	}
	if FindStylePack("themed").Sheet[0].Style.Background != 0x222222ff {
		t.Fatal("late registration ignored theme")
	}
}

func TestStyleThemeFailureIsAtomic(t *testing.T) {
	ClearStylePacks()
	ClearStyleModules()
	defer ClearStylePacks()
	defer ClearStyleModules()
	RegisterStyleModule("palette", "tokens { color { accent: #123456; } }")
	if !RegisterStylePackSource(themeTestSource, "Themed", "") ||
		!RegisterStylePackSource("@pack imported; @import <palette>; Button { background: accent; }", "", "") {
		t.Fatal("register sources")
	}
	SetActiveStylePack("themed")
	version := StylePackVersion()
	ClearStyleModules()
	if SetStyleTheme("dark") {
		t.Fatal("missing import accepted")
	}
	if StylePackVersion() != version || GetActiveStylePackID() != "themed" ||
		FindStylePack("themed").Sheet[0].Style.Background != 0x111111ff {
		t.Fatal("failed theme switch partially published results")
	}
	RegisterStyleModule("palette", "tokens { color { accent: #123456; } }")
	if !SetStyleTheme("dark") || FindStylePack("themed").Sheet[0].Style.Background != 0x222222ff {
		t.Fatal("failed switch prevented recovery")
	}
}

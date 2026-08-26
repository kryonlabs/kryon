package kryon

import "testing"

// The ThemeId enum shares a const block with unrelated constants, so its
// iota values are offset from the C ids; assert relations, not absolutes.
func TestThemeCatalogIncludesPlan9XfceSweet(t *testing.T) {
	if ThemeCount != ThemeSweet+1 || THEME_COUNT != 15 {
		t.Fatalf("ThemeCount = %d, THEME_COUNT = %d, want Sweet+1 / 15", ThemeCount, THEME_COUNT)
	}
	if ThemeXfce != ThemeSweet-1 || ThemePlan9 != ThemeXfce-1 {
		t.Fatalf("theme ids out of order: Plan9=%d Xfce=%d Sweet=%d", ThemePlan9, ThemeXfce, ThemeSweet)
	}
	if THEME_PLAN9 != 12 || THEME_XFCE != 13 || THEME_SWEET != 14 {
		t.Fatalf("C-parity aliases: PLAN9=%d XFCE=%d SWEET=%d", THEME_PLAN9, THEME_XFCE, THEME_SWEET)
	}
	if len(catalogLight) != int(ThemeCount) {
		t.Fatalf("catalogLight has %d entries, want %d", len(catalogLight), ThemeCount)
	}
	if len(catalogDark) != int(ThemeCount) {
		t.Fatalf("catalogDark has %d entries, want %d", len(catalogDark), ThemeCount)
	}
}

func TestNormalizeThemeKeepsNewPalettes(t *testing.T) {
	for _, id := range []ThemeId{ThemePlan9, ThemeXfce, ThemeSweet} {
		if normalizeTheme(int32(id)) != id {
			t.Fatalf("normalizeTheme(%d) = %d, want %d", id, normalizeTheme(int32(id)), id)
		}
	}
	if normalizeTheme(int32(ThemeCount)) != ThemeMono {
		t.Fatal("normalizeTheme(ThemeCount) should clamp to ThemeMono")
	}
	if normalizeTheme(-1) != ThemeMono {
		t.Fatal("normalizeTheme(-1) should clamp to ThemeMono")
	}
}

func TestSweetPalettesMatchCCatalog(t *testing.T) {
	dark := themeCatalogPalette(ThemeSweet, true)
	if dark.background != (Color{0x16, 0x19, 0x25, 0xFF}) {
		t.Fatalf("Sweet dark background = %#v", dark.background)
	}
	if dark.surface != (Color{0x18, 0x1B, 0x28, 0xFF}) {
		t.Fatalf("Sweet dark surface = %#v", dark.surface)
	}
	if dark.text != (Color{0xC3, 0xC7, 0xD1, 0xFF}) {
		t.Fatalf("Sweet dark text = %#v", dark.text)
	}
	if dark.circle != (Color{0xC5, 0x0E, 0xD2, 0xFF}) {
		t.Fatalf("Sweet dark circle = %#v", dark.circle)
	}
	if dark.link != (Color{0xE3, 0x23, 0xF0, 0xFF}) {
		t.Fatalf("Sweet dark link = %#v", dark.link)
	}

	light := themeCatalogPalette(ThemeSweet, false)
	if light.background != (Color{0xEE, 0xEE, 0xEE, 0xFF}) {
		t.Fatalf("Sweet light background = %#v", light.background)
	}
	if light.circle != (Color{0xC5, 0x0E, 0xD2, 0xFF}) {
		t.Fatalf("Sweet light circle = %#v", light.circle)
	}
}

func TestPlan9AndXfcePalettesMatchCCatalog(t *testing.T) {
	plan9 := themeCatalogPalette(ThemePlan9, false)
	if plan9.background != (Color{0xFF, 0xFF, 0xEA, 0xFF}) || plan9.link != (Color{0x00, 0x00, 0xAA, 0xFF}) {
		t.Fatalf("Plan9 light palette mismatch: %#v", plan9)
	}
	xfce := themeCatalogPalette(ThemeXfce, true)
	if xfce.background != (Color{0x13, 0x17, 0x22, 0xFF}) || xfce.link != (Color{0x58, 0xC7, 0xE0, 0xFF}) {
		t.Fatalf("Xfce dark palette mismatch: %#v", xfce)
	}
}

func TestMaterialSchemeMirrorsCDerivation(t *testing.T) {
	s := materialScheme(themeCatalogPalette(ThemeSweet, true), true)
	if s.Primary != (Color{0xC5, 0x0E, 0xD2, 0xFF}) {
		t.Fatalf("Primary = %#v, want the Sweet circle", s.Primary)
	}
	if s.OnPrimary != (Color{0xFF, 0xFF, 0xFF, 0xFF}) {
		t.Fatalf("OnPrimary = %#v, want white on the magenta accent", s.OnPrimary)
	}
	if s.Surface != (Color{0x18, 0x1B, 0x28, 0xFF}) {
		t.Fatalf("Surface = %#v, want the Sweet dark surface", s.Surface)
	}
	if s.SurfaceContainer != (Color{0x20, 0x23, 0x2F, 0xFF}) {
		t.Fatalf("SurfaceContainer = %#v, want bg + dark tone 10", s.SurfaceContainer)
	}
	if s.OnSurfaceVariant != (Color{0xDF, 0xE3, 0xED, 0xFF}) {
		t.Fatalf("OnSurfaceVariant = %#v, want text + dark tone 28", s.OnSurfaceVariant)
	}
	if s.Outline != (Color{0x40, 0x43, 0x4F, 0xFF}) {
		t.Fatalf("Outline = %#v, want bg + dark tone 42", s.Outline)
	}
	if s.Error != (Color{0xF2, 0xB8, 0xB5, 0xFF}) {
		t.Fatalf("Error = %#v, want the dark error tone", s.Error)
	}
	if s.OnError != (Color{0x1D, 0x1B, 0x20, 0xFF}) {
		t.Fatalf("OnError = %#v, want dark content on the light error tone", s.OnError)
	}
	if s.DisabledContainer.A != 96 || s.DisabledContent.A != 96 {
		t.Fatalf("disabled roles should carry alpha 96: %#v %#v", s.DisabledContainer, s.DisabledContent)
	}

	light := materialScheme(themeCatalogPalette(ThemeSweet, false), false)
	if light.Error != (Color{0xBA, 0x1A, 0x1A, 0xFF}) {
		t.Fatalf("light Error = %#v, want the light error tone", light.Error)
	}
	if light.SurfaceContainer != (Color{0xEA, 0xEA, 0xEA, 0xFF}) {
		t.Fatalf("light SurfaceContainer = %#v, want bg - light tone 4", light.SurfaceContainer)
	}
}

func TestDefaultThemeForThemeStylePairsPaletteWithStyle(t *testing.T) {
	if DefaultThemeForThemeStyle(ThemeStyleMaterial) != ThemeSweet {
		t.Fatal("Material style should pair with the Sweet palette")
	}
	if DefaultThemeForThemeStyle(ThemeStyleRetro) != ThemeMono {
		t.Fatal("Retro style should pair with the Mono palette")
	}
	if DefaultThemeForThemeStyle(ThemeStyleSystem) != ThemeMono {
		t.Fatal("System style should fall back to the Mono palette")
	}
}

func TestRuntimeAppliesSweetThemeAndScheme(t *testing.T) {
	rt := New(AppConfig{Width: 100, Height: 100}).(*runtime)
	rt.SetThemeSource(ThemeSourceApp)
	rt.SetThemeMode(ThemeModeDark)
	rt.SetCurrentTheme(int32(ThemeSweet), 1)

	if got := rt.GetThemeBackground(); got != (Color{0x16, 0x19, 0x25, 0xFF}) {
		t.Fatalf("GetThemeBackground = %#v, want Sweet dark background", got)
	}
	scheme := rt.GetUIMaterialScheme()
	if scheme.Primary != (Color{0xC5, 0x0E, 0xD2, 0xFF}) {
		t.Fatalf("scheme.Primary = %#v, want the Sweet accent", scheme.Primary)
	}
	if themeSettingsThemeLabel(int32(ThemeSweet)) != "Sweet" {
		t.Fatalf("picker label for Sweet = %q", themeSettingsThemeLabel(int32(ThemeSweet)))
	}
}

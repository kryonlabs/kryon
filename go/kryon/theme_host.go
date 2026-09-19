package kryon

func (r *runtime) GetThemeBackground() Color { return r.theme().background }

func (r *runtime) GetThemeText() Color { return r.theme().text }

func (r *runtime) GetThemeIcon() Color { return r.theme().icon }

func (r *runtime) GetThemeSurface() Color { return r.theme().surface }

func (r *runtime) GetThemeBorder() Color { return r.theme().border }

func (r *runtime) GetThemeButton() Color { return r.theme().button }

func (r *runtime) GetThemeButtonHover() Color { return r.theme().buttonHover }

func (r *runtime) GetThemeLink() Color { return r.theme().link }

// The primary trio mirrors the Default mapping in theme_runtime.go: the
// palette's circle color is Primary, its contrast color OnPrimary, and a
// background tone serves as SurfaceVariant.
func (r *runtime) GetThemePrimary() Color { return r.theme().circle }

func (r *runtime) GetThemeOnPrimary() Color { return materialOnColor(r.theme().circle) }

func (r *runtime) GetThemeSurfaceVariant() Color {
	return materialTone(r.theme().background, 10, 18, r.effectiveDark())
}

func (r *runtime) GetThemeScheme() DefaultScheme {
	return materialScheme(r.theme(), r.effectiveDark())
}

func (r *runtime) SetCurrentTheme(themeID int32, darkMode int32) {
	r.defaultTheme = false
	r.activeTheme = nil
	r.activeThemeFamily = nil
	r.currentThemeID = normalizeTheme(themeID)
	if darkMode != 0 {
		r.themeMode = ThemeModeDark
	} else {
		r.themeMode = ThemeModeLight
	}
}

func (r *runtime) SetTheme(theme Theme) {
	r.defaultTheme = false
	copy := theme
	r.activeThemeFamily = nil
	r.activeTheme = &copy
	r.themeMode = theme.Mode
}

func (r *runtime) SetThemeFamily(family ThemeFamily) {
	r.defaultTheme = false
	copy := family
	copy.Light.Mode = ThemeModeLight
	copy.Dark.Mode = ThemeModeDark
	r.activeThemeFamily = &copy
	r.applyThemeFamily()
}

func (r *runtime) GetThemeFamily() ThemeFamily {
	if r.activeThemeFamily == nil {
		return ThemeFamily{}
	}
	return *r.activeThemeFamily
}

func (r *runtime) applyThemeFamily() {
	if r.activeThemeFamily == nil {
		return
	}
	selected := r.activeThemeFamily.Light
	if r.effectiveDark() {
		selected = r.activeThemeFamily.Dark
	}
	r.activeTheme = &selected
}

func (r *runtime) GetTheme() Theme {
	if r.activeTheme != nil {
		return *r.activeTheme
	}
	if r.effectiveDark() {
		return ThemeDefaultDark()
	}
	return ThemeDefaultLight()
}

func (r *runtime) SetThemeSource(source ThemeSource) {
	if r.defaultTheme {
		r.defaultTheme = false
		r.activeTheme = nil
		r.activeThemeFamily = nil
	}
	if source != ThemeSourceSystem {
		source = ThemeSourceApp
	}
	r.themeSource = source
}

func (r *runtime) SetThemeMode(mode ThemeMode) {
	if mode < ThemeModeSystem || mode > ThemeModeDark {
		mode = ThemeModeSystem
	}
	r.themeMode = mode
	r.applyThemeFamily()
}

func (r *runtime) GetThemeMode() ThemeMode {
	return r.themeMode
}

func themeLabel(id int32) string {
	switch normalizeTheme(id) {
	case ThemeSky:
		return "Sky"
	case ThemeOcean:
		return "Ocean"
	case ThemeForest:
		return "Forest"
	case ThemeSunset:
		return "Sunset"
	case ThemeLavender:
		return "Lavender"
	case ThemeCherry:
		return "Cherry"
	case ThemeDawn:
		return "Dawn"
	case ThemeSage:
		return "Sage"
	case ThemeInk:
		return "Ink"
	case ThemeMint:
		return "Mint"
	case ThemeCobalt:
		return "Cobalt"
	case ThemePlan9:
		return "Plan9"
	case ThemeXfce:
		return "Xfce"
	case ThemeSweet:
		return "Sweet"
	default:
		return "Mono"
	}
}

func (r *runtime) theme() themePalette {
	if r.activeTheme != nil {
		colors := r.activeTheme.Colors
		return themeSelectionDefaults(themePalette{
			background:   colors.Background,
			surface:      colors.Surface,
			text:         colors.Text,
			circle:       colors.Accent,
			button:       colors.Accent,
			buttonHover:  colors.AccentHover,
			icon:         colors.Icon,
			link:         colors.Link,
			linkHover:    colors.LinkHover,
			textDisabled: colors.DisabledText,
			border:       colors.Border,
			focus:        colors.Focus,
			selected:     colors.Selection,
		})
	}
	dark := r.effectiveDark()
	if r.themeSource == ThemeSourceSystem {
		if palette, ok := currentSystemTheme(dark); ok {
			return palette
		}
	}
	return themeCatalogPalette(r.currentThemeID, dark)
}

func (r *runtime) effectiveDark() bool {
	return Theme_ResolveDark(ThemePolicy(r.themeMode), systemPrefersDark())
}

func systemPrefersDark() bool {
	return systemThemePrefersDark()
}

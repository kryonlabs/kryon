package kryon

import (
	"context"
	"io"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
)

const systemThemeRetry = 30 * time.Second

type systemThemePalette struct {
	themePalette
	name         string
	available    bool
	prefersDark  bool
	supportsMode bool
}

var (
	systemThemeMu          sync.Mutex
	systemThemePaletteNow  = defaultSystemPalette()
	systemThemeLastAttempt time.Time
	systemThemeRefreshes   int64
)

var catalogLight = [...]themePalette{
	ThemeSky:      catalogPalette(0xE2, 0xEE, 0xFC, 0xD4, 0xE4, 0xF5, 0x24, 0x48, 0x7C, 0x7E, 0xB7, 0xE6, 0xA6, 0xCF, 0xF2, 0x68, 0x9E, 0xD7, 0xE2, 0xEE, 0xFC, 0x4A, 0x90, 0xE2),
	ThemeOcean:    catalogPalette(0xD0, 0xE8, 0xF8, 0xC0, 0xDD, 0xEE, 0x1A, 0x40, 0x70, 0x5A, 0xA0, 0xD0, 0x80, 0xC0, 0xE0, 0x40, 0x90, 0xD0, 0xD0, 0xE8, 0xF8, 0x20, 0x80, 0xC0),
	ThemeForest:   catalogPalette(0xE0, 0xF0, 0xE0, 0xD1, 0xE5, 0xD1, 0x2A, 0x50, 0x30, 0x60, 0xB0, 0x70, 0xA0, 0xD0, 0xB0, 0x70, 0xC0, 0x90, 0xE0, 0xF0, 0xE0, 0x40, 0x90, 0x50),
	ThemeSunset:   catalogPalette(0xF8, 0xE8, 0xD8, 0xEC, 0xD8, 0xC6, 0x50, 0x28, 0x14, 0xD0, 0x80, 0x50, 0xF0, 0xC0, 0xA0, 0xE0, 0x90, 0x60, 0xF8, 0xE8, 0xD8, 0xC0, 0x60, 0x30),
	ThemeLavender: catalogPalette(0xF0, 0xE8, 0xF8, 0xE2, 0xD7, 0xEE, 0x40, 0x28, 0x60, 0x90, 0x70, 0xB0, 0xC0, 0xA0, 0xD0, 0xA0, 0x80, 0xC0, 0xF0, 0xE8, 0xF8, 0x70, 0x50, 0x90),
	ThemeCherry:   catalogPalette(0xF8, 0xD8, 0xE0, 0xEC, 0xC9, 0xD2, 0x60, 0x20, 0x30, 0xD0, 0x60, 0x80, 0xF0, 0xA0, 0xB0, 0xE0, 0x70, 0x90, 0xF8, 0xD8, 0xE0, 0xC0, 0x40, 0x60),
	ThemeDawn:     catalogPalette(0xF5, 0xE9, 0xDF, 0xE7, 0xD9, 0xD0, 0x39, 0x3B, 0x4A, 0xE0, 0x7A, 0x6D, 0x8F, 0xCF, 0xC6, 0x62, 0xB8, 0xB0, 0xF5, 0xE9, 0xDF, 0xC8, 0x62, 0x5C),
	ThemeSage:     catalogPalette(0xE8, 0xEC, 0xDF, 0xD9, 0xE0, 0xD0, 0x35, 0x44, 0x38, 0x82, 0xA0, 0x7D, 0xB6, 0xC9, 0xA6, 0x94, 0xAF, 0x84, 0xE8, 0xEC, 0xDF, 0x5D, 0x82, 0x68),
	ThemeInk:      catalogPalette(0xF3, 0xE4, 0xC8, 0xE4, 0xCF, 0xAA, 0x3F, 0x2B, 0x1C, 0xB8, 0x76, 0x3F, 0xD8, 0xB0, 0x72, 0xC7, 0x91, 0x51, 0xF8, 0xEE, 0xD6, 0x8F, 0x5B, 0x2F),
	ThemeMono:     catalogPalette(0xC0, 0xC0, 0xC0, 0xD4, 0xD0, 0xC8, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0xC0, 0xC0, 0xC0, 0xE8, 0xE8, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80),
	ThemeMint:     catalogPalette(0xE4, 0xF4, 0xEE, 0xD2, 0xE7, 0xDF, 0x1E, 0x45, 0x3A, 0x4B, 0xB6, 0x9A, 0x98, 0xD8, 0xC6, 0x70, 0xC7, 0xB2, 0xE4, 0xF4, 0xEE, 0x2D, 0x8F, 0x78),
	ThemeCobalt:   catalogPalette(0xE7, 0xEA, 0xF4, 0xD7, 0xDD, 0xEC, 0x19, 0x27, 0x4A, 0x4F, 0x67, 0xC8, 0xA9, 0xB8, 0xEF, 0x7D, 0x92, 0xE0, 0xE7, 0xEA, 0xF4, 0x35, 0x4F, 0xB5),
}

var catalogDark = [...]themePalette{
	ThemeSky:      catalogPalette(0x18, 0x28, 0x38, 0x22, 0x36, 0x48, 0xB0, 0xD0, 0xEA, 0x50, 0x80, 0xB0, 0x30, 0x50, 0x70, 0x40, 0x70, 0x90, 0xB0, 0xD0, 0xEA, 0x60, 0xA0, 0xD0),
	ThemeOcean:    catalogPalette(0x10, 0x25, 0x40, 0x1C, 0x34, 0x50, 0x98, 0xC0, 0xDE, 0x30, 0x60, 0x90, 0x20, 0x40, 0x60, 0x30, 0x55, 0x75, 0x98, 0xC0, 0xDE, 0x50, 0x90, 0xC0),
	ThemeForest:   catalogPalette(0x15, 0x30, 0x20, 0x20, 0x3C, 0x2A, 0xB4, 0xD0, 0xB4, 0x35, 0x65, 0x45, 0x25, 0x45, 0x30, 0x30, 0x55, 0x40, 0xB4, 0xD0, 0xB4, 0x50, 0x80, 0x60),
	ThemeSunset:   catalogPalette(0x30, 0x18, 0x10, 0x3E, 0x24, 0x18, 0xE0, 0xB0, 0x90, 0x80, 0x40, 0x28, 0x50, 0x28, 0x18, 0x60, 0x35, 0x25, 0xE0, 0xB0, 0x90, 0xC0, 0x50, 0x30),
	ThemeLavender: catalogPalette(0x20, 0x15, 0x30, 0x2E, 0x20, 0x42, 0xC4, 0xA8, 0xD0, 0x50, 0x35, 0x70, 0x35, 0x20, 0x50, 0x45, 0x30, 0x65, 0xC4, 0xA8, 0xD0, 0x90, 0x60, 0xB0),
	ThemeCherry:   catalogPalette(0x30, 0x15, 0x20, 0x40, 0x20, 0x2A, 0xD2, 0x98, 0xA8, 0x70, 0x30, 0x45, 0x45, 0x20, 0x28, 0x55, 0x30, 0x38, 0xD2, 0x98, 0xA8, 0xB0, 0x40, 0x60),
	ThemeDawn:     catalogPalette(0x27, 0x22, 0x2C, 0x35, 0x2D, 0x38, 0xE9, 0xC7, 0xB7, 0xB8, 0x5F, 0x60, 0x3A, 0x6B, 0x70, 0x4D, 0x83, 0x85, 0xE9, 0xC7, 0xB7, 0xD0, 0x72, 0x67),
	ThemeSage:     catalogPalette(0x1F, 0x2A, 0x22, 0x2D, 0x3A, 0x30, 0xC7, 0xD4, 0xC0, 0x5F, 0x81, 0x65, 0x3C, 0x54, 0x40, 0x4E, 0x66, 0x50, 0xC7, 0xD4, 0xC0, 0x8B, 0xA0, 0x70),
	ThemeInk:      catalogPalette(0x25, 0x1A, 0x12, 0x33, 0x24, 0x18, 0xF1, 0xD8, 0xB0, 0xA3, 0x65, 0x32, 0x5A, 0x3C, 0x24, 0x72, 0x4B, 0x2C, 0xF1, 0xD8, 0xB0, 0xD5, 0x91, 0x52),
	ThemeMono:     catalogPalette(0x20, 0x20, 0x20, 0x30, 0x30, 0x30, 0xF0, 0xF0, 0xF0, 0x80, 0x80, 0x80, 0x40, 0x40, 0x40, 0x58, 0x58, 0x58, 0xF0, 0xF0, 0xF0, 0x80, 0xA0, 0xFF),
	ThemeMint:     catalogPalette(0x12, 0x2D, 0x28, 0x1B, 0x3B, 0x34, 0xB9, 0xE4, 0xD5, 0x2E, 0x82, 0x72, 0x25, 0x55, 0x4B, 0x32, 0x69, 0x5E, 0xB9, 0xE4, 0xD5, 0x6E, 0xC8, 0xB0),
	ThemeCobalt:   catalogPalette(0x12, 0x18, 0x33, 0x1B, 0x24, 0x45, 0xC2, 0xCC, 0xF5, 0x38, 0x4C, 0xA8, 0x27, 0x35, 0x73, 0x34, 0x45, 0x8B, 0xC2, 0xCC, 0xF5, 0x8E, 0xA0, 0xF0),
}

func catalogPalette(bgR, bgG, bgB, surfaceR, surfaceG, surfaceB, textR, textG, textB, circleR, circleG, circleB, buttonR, buttonG, buttonB, hoverR, hoverG, hoverB, iconR, iconG, iconB, linkR, linkG, linkB uint8) themePalette {
	p := themePalette{
		background:  Color{bgR, bgG, bgB, 255},
		surface:     Color{surfaceR, surfaceG, surfaceB, 255},
		text:        Color{textR, textG, textB, 255},
		button:      Color{buttonR, buttonG, buttonB, 255},
		buttonHover: Color{hoverR, hoverG, hoverB, 255},
		icon:        Color{iconR, iconG, iconB, 255},
		link:        Color{linkR, linkG, linkB, 255},
	}
	return completeThemePalette(p)
}

func themeCatalogPalette(id ThemeId, dark bool) themePalette {
	id = normalizeTheme(int32(id))
	if dark {
		return catalogDark[id]
	}
	return catalogLight[id]
}

func normalizeTheme(id int32) ThemeId {
	if id < 0 || id >= int32(ThemeCount) {
		return ThemeMono
	}
	return ThemeId(id)
}

func currentSystemTheme(dark bool) (themePalette, bool) {
	p := refreshSystemTheme(false)
	if !p.available {
		return themePalette{}, false
	}
	if p.supportsMode && p.prefersDark != dark {
		p = materialSystemPalette(dark)
	}
	return p.themePalette, true
}

func systemThemePrefersDark() bool {
	mode := strings.ToLower(strings.TrimSpace(os.Getenv("KRYON_THEME_MODE")))
	if mode == "light" {
		return false
	}
	if mode == "dark" {
		return true
	}
	return refreshSystemTheme(false).prefersDark
}

func refreshSystemTheme(force bool) systemThemePalette {
	systemThemeMu.Lock()
	defer systemThemeMu.Unlock()

	now := time.Now()
	if !force && !systemThemeLastAttempt.IsZero() && now.Sub(systemThemeLastAttempt) < systemThemeRetry {
		return systemThemePaletteNow
	}
	systemThemeLastAttempt = now
	systemThemeRefreshes++

	if palette, ok := gtkCSSPalette(); ok {
		systemThemePaletteNow = palette
		return systemThemePaletteNow
	}
	if dark, ok := explicitDarkPreference(); ok {
		systemThemePaletteNow = materialSystemPalette(dark)
		return systemThemePaletteNow
	}
	systemThemePaletteNow = defaultSystemPalette()
	return systemThemePaletteNow
}

func defaultSystemPalette() systemThemePalette {
	p := themePalette{
		background:  Color{0xF0, 0xF0, 0xF0, 0xFF},
		surface:     Color{0xE8, 0xE8, 0xE8, 0xFF},
		text:        Color{0x10, 0x10, 0x10, 0xFF},
		button:      Color{0xDD, 0xDD, 0xDD, 0xFF},
		buttonHover: Color{0xC8, 0xD8, 0xEA, 0xFF},
		icon:        Color{0x10, 0x10, 0x10, 0xFF},
		link:        Color{0x20, 0x70, 0xC0, 0xFF},
	}
	p = completeThemePalette(p)
	return systemThemePalette{themePalette: p, name: "System"}
}

func materialSystemPalette(dark bool) systemThemePalette {
	var p themePalette
	if dark {
		p = themePalette{
			background:  Color{0x14, 0x12, 0x18, 0xFF},
			surface:     Color{0x21, 0x1F, 0x26, 0xFF},
			text:        Color{0xE6, 0xE0, 0xE9, 0xFF},
			button:      Color{0x4A, 0x44, 0x58, 0xFF},
			buttonHover: Color{0x67, 0x50, 0xA4, 0xFF},
			icon:        Color{0xE6, 0xE0, 0xE9, 0xFF},
			link:        Color{0xD0, 0xBC, 0xFF, 0xFF},
		}
	} else {
		p = themePalette{
			background:  Color{0xFF, 0xFB, 0xFE, 0xFF},
			surface:     Color{0xF7, 0xF2, 0xFA, 0xFF},
			text:        Color{0x1D, 0x1B, 0x20, 0xFF},
			button:      Color{0xE7, 0xE0, 0xEC, 0xFF},
			buttonHover: Color{0xD0, 0xBC, 0xFF, 0xFF},
			icon:        Color{0x1D, 0x1B, 0x20, 0xFF},
			link:        Color{0x67, 0x50, 0xA4, 0xFF},
		}
	}
	p = completeThemePalette(p)
	return systemThemePalette{themePalette: p, name: "Material", available: true, prefersDark: dark, supportsMode: true}
}

func gtkCSSPalette() (systemThemePalette, bool) {
	theme, variant, ok := gtkCSSThemeName()
	if !ok {
		return systemThemePalette{}, false
	}
	path, ok := gtkCSSThemeFile(theme, variant)
	if !ok {
		return systemThemePalette{}, false
	}
	text, ok := readTextFile(path, 1<<20)
	if !ok {
		return systemThemePalette{}, false
	}

	base, haveBase := cssDefineColor(text, "theme_base_color")
	if !haveBase {
		base, haveBase = cssDefineColor(text, "base_color")
	}
	bg, haveBG := cssDefineColor(text, "theme_bg_color")
	if !haveBG {
		bg, haveBG = cssDefineColor(text, "bg_color")
	}
	fg, haveFG := cssDefineColor(text, "theme_fg_color")
	if !haveFG {
		fg, haveFG = cssDefineColor(text, "fg_color")
	}
	selected, haveSelected := cssDefineColor(text, "theme_selected_bg_color")
	if !haveSelected {
		selected, haveSelected = cssDefineColor(text, "selected_bg_color")
	}
	selectedText, haveSelectedText := cssDefineColor(text, "theme_selected_fg_color")
	if !haveSelectedText {
		selectedText, haveSelectedText = cssDefineColor(text, "selected_fg_color")
	}
	border, haveBorder := cssDefineColor(text, "borders")
	if !haveBase && !haveBG {
		return systemThemePalette{}, false
	}

	p := defaultSystemPalette()
	if haveBase {
		p.background = base
	} else {
		p.background = bg
	}
	if haveBG {
		p.surface = bg
	} else {
		p.surface = p.background
	}
	if haveFG {
		p.text = fg
	} else {
		p.text = Color{0x10, 0x10, 0x10, 0xFF}
	}
	p.button = p.surface
	p.buttonHover = lightenColor(p.button, 18)
	if haveSelected {
		p.link = selected
		p.focus = selected
	} else {
		p.link = p.buttonHover
		p.focus = p.link
	}
	if haveSelectedText {
		p.selectedText = selectedText
	}
	if haveBorder {
		p.border = border
	}
	p.icon = p.text
	p = systemThemePalette{
		themePalette: completeThemePalette(p.themePalette),
		name:         theme,
		available:    true,
		prefersDark:  luminance(p.text) > luminance(p.background),
	}
	return p, true
}

func gtkCSSThemeName() (theme, variant string, ok bool) {
	if env := strings.TrimSpace(os.Getenv("GTK_THEME")); env != "" {
		parts := strings.SplitN(env, ":", 2)
		theme = strings.TrimSpace(parts[0])
		if len(parts) > 1 {
			variant = strings.TrimSpace(parts[1])
		}
		return theme, variant, theme != ""
	}
	for _, path := range []string{xsettingsConfigPath(), gtkSettingsConfigPath("gtk-3.0"), gtkSettingsConfigPath("gtk-4.0")} {
		if path == "" {
			continue
		}
		text, ok := readTextFile(path, 64*1024)
		if !ok {
			continue
		}
		if strings.HasSuffix(path, "xsettings.xml") {
			if value, ok := xmlPropertyValue(text, "ThemeName"); ok {
				return value, "", true
			}
			continue
		}
		if value, ok := settingsINIValue(text, "gtk-theme-name"); ok {
			return value, "", true
		}
	}
	if value, ok := xfconfQueryValue("/Net/ThemeName"); ok {
		return value, "", true
	}
	return "", "", false
}

func gtkCSSThemeFile(theme, variant string) (string, bool) {
	names := []string{"gtk.css"}
	if strings.EqualFold(variant, "dark") || strings.Contains(strings.ToLower(theme), "dark") {
		names = []string{"gtk-dark.css", "gtk.css"}
	}
	for _, root := range themeRoots() {
		for _, name := range names {
			path := filepath.Join(root, theme, "gtk-3.0", name)
			if fileExists(path) {
				return path, true
			}
		}
	}
	return "", false
}

func themeRoots() []string {
	var roots []string
	if home := os.Getenv("HOME"); home != "" {
		roots = append(roots, filepath.Join(home, ".themes"))
	}
	if xdg := os.Getenv("XDG_DATA_HOME"); xdg != "" {
		roots = append(roots, filepath.Join(xdg, "themes"))
	}
	roots = append(roots, "/usr/local/share/themes", "/usr/share/themes")
	return roots
}

func explicitDarkPreference() (bool, bool) {
	for _, value := range []string{
		os.Getenv("KRYON_THEME_MODE"),
		os.Getenv("GTK_THEME"),
		os.Getenv("QT_STYLE_OVERRIDE"),
		os.Getenv("COLOR_SCHEME"),
		os.Getenv("XDG_CURRENT_DESKTOP"),
	} {
		text := strings.ToLower(value)
		if strings.Contains(text, "dark") {
			return true, true
		}
		if strings.Contains(text, "light") {
			return false, true
		}
	}
	for _, path := range []string{gtkSettingsConfigPath("gtk-3.0"), gtkSettingsConfigPath("gtk-4.0"), xsettingsConfigPath()} {
		if path == "" {
			continue
		}
		text, ok := readTextFile(path, 64*1024)
		if !ok {
			continue
		}
		lower := strings.ToLower(text)
		if strings.Contains(lower, "gtk-application-prefer-dark-theme=true") {
			return true, true
		}
		if strings.Contains(lower, "dark") {
			return true, true
		}
	}
	return false, false
}

func xsettingsConfigPath() string {
	if config := os.Getenv("XDG_CONFIG_HOME"); config != "" {
		return filepath.Join(config, "xfce4", "xfconf", "xfce-perchannel-xml", "xsettings.xml")
	}
	if home := os.Getenv("HOME"); home != "" {
		return filepath.Join(home, ".config", "xfce4", "xfconf", "xfce-perchannel-xml", "xsettings.xml")
	}
	return ""
}

func gtkSettingsConfigPath(version string) string {
	if config := os.Getenv("XDG_CONFIG_HOME"); config != "" {
		return filepath.Join(config, version, "settings.ini")
	}
	if home := os.Getenv("HOME"); home != "" {
		return filepath.Join(home, ".config", version, "settings.ini")
	}
	return ""
}

func xmlPropertyValue(text, name string) (string, bool) {
	pattern := `name="` + name + `"`
	cursor := strings.Index(text, pattern)
	if cursor < 0 {
		return "", false
	}
	tagEnd := strings.IndexByte(text[cursor:], '>')
	if tagEnd < 0 {
		return "", false
	}
	tag := text[cursor : cursor+tagEnd]
	value := strings.Index(tag, `value="`)
	if value < 0 {
		return "", false
	}
	start := value + len(`value="`)
	end := strings.IndexByte(tag[start:], '"')
	if end < 0 {
		return "", false
	}
	out := strings.TrimSpace(tag[start : start+end])
	return out, out != ""
}

func settingsINIValue(text, key string) (string, bool) {
	for _, line := range strings.Split(text, "\n") {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "#") || !strings.HasPrefix(line, key) {
			continue
		}
		parts := strings.SplitN(line, "=", 2)
		if len(parts) != 2 {
			continue
		}
		value := strings.Trim(strings.TrimSpace(parts[1]), `"`)
		return value, value != ""
	}
	return "", false
}

func xfconfQueryValue(property string) (string, bool) {
	ctx, cancel := context.WithTimeout(context.Background(), 300*time.Millisecond)
	defer cancel()
	out, err := exec.CommandContext(ctx, "xfconf-query", "-c", "xsettings", "-p", property).Output()
	if err != nil {
		return "", false
	}
	value := strings.TrimSpace(string(out))
	return value, value != ""
}

func cssDefineColor(text, name string) (Color, bool) {
	pattern := "@define-color " + name
	offset := 0
	for {
		idx := strings.Index(text[offset:], pattern)
		if idx < 0 {
			return Color{}, false
		}
		start := offset + idx + len(pattern)
		if start < len(text) && isCSSIdent(text[start]) {
			offset = start
			continue
		}
		end := strings.IndexByte(text[start:], ';')
		if end < 0 {
			return Color{}, false
		}
		value := strings.TrimSpace(text[start : start+end])
		if color, ok := cssParseColor(value); ok {
			return color, true
		}
		offset = start + end + 1
	}
}

func isCSSIdent(b byte) bool {
	return b == '_' || b == '-' || (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9')
}

func cssParseColor(value string) (Color, bool) {
	value = strings.TrimSpace(value)
	if strings.HasPrefix(value, "#") {
		return cssParseHex(value)
	}
	if strings.HasPrefix(value, "rgb(") || strings.HasPrefix(value, "rgba(") {
		return cssParseRGBFunc(value)
	}
	if color, ok := cssNamedColor(strings.ToLower(value)); ok {
		return color, true
	}
	return cssParseUnitTriplet(value)
}

func cssParseHex(value string) (Color, bool) {
	hex := strings.TrimPrefix(value, "#")
	if len(hex) >= 3 {
		hex = hex[:minInt(len(hex), 8)]
	}
	switch len(hex) {
	case 3:
		r, okR := cssHexByte(hex[0], hex[0])
		g, okG := cssHexByte(hex[1], hex[1])
		b, okB := cssHexByte(hex[2], hex[2])
		return Color{r, g, b, 255}, okR && okG && okB
	case 6, 8:
		r, okR := cssHexByte(hex[0], hex[1])
		g, okG := cssHexByte(hex[2], hex[3])
		b, okB := cssHexByte(hex[4], hex[5])
		a := uint8(255)
		okA := true
		if len(hex) == 8 {
			a, okA = cssHexByte(hex[6], hex[7])
		}
		return Color{r, g, b, a}, okR && okG && okB && okA
	}
	return Color{}, false
}

func cssHexByte(a, b byte) (uint8, bool) {
	hi, okHi := cssHexNibble(a)
	lo, okLo := cssHexNibble(b)
	return uint8(hi<<4 | lo), okHi && okLo
}

func cssHexNibble(b byte) (int, bool) {
	switch {
	case b >= '0' && b <= '9':
		return int(b - '0'), true
	case b >= 'a' && b <= 'f':
		return int(b-'a') + 10, true
	case b >= 'A' && b <= 'F':
		return int(b-'A') + 10, true
	}
	return 0, false
}

func cssParseUnitTriplet(value string) (Color, bool) {
	fields := strings.Fields(value)
	if len(fields) < 3 {
		return Color{}, false
	}
	var c [3]uint8
	for i := 0; i < 3; i++ {
		f, err := strconv.ParseFloat(fields[i], 64)
		if err != nil {
			return Color{}, false
		}
		c[i] = unitToByte(f)
	}
	return Color{c[0], c[1], c[2], 255}, true
}

func cssParseRGBFunc(value string) (Color, bool) {
	start := strings.IndexByte(value, '(')
	end := strings.LastIndexByte(value, ')')
	if start < 0 || end <= start {
		return Color{}, false
	}
	body := strings.NewReplacer(",", " ", "/", " ").Replace(value[start+1 : end])
	fields := strings.Fields(body)
	if len(fields) < 3 {
		return Color{}, false
	}
	var c [3]uint8
	for i := 0; i < 3; i++ {
		raw := strings.TrimSuffix(fields[i], "%")
		f, err := strconv.ParseFloat(raw, 64)
		if err != nil {
			return Color{}, false
		}
		if strings.HasSuffix(fields[i], "%") {
			c[i] = uint8(math.Round(clampFloat(f/100, 0, 1) * 255))
		} else {
			c[i] = uint8(math.Round(clampFloat(f, 0, 255)))
		}
	}
	return Color{c[0], c[1], c[2], 255}, true
}

func cssNamedColor(name string) (Color, bool) {
	named := map[string]Color{
		"black":   {0x00, 0x00, 0x00, 0xFF},
		"white":   {0xFF, 0xFF, 0xFF, 0xFF},
		"gray":    {0x80, 0x80, 0x80, 0xFF},
		"grey":    {0x80, 0x80, 0x80, 0xFF},
		"silver":  {0xC0, 0xC0, 0xC0, 0xFF},
		"navy":    {0x00, 0x00, 0x80, 0xFF},
		"red":     {0xFF, 0x00, 0x00, 0xFF},
		"green":   {0x00, 0x80, 0x00, 0xFF},
		"blue":    {0x00, 0x00, 0xFF, 0xFF},
		"yellow":  {0xFF, 0xFF, 0x00, 0xFF},
		"orange":  {0xFF, 0xA5, 0x00, 0xFF},
		"purple":  {0x80, 0x00, 0x80, 0xFF},
		"magenta": {0xFF, 0x00, 0xFF, 0xFF},
		"cyan":    {0x00, 0xFF, 0xFF, 0xFF},
	}
	color, ok := named[name]
	return color, ok
}

func readTextFile(path string, limit int64) (string, bool) {
	file, err := os.Open(path)
	if err != nil {
		return "", false
	}
	defer file.Close()
	data, err := io.ReadAll(io.LimitReader(file, limit))
	if err != nil || len(data) == 0 {
		return "", false
	}
	return string(data), true
}

func fileExists(path string) bool {
	info, err := os.Stat(path)
	return err == nil && !info.IsDir()
}

func completeThemePalette(p themePalette) themePalette {
	if p.link.A == 0 {
		p.link = p.buttonHover
	}
	if p.selected.A == 0 {
		p.selected = mixColor(p.surface, p.link, 0.35)
	}
	if p.selectedHot.A == 0 {
		p.selectedHot = mixColor(p.surface, p.link, 0.55)
	}
	if p.selectedText.A == 0 {
		p.selectedText = readableTextOn(p.selectedHot, p.text, p.background)
	}
	if p.border.A == 0 {
		p.border = mixColor(p.surface, p.text, 0.18)
	}
	if p.focus.A == 0 {
		p.focus = p.link
	}
	return p
}

func readableTextOn(surface, primary, alternate Color) Color {
	if colorContrast(surface, primary) >= colorContrast(surface, alternate) {
		return primary
	}
	return alternate
}

func colorContrast(a, b Color) int {
	d := luminance(a) - luminance(b)
	if d < 0 {
		return -d
	}
	return d
}

func lightenColor(c Color, delta int) Color {
	return Color{
		R: clampByte(int(c.R) + delta),
		G: clampByte(int(c.G) + delta),
		B: clampByte(int(c.B) + delta),
		A: c.A,
	}
}

func luminance(c Color) int {
	return (int(c.R)*299 + int(c.G)*587 + int(c.B)*114) / 1000
}

func unitToByte(v float64) uint8 {
	return uint8(math.Round(clampFloat(v, 0, 1) * 255))
}

func clampFloat(v, lo, hi float64) float64 {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}

func clampByte(v int) uint8 {
	if v < 0 {
		return 0
	}
	if v > 255 {
		return 255
	}
	return uint8(v)
}

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func resetSystemThemeForTest() {
	systemThemeMu.Lock()
	defer systemThemeMu.Unlock()
	systemThemePaletteNow = defaultSystemPalette()
	systemThemeLastAttempt = time.Time{}
	systemThemeRefreshes = 0
}

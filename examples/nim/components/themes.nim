## Theme Definitions for Kryon UI Framework
##
## Provides 6 beautiful preset themes with instant switching support

import kryon_dsl

# Theme type - name + 16 packed RGBA colors
type
  ThemePreset* = object
    name*: string
    colors*: Theme  # array[16, uint32] from style_vars

# Pastel Dream - Soft, light, cheerful
# Colors in 0xAARRGGBB format (alpha in high byte)
const pastelTheme* = ThemePreset(
  name: "Pastel Dream",
  colors: [
    0xFFA8D8EA'u32,  # primary - soft blue
    0xFF89C9DE'u32,  # primaryHover
    0xFFAA96DA'u32,  # secondary - lavender
    0xFFFCBAD3'u32,  # accent - pink
    0xFFFFFDD0'u32,  # background - cream
    0xFFFFF5E6'u32,  # surface
    0xFFFFE8CC'u32,  # surfaceHover
    0xFFFFFFFF'u32,  # card - white
    0xFF4A4A4A'u32,  # text - dark gray
    0xFF7A7A7A'u32,  # textMuted
    0xFF2D3436'u32,  # textOnPrimary
    0xFFA8E6CF'u32,  # success - mint
    0xFFFFD93D'u32,  # warning - yellow
    0xFFFF8B94'u32,  # error - coral
    0xFFE0E0E0'u32,  # border
    0xFFF0F0F0'u32,  # divider
  ]
)

# Coffee House - Warm, cozy, brown tones
const coffeeTheme* = ThemePreset(
  name: "Coffee House",
  colors: [
    0xFFC4A484'u32,  # primary - tan
    0xFFB08968'u32,  # primaryHover
    0xFF8B7355'u32,  # secondary - brown
    0xFFD4A574'u32,  # accent - caramel
    0xFF2C1810'u32,  # background - dark brown
    0xFF3D2317'u32,  # surface
    0xFF4E2E1E'u32,  # surfaceHover
    0xFF4A3728'u32,  # card
    0xFFF5E6D3'u32,  # text - cream
    0xFFC9B8A8'u32,  # textMuted
    0xFF1A0F0A'u32,  # textOnPrimary
    0xFF7CB342'u32,  # success
    0xFFFFB74D'u32,  # warning
    0xFFE57373'u32,  # error
    0xFF5D4037'u32,  # border
    0xFF4E342E'u32,  # divider
  ]
)

# Midnight - Deep purple/blue, modern dark
const midnightTheme* = ThemePreset(
  name: "Midnight",
  colors: [
    0xFF7C4DFF'u32,  # primary - purple
    0xFF651FFF'u32,  # primaryHover
    0xFF448AFF'u32,  # secondary - blue
    0xFFFF4081'u32,  # accent - pink
    0xFF0D1117'u32,  # background - near black
    0xFF161B22'u32,  # surface
    0xFF21262D'u32,  # surfaceHover
    0xFF1C2128'u32,  # card
    0xFFE6EDF3'u32,  # text - light
    0xFF8B949E'u32,  # textMuted
    0xFFFFFFFF'u32,  # textOnPrimary
    0xFF3FB950'u32,  # success
    0xFFD29922'u32,  # warning
    0xFFF85149'u32,  # error
    0xFF30363D'u32,  # border
    0xFF21262D'u32,  # divider
  ]
)

# Forest - Nature greens, earthy
const forestTheme* = ThemePreset(
  name: "Forest",
  colors: [
    0xFF4CAF50'u32,  # primary - green
    0xFF388E3C'u32,  # primaryHover
    0xFF81C784'u32,  # secondary - light green
    0xFFFFEB3B'u32,  # accent - yellow
    0xFF1B2E1B'u32,  # background - dark green
    0xFF243524'u32,  # surface
    0xFF2D422D'u32,  # surfaceHover
    0xFF2E4A2E'u32,  # card
    0xFFE8F5E9'u32,  # text - light green tint
    0xFFA5D6A7'u32,  # textMuted
    0xFF1B2E1B'u32,  # textOnPrimary
    0xFF69F0AE'u32,  # success
    0xFFFFD54F'u32,  # warning
    0xFFFF5252'u32,  # error
    0xFF3E5C3E'u32,  # border
    0xFF2E4A2E'u32,  # divider
  ]
)

# Rose Gold - Elegant pink/rose tones
const roseTheme* = ThemePreset(
  name: "Rose Gold",
  colors: [
    0xFFE91E63'u32,  # primary - pink
    0xFFC2185B'u32,  # primaryHover
    0xFFF48FB1'u32,  # secondary - light pink
    0xFFFFD700'u32,  # accent - gold
    0xFF1A1215'u32,  # background - dark wine
    0xFF251A1D'u32,  # surface
    0xFF302226'u32,  # surfaceHover
    0xFF2D1F23'u32,  # card
    0xFFFCE4EC'u32,  # text - pink tint
    0xFFF8BBD9'u32,  # textMuted
    0xFFFFFFFF'u32,  # textOnPrimary
    0xFF69F0AE'u32,  # success
    0xFFFFD54F'u32,  # warning
    0xFFFF5252'u32,  # error
    0xFF4A2F36'u32,  # border
    0xFF3D252B'u32,  # divider
  ]
)

# Ocean Breeze - Cyan/teal, calm
const oceanTheme* = ThemePreset(
  name: "Ocean Breeze",
  colors: [
    0xFF00BCD4'u32,  # primary - cyan
    0xFF0097A7'u32,  # primaryHover
    0xFF4DD0E1'u32,  # secondary - light cyan
    0xFFFF6E40'u32,  # accent - coral orange
    0xFF0A1929'u32,  # background - navy
    0xFF0D2137'u32,  # surface
    0xFF122A45'u32,  # surfaceHover
    0xFF132F4C'u32,  # card
    0xFFE3F2FD'u32,  # text - light blue tint
    0xFF90CAF9'u32,  # textMuted
    0xFF0A1929'u32,  # textOnPrimary
    0xFF66BB6A'u32,  # success
    0xFFFFA726'u32,  # warning
    0xFFF44336'u32,  # error
    0xFF1E4976'u32,  # border
    0xFF173A5E'u32,  # divider
  ]
)

# All available themes
const allThemes* = [pastelTheme, coffeeTheme, midnightTheme, forestTheme, roseTheme, oceanTheme]

# Get theme names for dropdown options
proc themeNames*(): seq[string] =
  result = @[]
  for t in allThemes:
    result.add(t.name)

# Global theme state
var currentThemeIndex*: int = 2  # Default to Midnight

# Get current theme
proc currentTheme*(): ThemePreset =
  allThemes[currentThemeIndex]

# Set theme by index and apply it
proc setThemeByIndex*(index: int) =
  if index >= 0 and index < allThemes.len:
    currentThemeIndex = index
    applyTheme(allThemes[index].colors)

# Initialize with default theme
proc initTheme*() =
  applyTheme(allThemes[currentThemeIndex].colors)

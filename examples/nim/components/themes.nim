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
# Colors in web-standard 0xRRGGBBAA format (like CSS #RRGGBBAA)
const pastelTheme* = ThemePreset(
  name: "Pastel Dream",
  colors: [
    0xA8D8EAFF'u32,  # primary - soft blue
    0x89C9DEFF'u32,  # primaryHover
    0xAA96DAFF'u32,  # secondary - lavender
    0xFCBAD3FF'u32,  # accent - pink
    0xFFFDD0FF'u32,  # background - cream
    0xFFF5E6FF'u32,  # surface
    0xFFE8CCFF'u32,  # surfaceHover
    0xFFFFFFFF'u32,  # card - white
    0x4A4A4AFF'u32,  # text - dark gray
    0x7A7A7AFF'u32,  # textMuted
    0x2D3436FF'u32,  # textOnPrimary
    0xA8E6CFFF'u32,  # success - mint
    0xFFD93DFF'u32,  # warning - yellow
    0xFF8B94FF'u32,  # error - coral
    0xE0E0E0FF'u32,  # border
    0xF0F0F0FF'u32,  # divider
  ]
)

# Coffee House - Warm, cozy, brown tones
const coffeeTheme* = ThemePreset(
  name: "Coffee House",
  colors: [
    0xC4A484FF'u32,  # primary - tan
    0xB08968FF'u32,  # primaryHover
    0x8B7355FF'u32,  # secondary - brown
    0xD4A574FF'u32,  # accent - caramel
    0x2C1810FF'u32,  # background - dark brown
    0x3D2317FF'u32,  # surface
    0x4E2E1EFF'u32,  # surfaceHover
    0x4A3728FF'u32,  # card
    0xF5E6D3FF'u32,  # text - cream
    0xC9B8A8FF'u32,  # textMuted
    0x1A0F0AFF'u32,  # textOnPrimary
    0x7CB342FF'u32,  # success
    0xFFB74DFF'u32,  # warning
    0xE57373FF'u32,  # error
    0x5D4037FF'u32,  # border
    0x4E342EFF'u32,  # divider
  ]
)

# Midnight - Deep purple/blue, modern dark
const midnightTheme* = ThemePreset(
  name: "Midnight",
  colors: [
    0x7C4DFFFF'u32,  # primary - purple
    0x651FFFFF'u32,  # primaryHover
    0x448AFFFF'u32,  # secondary - blue
    0xFF4081FF'u32,  # accent - pink
    0x0D1117FF'u32,  # background - near black
    0x161B22FF'u32,  # surface
    0x21262DFF'u32,  # surfaceHover
    0x1C2128FF'u32,  # card
    0xE6EDF3FF'u32,  # text - light
    0x8B949EFF'u32,  # textMuted
    0xFFFFFFFF'u32,  # textOnPrimary
    0x3FB950FF'u32,  # success
    0xD29922FF'u32,  # warning
    0xF85149FF'u32,  # error
    0x30363DFF'u32,  # border
    0x21262DFF'u32,  # divider
  ]
)

# Forest - Nature greens, earthy
const forestTheme* = ThemePreset(
  name: "Forest",
  colors: [
    0x4CAF50FF'u32,  # primary - green
    0x388E3CFF'u32,  # primaryHover
    0x81C784FF'u32,  # secondary - light green
    0xFFEB3BFF'u32,  # accent - yellow
    0x1B2E1BFF'u32,  # background - dark green
    0x243524FF'u32,  # surface
    0x2D422DFF'u32,  # surfaceHover
    0x2E4A2EFF'u32,  # card
    0xE8F5E9FF'u32,  # text - light green tint
    0xA5D6A7FF'u32,  # textMuted
    0x1B2E1BFF'u32,  # textOnPrimary
    0x69F0AEFF'u32,  # success
    0xFFD54FFF'u32,  # warning
    0xFF5252FF'u32,  # error
    0x3E5C3EFF'u32,  # border
    0x2E4A2EFF'u32,  # divider
  ]
)

# Rose Gold - Elegant pink/rose tones
const roseTheme* = ThemePreset(
  name: "Rose Gold",
  colors: [
    0xE91E63FF'u32,  # primary - pink
    0xC2185BFF'u32,  # primaryHover
    0xF48FB1FF'u32,  # secondary - light pink
    0xFFD700FF'u32,  # accent - gold
    0x1A1215FF'u32,  # background - dark wine
    0x251A1DFF'u32,  # surface
    0x302226FF'u32,  # surfaceHover
    0x2D1F23FF'u32,  # card
    0xFCE4ECFF'u32,  # text - pink tint
    0xF8BBD9FF'u32,  # textMuted
    0xFFFFFFFF'u32,  # textOnPrimary
    0x69F0AEFF'u32,  # success
    0xFFD54FFF'u32,  # warning
    0xFF5252FF'u32,  # error
    0x4A2F36FF'u32,  # border
    0x3D252BFF'u32,  # divider
  ]
)

# Ocean Breeze - Cyan/teal, calm
const oceanTheme* = ThemePreset(
  name: "Ocean Breeze",
  colors: [
    0x00BCD4FF'u32,  # primary - cyan
    0x0097A7FF'u32,  # primaryHover
    0x4DD0E1FF'u32,  # secondary - light cyan
    0xFF6E40FF'u32,  # accent - coral orange
    0x0A1929FF'u32,  # background - navy
    0x0D2137FF'u32,  # surface
    0x122A45FF'u32,  # surfaceHover
    0x132F4CFF'u32,  # card
    0xE3F2FDFF'u32,  # text - light blue tint
    0x90CAF9FF'u32,  # textMuted
    0x0A1929FF'u32,  # textOnPrimary
    0x66BB6AFF'u32,  # success
    0xFFA726FF'u32,  # warning
    0xF44336FF'u32,  # error
    0x1E4976FF'u32,  # border
    0x173A5EFF'u32,  # divider
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

# Theme customization

Linux desktop frame applications follow the system palette by default. The
host reads the palette before the first frame and refreshes it while the app
is open. It uses the containing host's palette when embedded, or the configured
GTK palette on a standalone window. When the palette cannot be read, Kryon
uses its built-in colors for the detected light or dark mode.

Import `kryon/Theme` for theme tokens. `InstallDefaultTheme(dark)` installs
the default palette, metrics, fonts, shadows and motion. `SetThemeMode(dark)`
changes the palette and keeps installed metrics, fonts and motion settings.
Both calls select an explicit mode for the app. `SetThemeMode` resets palette
colors when changing modes, so reapply custom accents afterward.

Use `WithAccent(theme, accent, on_accent, accent_strong, accent_subtle)` and
`InstallTheme(theme)` to install a fixed palette. `ThemePolicy` and `ResolveDark`
support system, light and dark preferences. `SetReducedMotion` controls motion
independently of palette selection.

Typed style rules and optional CSS remain available for application-specific
colors and control styling, using the same components and interaction behavior.

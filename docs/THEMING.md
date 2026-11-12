# Theme customization

Kryon uses one modern CUI-inspired style. Light and dark are color modes of
that style; applications do not need a style-pack registry or selector.

Import `kryon/Theme` for theme tokens. `InstallDefaultTheme(dark)` installs
the default palette, metrics, fonts, shadows and motion. `SetThemeMode(dark)`
changes the palette and keeps installed metrics, fonts and motion settings.
It resets palette colors, so reapply custom accents after changing modes.

Use `WithAccent(theme, accent, on_accent, accent_strong, accent_subtle)` and
`InstallTheme(theme)` to customize colors. `ThemePolicy` and `ResolveDark`
support system, light and dark preferences. `SetReducedMotion` controls motion.

Typed style rules and optional CSS remain available for application-specific
colors and control styling, using the same components and interaction behavior.

The former style-picker gallery route remains a single-style example so
existing links continue to open.

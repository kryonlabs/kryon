# Theme customization

Native and portable frame applications follow the system palette by default.
They read it before drawing and refresh it while the app is open. Linux reads
the containing host's palette when embedded, or the configured GTK palette in
a standalone window. Windows reads system window colors. Canvas reads the
browser's color-scheme preference, and Android reads the current night mode.
When platform colors are unavailable, Kryon uses its built-in colors for the
detected light or dark mode.

The application resolves the frame's background from its current palette and
optional `canvas` style token after composing the frame. Portable hosts use
that application-resolved color when clearing the canvas. Palette changes
invalidate a retained frame, so live theme changes also update empty areas.

Import `kryon/Theme` for theme tokens. `InstallDefaultTheme(dark)` installs
the default palette, metrics, fonts, shadows and motion. `SetThemeMode(dark)`
changes the palette and keeps installed metrics, fonts and motion settings.
Both calls select an explicit mode for the app and stop automatic system
palette updates. `SetThemeMode` resets palette
colors when changing modes, so reapply custom accents afterward.

Use `WithAccent(theme, accent, on_accent, accent_strong, accent_subtle)` and
`InstallTheme(theme)` to install a fixed palette. `ThemePolicy` and `ResolveDark`
support system, light and dark preferences. `SetReducedMotion` controls motion
independently of palette selection.

Typed style rules and optional CSS remain available for application-specific
colors and control styling, using the same components and interaction behavior.
Optional CSS also covers structural selectors (`:first-child`, `:last-child`,
`:nth-child()` and their of-type family), sibling combinators (`+`, `~`),
complete flexbox layout and
`linear-gradient()`/`radial-gradient()` fills; the exact native subset and the
remaining browser-only features are listed in [HTML_TO_KRYON.md](HTML_TO_KRYON.md).

Plain `Text.Draw` uses the built-in foreground and accent without importing the
full theme. Installing a theme through `kryon/Theme` connects its current
colors automatically; subsequent theme changes apply to labels and links.
The frame loop imports palette values and host capabilities without importing
stylesheet rules. Stylesheets install their text resolver and tree selector
matcher when used;
motion installs its sampler independently. The public `Text.Draw` call stays the same in each case.

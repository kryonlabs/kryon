# Plan 12 — Aero Theme

A Windows Vista/7-glass style implemented centrally in kryon as a new
`ThemeStyle` (`THEME_STYLE_AERO`), so every widget (C API, ui_tree nodes,
`.kry` widgets) and every vendoring app inherits it.

Design decisions:

- **Simulated glass only** — translucent fills, gloss gradients, highlight
  borders, hover glow, layered soft shadows, all with existing drawing
  primitives. No shaders, so it renders identically on raylib, Android, web,
  software and null backends.
- **Signature palette + all existing palettes** — new `THEME_AERO`
  (13th palette, `aero_light`/`aero_dark` scopes, `themes/aero*.ini`) is the
  style's default; the style still derives its glass tints from whichever
  palette is active, so all 12 older palettes work.
- **Opt-in** — SYSTEM resolution and platform defaults are unchanged; apps
  that pin RETRO/MATERIAL keep their look until they select Aero.

Structure:

1. `include/theme_style.h` appends `THEME_STYLE_AERO` after MATERIAL (old
   app clamps treat it as invalid → SYSTEM fallback, safe until bumped).
2. `src/core/theme.c` accepts AERO in `SetThemeStyle`, labels it via the
   existing `theme_style_aero` locale string, defaults it to `THEME_AERO`.
3. `src/ui/ui_style.c` gains an AERO token preset (4/8px radii, translucent
   control/panel alpha, gloss `shine_alpha`, soft shadow offset).
4. New `src/ui/ui_aero.c`: `UIAeroScheme` derived from the 8 theme colors
   (glass fill, gradient top/bottom, inner highlight, border, hover glow,
   pressed shade, focus ring) + paint helpers:
   - `ui_aero_paint_control` — glossy control background (rounded fill,
     alpha-stepped gloss overlays toward the top, inset highlight border,
     hover glow rings, pressed shade)
   - `ui_aero_paint_panel` — glass panel (layered soft shadow, translucent
     fill, upper sheen, highlight + border)
   - `ui_aero_paint_inset` — carved-in fields/troughs
   - `ui_aero_paint_track` — gradient fills for progress bars/slider
     tracks/titlebars
   These hook into the shared `ui_draw_control_background()` path so most
   classic widgets get glass without per-widget edits.
5. Widget sweep: paint-only Aero branches beside the existing Material
   branches in button.c, ui_slider.c, text inputs, dropdown.c, tab_bar.c,
   scroll.c, ui_tk.c, modal.c, toast.c, chrome files (top/bottom nav,
   toolbar, titlebar, rows, profile header, icon controls, guide,
   theme_picker.c). Layout/hit-testing/props ABI untouched.
6. Palette + picker: `themes/aero.ini` + `themes/aero_dark.ini`,
   `theme_meta.{h,c}` entries, locale `theme_aero` label, style dropdown
   gains the 4th option, sanitize accepts AERO, Go binding
   `ThemeStyleAero`. `.kry` apps get constants via `#import "kryon.h"`.
7. Rollout: commit on kryon master, then bump `vendor/kryon` pointers and
   style/theme-count clamps in inbe, uku and krait (vendor rule).

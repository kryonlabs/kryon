# Widget styling

Button is the reference implementation for Kryon's default control styling.
Other controls resolve the same styles and consume the same material and content
metrics. They adapt layout to their interaction; they do not copy screenshot
colors or maintain a second theme palette.

The visual reference is the Magnetic Lightfield button board in
`design/widget-proposals/08-magnetic-lightfield.png`. The Frosted Glass dropdown
proposal extends that visual language with a floating translucent panel.
Reference images guide visual review; executable shared policy defines behavior.

## Sources of truth

| Concern | Shared source | Responsibility |
|---|---|---|
| Theme palette and dimensions | `runtime/theme.kry` | Active theme colors, radii, size tokens, timing |
| State and style overrides | `runtime/style.kry` | State priority, explicit fields, interpolation, content sizing |
| Button appearance and content | `runtime/button.kry` | Tone/emphasis colors, measurement, motion, content drawing |
| Material geometry | `runtime/surface.kry` | Face, gradients, rim, shadow, glow, focus, chevron coverage |
| Material assembly | `runtime/material.kry` | Layer bounds, scale, fill interpolation, shared surface segments |
| Dropdown role adaptations | `runtime/dropdown.kry` | Popup glass and selected-row emphasis; inherited Button metrics |

C and Go resolve the active Button style before adapting it for a widget role.
Backend code draws shared policy results. A new palette or effect belongs in the
shared source, with generated runtime and visual verification alongside it.

## Color and emphasis

Tone communicates purpose: accent, neutral, danger, success, warning, or link.
Emphasis controls treatment: filled, soft, outline, ghost, or link. A selected
value is not a dangerous action and should not acquire a semantic tone merely
to resemble an image. Custom theme accents must propagate to the entire control.

Dark controls use depth, illuminated rims, and readable foregrounds. Light
controls use pale faces, restrained shadows, and colored ink. Resolve each mode
through Button; do not invert colors or use the same translucent black shadow
in both modes. Disabled labels remain readable according to Button policy.

## States and motion

The automatic state priority is disabled, loading, pressed, hovered, focused,
selected, then normal. An explicit state overrides automatic interaction after
disabled/loading normalization. Focus is also a separate motion channel, so a
hovered focused control retains its focus cue.

| State | Styling and behavior |
|---|---|
| Normal | Resolved tone/emphasis face, border, foreground, and material |
| Hover | Button hover colors and shared hover interpolation |
| Pressed | Button pressed colors and material displacement |
| Focus | Shared focus color and ring; focus is never simulated by recoloring text |
| Selected | Persistent value indication, distinct from pointer or keyboard hover |
| Disabled | Button disabled style; no activation, hover, or focus effects |
| Loading | Button's activity rendering; replaces content and prevents activation |

Use the shared motion tracks and the theme's fast/normal timings. Do not invent
per-widget easing or dim a disabled color again with an unrelated alpha value.
A menu option's hover indicates navigation; its checkmark indicates selection.
Opening a popup does not commit the highlighted value.

## Typography, spacing, and geometry

These are default logical units; use resolved metrics so theme changes propagate.
Explicit physical bounds remain authoritative. A shorter rectangle alone does
not change a control's semantic size.

| Token | Small | Medium (default) | Large |
|---|---:|---:|---:|
| Minimum height | 32 | 40 | 48 |
| Horizontal padding | 12 | 16 | 20 |
| Font size | 16 | 18 | 20 |
| Icon size | 14 | 18 | 20 |

The default icon/text gap is 8. Use the style's radius, border width, foreground,
opacity, typeface, and content offsets. Measure labels with the resolved font.
Clip long content before trailing affordances. Icons follow the foreground
color unless the existing icon asset intentionally carries its own colors.

## Dropdown mapping

| Part | Button source | Allowed adaptation |
|---|---|---|
| Trigger | Neutral + soft, matching state | Left-aligned value, leading option icon, trailing disclosure; Button style is unchanged |
| Floating panel | Neutral + soft, normal | Shared glass material, translucent gradient, panel radius |
| Ordinary row | Neutral + soft | Panel remains visible until hover or selection |
| Hovered row | Neutral + soft, hover | Inset row bounds; light rows are flat and borderless |
| Selected dark row | Accent + filled, normal | Checkmark; uses the same accent material as Button |
| Selected light row | Accent + soft, normal | Flat, borderless row and checkmark |
| Disabled option | Neutral + soft, disabled | Cannot activate; keyboard navigation skips it |

The trigger and rows use the resolved Button font size, icon size, padding, and
gap. `DropdownOption.font_name` can select a registered typeface for an option;
labels, leading icons, and checkmarks use the row foreground. A separator is a
subtle rule before its option and does not create an extra selectable row.
Popup geometry, scrolling, and input capture refer to the same bounds.

Searchable and multiple-selection menus are compositions with additional
interaction and state. The reference board illustrates them; the single-select
Dropdown API does not implicitly provide those behaviors.

## Applying the contract to another widget

Choose the widget's roles and map each to a Button tone, emphasis, and state.
Inherit the resolved style and content metrics. Document any role adaptation,
such as a flat list row or floating panel, in a mapping like the one above.
Keep input/state ownership in the widget and material drawing in shared policy.

Verify dark and light themes, a non-blue custom accent, keyboard focus, disabled
behavior, long labels, and selection versus hover. Compare actual rendered
controls against Buttons at matching sizes and states. Run generated-output
scanners and C/Go parity when runtime behavior changes. `make dropdown-capture`
produces the native dropdown board. Its dark panel was approved on 2026-09-10
and is checked by `make dropdown-reference-test`; the light panel remains a
review artifact. `examples/27_dropdowns.kry` provides the live interactive showcase.

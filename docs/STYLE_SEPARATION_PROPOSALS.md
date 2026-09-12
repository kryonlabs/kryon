# Kryon Style Separation Plan

Status: open proposals — staged plan\
Scope: removing visual styling 100% from widget props and widget implementations, and adding a limited CSS-class styling layer to Kryon\
Rule: widgets own structure, input, state, and measurement; the style layer owns every visual value; one implementation; C/Go parity; no compatibility aliases

Kryon widgets still do two jobs. They own widget behavior — identity, input,
state, layout, and content — and they also carry or assemble visual styling:
colors, radii, spacing, typography, materials, and state appearance. This
plan defines what 100% separation means, inventories where styling leaks into
widgets today, and details five proposals for a limited CSS-class styling
layer inside Kryon. All five proposals lower into the existing `StyleData`,
`MergeValues`, and `ResolveValues` machinery in `runtime/style.kry`; they
differ only in how style values reach a widget instance. Rendering, materials,
and transition paths do not fork between proposals.

## 1. Goal

- Widget code contains zero visual decisions. A widget implementation decides
  *what* it is (kind, identity, content, state, input, measurement) and never
  *how it looks* (no colors, radii, paddings, font sizes, opacities, or
  materials chosen in widget code).
- One styling path per widget family, expressed as shared `.kry` policy, in
  the established `DefaultButtonStyle` pattern from `runtime/button.kry`.
- App-facing styling that reads like a limited CSS: closed property
  vocabulary, class- and name-based targeting, state variants, theme-token
  values, cascade limited enough to stay deterministic and testable.
- Styling works identically in C, Go, JS, and KRB, is hot-reloadable through
  `kryon-preview`, and degrades cleanly on the `termi` cell backend.
- Enforcement by scanners and capture boards, so separation cannot regress.

## 2. Definition of done — the separation contract

| Rule | Requirement |
|---|---|
| S1 | No public widget props struct carries a raw visual field. `Color`, radius, padding, gap, font size, opacity, material, and typeface do not appear in `*Props`. Props carry identity, behavior, content pointers, and semantic enums (tone, emphasis, size, role, state) plus at most one style reference (see proposals). |
| S2 | Every widget family resolves visuals through one pure shared policy function in `runtime/<widget>.kry`: `Default<Widget>Style(semantics, state, Palette, Metrics) -> StyleData`. |
| S3 | Every visual value originates from a theme token (`Palette`, `Metrics`) or a style rule. Raw literals are legal only inside style rules and theme definitions, never at widget call sites. |
| S4 | State appearance for all widgets is selected by `runtime/style.kry` (`ResolveState`, `ResolveValues`); no widget re-implements state precedence. |
| S5 | Paint code consumes resolved `StyleData` through the shared surface/material path (`runtime/surface.kry`, `runtime/material.kry`). Widget `.c` files do not call `GetTheme*` getters to assemble visuals. |
| S6 | The identical rule set and resolution run in C, Go, JS, and KRB — rules are compiled to data, not re-parsed per backend — guarded by the existing parity gates. |
| S7 | `scripts/check-style-separation.py` enforces S1 and S5 inside `make test`, and a per-family capture board renders every state in light and dark themes. |

"100% separated" means S1–S7 hold with zero scanner exemptions. Anything a
widget currently does with a `Color` either becomes a semantic enum, a style
rule, or a theme token.

## 3. Current state

### 3.1 Already separated

| Concern | Owner | Status |
|---|---|---|
| Semantic palette (29 colors: surfaces, text tiers, accent/semantic pairs, focus, selection, shadow) | `runtime/theme.kry` `Palette`, mirrored by `ThemeColors` in `include/theme.h` | separated |
| Metrics (radii, spacing scale, control heights/paddings, type and icon scale, shadow, timings) | `runtime/theme.kry` `Metrics`, mirrored by `ThemeMetrics` in `include/ui_controls.h` | separated |
| State precedence (disabled > loading > pressed > hovered > focused > selected > normal) and explicit-state overrides | `runtime/style.kry` `ResolveState` / `ResolveInteraction` | separated |
| Presence-bit style merge (a field is set or absent; transparent is a value, not "unset") | `runtime/style.kry` `StyleData`, `MergeValues`, `ResolveValues`; field bits in `runtime/control_props.kry` `StyleField` | separated |
| Button reference styling (tone x emphasis x size x state, pill/circle shapes) | `runtime/button.kry` `DefaultButtonStyle` | separated for the Button family; other families still adapting |
| Materials (flat, lightfield, glass), focus track, shadows, glow | `runtime/surface.kry`, `runtime/material.kry` | separated |
| State transitions and motion | `runtime/style.kry` `TransitionFrame`, `InteractionMotion` | separated |
| Theme files, named scopes, INI persistence, export/import | `themes/*.ini`, `ThemeScope` API in `include/theme.h` | separated, but colors only |

### 3.2 Where styling still leaks into widgets

| Leak | Location | Symptom |
|---|---|---|
| Inline override table in props | `ButtonProps.style: ControlStyle` (`include/ui_button_props.generated.h` from `runtime/button_props.kry`) | A full seven-state style table travels inside a widget call; callers hand-assemble appearance at every call site |
| Raw-color input styling | `TextInputStyle` in `include/ui_controls.h`, embedded in `TextInputProps`, `TextFieldProps`, `TextAreaProps`, `ReadonlyTextBoxProps` | `background`, `border`, `focus_border`, `text`, `cursor`, `radius`, `padding_x/y` are props |
| Raw text colors | `TextProps.color` (`include/ui_text.h`); retained `primitive.color`/`primitive.border` (`include/ui_tree.h`) | Every text call site picks a color by hand; theme-aware text requires reading the palette in layout code |
| Row, link, and page colors | `label_color`/`color` in `include/ui_rows.h`; `background`/`hover_color` in `include/ui_page.h` | Widgets accept one-off colors with no token vocabulary |
| Alpha-zero-means-default fallbacks | For example `src/ui/button.c`: `background.a != 0 ? background : c_button` | Widget code invents defaults and precedence; transparent cannot be distinguished from unset |
| Direct theme reads inside widget paint paths | 38 `GetTheme*` calls across `src/ui/*.c` | Widgets assemble their own visuals instead of consuming resolved `StyleData` |
| Inline style literals in app code | `examples/02_buttons.kry` constructs `(Style){...}` at call sites and reads `chrome.colors.*` in layout code | Styling decisions live mixed into layout code |
| No rule layer at all | — | The same override is retyped per call site; nothing corresponds to a CSS rule, class, or scoped theme |

The leaks are cumulative: props carry style values, widget code fills gaps
with hardcoded fallbacks, and apps bypass policy by inlining literals.
Removing the props channel without adding a rule layer would force more
hardcoded defaults, so the two moves must be staged together.

## 4. Shared constraints and the resolution spine

Every proposal must satisfy these constraints, taken from the repository
rules and the existing runtime shape:

1. **One spine.** Style resolution is always:
   `defaults -> rule/class merge -> state select -> transition -> paint`.
   The merge step is the existing presence-bit `MergeValues`; state selection
   is `ResolveValues`; transition is `TransitionFrame`. Proposals differ only
   in how the *rule/class merge* inputs are declared and stored.
2. **Closed property vocabulary.** Properties are exactly the 16 `StyleField`
   values (`background`, `foreground`, `border`, `focus`, `radius`,
   `border_width`, `opacity`, `padding_x/y`, `gap`, `font_size`, `icon_size`,
   `content_offset`, `background_end`, `material`, `typeface`) plus the
   semantic switches `tone`, `emphasis`, `size`, and text `role`. Adding a
   property is a runtime + parity change in the same commit (Test Rule).
3. **Tokens before literals.** Rule values reference theme tokens (`@accent`,
   `@space_4`, `@radius_medium`). Light/dark switching happens in the
   palette, never in rules.
4. **Immediate-mode safe.** Rules match on stable, interned identity
   (widget kind, class name, node name) — never on pointers or call order.
   Resolution output is cacheable per frame and invalidatable on theme or
   rule change.
5. **Backend degradation, not forking.** Rule resolution is backend
   independent. `termi` collapses geometry (radius, border, material) and
   maps colors to cells; `canvas`/`dom`/`libdraw` consume the same resolved
   `StyleData` they consume today.
6. **Compiled, not re-parsed.** Shipped apps embed rule tables as data
   (KIR constants / generated C/Go). Text parsing exists only in
   `kryon-preview` and `krb-run` for hot reload.
7. **Clean naming.** The surface uses domain names (`Style`, `Theme`,
   `Surface`, tone/emphasis/role). No invented prefix, no CSS jargon that
   does not map to a Kryon concept.
8. **Widgets keep ownership of state.** Hover, press, focus, and selection
   are input facts owned by widgets; rules only describe how a state looks.

The five proposals below are ordered from "least new surface" to "most new
surface". They are not mutually exclusive; section 11 stages them.

## 5. Proposal A — Token-only semantic styling (no override channel)

### Concept

Finish the direction `docs/WIDGET_STYLING.md` already describes: props carry
only semantic enums, and every visual value is a function of
`(semantics, state, Palette, Metrics)` computed by shared policy. There is no
per-instance style override surface at all — the theme plus the semantic
vocabulary is the entire styling system. This is the "no CSS" baseline that
every other proposal needs anyway.

### Surface

```kry
Button save { label = "Save"; tone = Accent; emphasis = Filled }
Button undo { label = "Undo"; tone = Neutral; emphasis = Ghost; size = Small }
Text title { text = "Account"; role = Title }
Text hint  { text = "Optional"; role = Caption }
TextField name { placeholder = "Name"; tone = Neutral }
```

C mirrors this exactly: `ButtonProps` keeps `tone`, `emphasis`, `size`,
`state`; loses `style`. `TextInputStyle` is deleted; input widgets gain
`tone`/`size`. `TextProps.color`/`.border` are replaced by the `role` system
already proposed for Text in `docs/TEXT_NODE_PROPOSALS.md`.

### Code changes

- Extend the semantic vocabulary where apps need it: one `Tone` list shared by
  all controls (neutral, accent, danger, success, warning, info, link), the
  existing `Emphasis` list, the existing `ControlSize` list, and Text roles.
- Write `Default<Widget>Style` for every family that lacks one (inputs, rows,
  href, card, slider, toggle, checkbox, radio, progress, separator) in its
  `runtime/<widget>.kry`, all consuming `Palette`/`Metrics` only.
- Delete raw visual fields from props: `ButtonProps.style`,
  `TextInputStyle` (four structs), `TextProps.color/border`, row and page
  colors. Update `runtime/*_props.kry`, regenerate headers, migrate callers.
- Replace the 38 paint-time `GetTheme*` calls in `src/ui/*.c` with resolved
  `StyleData` consumption.
- One-off looks become theme scopes (existing `RegisterThemeScope`) or new
  semantic enum values — decided in Kryon, not per call site.

### Migration

Breaking change to props structs. Migrate `examples/`, then downstream apps
(kapsule, inbe, krait, uku, atr) with one submodule pointer bump each after
the Kryon commit lands on master.

### Performance, parity, testing

Zero runtime cost — resolution is the existing pure functions. Parity risk is
low (no new runtime semantics). Capture boards per family plus the new
scanner are the regression gate.

### Assessment

- Pros: complete separation by construction; smallest possible surface;
  trivially parity-stable; zero runtime cost; no new language or file format;
  matches the published Button contract.
- Cons: no per-instance customization; every real one-off look becomes an
  enum or theme-scope request; downstream apps with brand-specific screens
  may push back; vocabulary growth becomes the release valve and needs
  review discipline.
- Effort: medium (mostly migration). Risk: low.

## 6. Proposal B — Named style registry (classes without selectors)

### Concept

Add exactly one override channel: named styles, registered once and referenced
by name — the moral equivalent of a CSS class, without any selector engine. A
style is data declared beside the UI, resolved at registration, and merged
over widget defaults by the existing `MergeValues`. Call sites stop carrying
inline tables and start carrying a name.

### Surface

`.kry` top-level declaration (compiled to a static table by `k2c`/`k2go`):

```kry
Style danger-cta: ControlStyle {
    normal  = (Style){.background = @danger, .foreground = @on_danger}
    hover   = (Style){.background = @danger_pressed}
    pressed = (Style){.background = @danger_pressed, .opacity = 0.9}
}

Button delete-account { label = "Delete account"; style = "danger-cta" }
```

C host API (also mirrored in `go/kryon`):

```c
bool RegisterStyle(const char *name, ControlStyle style);   /* app or generated code */
const ControlStyle *FindStyle(const char *name);            /* NULL = no such style */
void ClearStyles(void);                                     /* tests and reloads */
```

`ButtonProps` and friends gain a single borrowed `const char *style` field.
The inline `ControlStyle style` table field is removed (S1 still holds: a
name is a reference, not a visual value).

### Resolution pipeline

1. Widget defaults from Proposal A policy: `Default<Widget>Style(...)`.
2. Registry lookup by name; merge the found `ControlStyle`'s state slice via
   the existing `ResolveValues`/`MergeValues`.
3. State select and transition as today.

Resolution cost is one hash lookup per widget per frame, or zero when the
table is static and names are interned at generation time.

### Code changes

- New `runtime/style_registry.kry` policy module (intern table, lookup,
  merge order) so C, Go, and JS share one implementation.
- `k2c`/`k2cpp`/`k2go`: lower top-level `Style` declarations into generated
  registration calls or static tables; extend the props-field mappings in
  `cmd/kir/kir_parse.c` and `cmd/k2go/k2go_lower.c`.
- Replace `ButtonProps.style` usage in examples with named styles.

### Migration

Additive first (registry + name field), then remove the inline table field in
a second commit once callers move. Same downstream pointer-bump flow as A.

### Performance, parity, testing

Near-zero runtime cost. Parity: the table is data; both runtimes must produce
identical merges — cover with the scripted parity harness on a board that
uses named styles. Names are app-scoped; duplicate registration is an error
at registration time, checked in tests.

### Assessment

- Pros: the smallest possible "limited CSS" step; reuses all existing
  machinery; deterministic; compiles to data; natural fit for design-system
  modules shared across apps.
- Cons: no structural targeting (cannot say "all buttons in this panel");
  string references can drift (mitigate by compile-time name checking in
  `k2c`/`k2go`); still no locality between layout and style.
- Effort: medium. Risk: low-medium (frontend lowering work in `k2c`/`k2go`).

## 7. Proposal C — Tree-scoped cascade (`Style` nodes in `.kry`)

### Concept

Make `Style` a first-class node inside the UI tree. A `Style` node declares a
rule; its position in the tree defines its scope — it applies to matching
descendants of its parent container. This is the "limited CSS" proper: kind,
class, and name targeting with state variants, scoped by structure, with a
fixed specificity rule and no free-form cascade. It localizes styling where
the structure lives, without an external file or a second language.

### Surface

```kry
Column danger-zone {
    Style Button        { tone = Danger; emphasis = Filled }
    Style Button.cta    { radius = 2 }
    Style .quiet        { emphasis = Ghost }
    Style #confirm:hover { background = @danger_pressed; opacity = 0.9 }
    Style TextField     { tone = Danger; padding_x = @space_4 }

    Text title { text = "Danger zone"; role = Title }
    TextField name { placeholder = "Type DELETE"; class = "quiet" }
    Button confirm { label = "Delete account"; class = "cta"; tone = Danger }
}
```

Props gain one identity field: `class = "cta"` (a short class list; the
`style = "name"` reference from Proposal B remains for whole-table reuse).

### Selector grammar

Deliberately small, defined once, identical across backends:

```ebnf
rule         = rule-target [ state-suffix ] ;
rule-target  = kind | class | name | kind "." class ;
kind         = "Button" | "TextField" | ... ;  (* widget kind names *)
class        = "." identifier ;
name         = "#" identifier ;
state-suffix = ":" ( "hover" | "pressed" | "focused" |
                     "selected" | "disabled" | "loading" ) ;
```

- Scope: a rule matches only descendants of the `Style` node's parent.
- Specificity is fixed and total: `kind` < `.class` < `kind.class` < `#name`
  (for example `Button.cta` beats both `Button` and `.cta`). Within equal
  specificity, later source order wins. That is the
  entire cascade — no parent or sibling combinators, no attribute selectors,
  no media queries (light/dark is a palette concern), no inheritance of
  layout.
- State suffixes select the `ControlStyle` state slot the rule's values land
  in; unsuffixed rules fill `normal` and remain present for merging.
- Duplicate `#name` inside one scope is a compile error in `k2c`/`k2go` and a
  runtime diagnostic in preview.

### Property vocabulary

Same closed set as section 4: the 16 `StyleField` properties, token
references (`@accent`, `@radius_medium`, `@space_4`), literals, and the
semantic switches (`tone`, `emphasis`, `size`, `role`). Unknown properties or
tokens are compile errors, not warnings.

### Resolution pipeline

1. Hosts walk the frame as today. Entering a container collects its `Style`
   child rules onto a scope stack; leaving the container pops them. This
   mirrors the existing `#instance(key)` retention pattern — no new lifetime
   model.
2. For each widget instance, the resolver gathers candidate rules from the
   scope stack (outermost first), orders them by the fixed specificity plus
   source order, and merges their state slices over the Proposal A defaults
   with `MergeValues`. Named-registry styles (Proposal B) merge before
   cascade rules.
3. `ResolveValues` selects the state slice; `TransitionFrame` interpolates.
4. The retained tree stores resolved `StyleData` per instance, so repaint and
   KRB serialization reuse identical values.

Cache: resolved `StyleData` keyed by `(scope fingerprint, kind, classes,
name, state)`. The fingerprint changes only when the scope stack, theme, or a
rule changes — not per frame. Hot reload through `kryon-preview` invalidates
fingerprints and re-resolves without rebuilding layout.

Cost model: rules are few (tens, not thousands); matching is comparisons on
interned strings, ordered once per scope entry. Worst case is
`O(rules_in_scope x widgets_in_scope)` per fingerprint change, amortized to
near zero in steady state.

### Code changes

- New `runtime/style_cascade.kry`: rule record, ordering predicate, scope
  stack, gather-and-merge. One implementation shared by C, Go, and JS.
- `cmd/kir/kir_parse.c`: parse `Style` child nodes and `class` props fields.
- `cmd/k2c`, `cmd/k2cpp`, `cmd/k2go`, `cmd/k2js`: emit rule tables and scope
  push/pop around container calls; extend props-field mappings.
- `cmd/k2b`: serialize the rule table into the KRB cartridge.
- `src/ui/ui_tree.c` and the retained tree: store resolved `StyleData` and
  the scope fingerprint per instance.
- `kryon-preview`: rule editing triggers fingerprint invalidation; the
  inspector shows which rule contributed each field.

### Migration

Purely additive at first (`Style` nodes and `class` fields are optional).
Once examples and apps express overrides as rules, the inline
`ControlStyle` props channel is deleted (finishes Proposal A's S1). The
`#name` targets use the node names `.kry` already has (`Button save { ... }`).

### Performance, parity, testing

- Parity harness drives a cascade-heavy board (nested scopes, class/kind/name
  conflicts, state suffixes) through identical scripted input on C and Go.
- Golden captures pin the resolved boards in light and dark themes.
- Determinism tests: source order, specificity ties, scope shadowing, and
  token indirection are unit-tested in `runtime/style_cascade.kry` output.
- `termi` board proves geometry degradation.
- Identity tests: repeated names across sibling scopes, class lists, name
  reuse across frames.

### Assessment

- Pros: styling lives with structure; the familiar CSS mental model with the
  dangerous parts removed; no new file format; scoped, cacheable,
  deterministic; `.kry`-canonical (matches "new public widget behavior
  starts in `.kry`"); enables deleting the last inline override channel.
- Cons: the largest frontend change (parser, four backends, retained tree);
  cascade semantics must be specified and policed against scope creep;
  immediate-mode hosts must maintain the scope stack correctly.
- Effort: large. Risk: medium — contained by sharing one `.kry` engine and
  landing after A and B.

## 8. Proposal D — Kryon style sheets (`.kss` external files)

### Concept

Move rules out of application source entirely into a separate style sheet
file, the way CSS separates from HTML. The selector and property grammar is
Proposal C's, unchanged; only the container differs — a file bound to the app
by an import. Shipped apps compile the sheet into the same rule table C
uses; only the preview tooling parses text at runtime.

### Surface

`brand.kss`:

```text
Button.danger  { background: @danger; foreground: @on_danger }
#save:hover    { background: @accent_hover }
TextField      { border: @border; radius: @radius_medium; padding: @space_4 }
Card           { material: glass; radius: @radius_large }
Text.caption   { role: Caption }
```

Binding and host API:

```kry
#style "brand.kss"        # inside the app block; k2c/k2go compile and embed
```

```c
bool LoadStyleSheet(const char *path);   /* preview and dynamically themed apps */
void ApplyStyleSheet(RuleTable table);   /* compiled-in tables */
```

### Resolution pipeline

Identical to Proposal C: the file lowers to the same rule table, so
specificity, scope, states, tokens, and caching do not change. A file-level
rule has no tree scope of its own; it applies at app scope (outermost), and
in-tree `Style` nodes (Proposal C) always win over file rules.

### Code changes

- New small parser (`cmd/kss` shared library used by `k2c`/`k2go`/`kryon-preview`)
  for the text grammar; emitted diagnostics carry file and line.
- `#style` import handling in the KIR frontend; rule-table serialization for
  KRB.
- `kryon-preview`: watch and hot-reload `.kss` edits with fingerprint
  invalidation — the classic edit-without-recompile workflow.

### Migration

Additive and independent of A/B/C milestones; can be adopted per app.

### Performance, parity, testing

Zero shipped-runtime parsing; parity rides on the shared rule table. Tests:
parser golden files (valid, invalid, token errors with line numbers),
compile-embed equality (`LoadStyleSheet(f)` output equals the generated
table), hot-reload frames in preview, and one end-to-end board.

### Assessment

- Pros: strongest physical separation (app code never mentions visuals);
  designers can own one file; hot reload without recompiling; familiar
  workflow for web-native developers.
- Cons: a second source language to specify, test, and keep in sync with
  `.kry` semantics; errors span two files; risk of drift between preview
  parsing and compiled tables (mitigated by one shared parser); KRB grows a
  table section; duplication with Proposal C's grammar must stay literal —
  one grammar definition, two containers.
- Effort: large. Risk: medium — mostly tooling and specification weight,
  not runtime.

## 9. Proposal E — Class-scoped theme palettes (theme files as the styling surface)

### Concept

Make the theme the only styling surface. Extend the existing theme system —
`ThemeScope`, `themes/*.ini`, `runtime/theme.kry` — from a flat palette to
layered, per-widget-class token groups with state variants and a metrics
subset. Apps and shipped themes restyle whole widget families by editing
theme data; app code never changes. This is "CSS by theme" with a closed,
reviewed vocabulary instead of open-ended rules.

### Surface

Theme file growth (INI today; same content projected into `theme.kry`):

```ini
[Button]
background       = @surface_raised
background.hover = @accent_hover
foreground       = @text

[Button.danger]
background       = @danger
foreground       = @on_danger

[Slider]
track            = @surface_sunken
thumb            = @accent
radius           = @radius_medium
```

Host API (extending the existing scope API):

```c
Color GetThemeClassColor(const char *klass, const char *key);  /* "Button.danger" */
float GetThemeClassMetric(const char *klass, const char *key);
```

`runtime/theme.kry` gains a layered lookup: class-scoped token, else base
palette token. `Default<Widget>Style` reads through this projection, so all
existing policy functions keep their signatures.

### Resolution pipeline

No new resolution step: tokens resolve at theme load (indirection `@ref`
resolved once, cycle-checked), and shared policy consumes the flattened
result exactly as it consumes `Palette`/`Metrics` today. Light/dark stays a
palette switch.

### Code changes

- `include/theme.h` / `src/core/theme.c`: scope entries gain metric values,
  state variants, and `@ref` indirection; loader rejects unknown class/token
  names (closed vocabulary, versioned).
- `runtime/theme.kry`: layered palette projection shared by all hosts.
- Port the shipped `themes/*.ini` families to the new keys where they
  intentionally differ from defaults; keep user themes forward-compatible
  through the aggregate-variable path.

### Migration

Additive and theme-file-only; no props changes, no app code changes. Pairs
naturally with Proposal A (A deletes per-instance overrides; E restores
family-level variety through data).

### Performance, parity, testing

One-time load resolution, cached flat tables, zero per-frame cost beyond
today. Tests: loader acceptance/rejection boards, light/dark projections,
INI round-trips through export/import, capture boards for each restyled
family, and the theme picker flow.

### Assessment

- Pros: reuses the entire existing theme pipeline (INI, scopes, persistence,
  export/import, system theme); user-visible theming gets deeper without new
  language surface; perfect for shipped theme families; zero app-code churn.
- Cons: theme files risk becoming an unbounded pseudo-CSS — the closed,
  whitelisted vocabulary must be enforced by the loader; no per-instance or
  structural targeting; INI needs metrics and state keys (format growth);
  everything interesting still requires editing a theme, which may be too
  coarse for app screens.
- Effort: medium. Risk: low-medium (format growth and compatibility).

## 10. Comparison

| Criterion | A Token-only | B Registry | C Cascade | D `.kss` files | E Theme scopes |
|---|---|---|---|---|---|
| Separates styling from widget code | complete | complete | complete | complete | complete |
| Removes raw visuals from props | yes | yes (name ref only) | yes (class ref only) | yes (nothing) | yes |
| Per-instance overrides | none | named classes | classes + names | classes + names | none |
| Structural targeting (scope) | none | none | descendant scopes | app scope | none |
| State variants | policy only | per-state slots | per-state rules | per-state rules | theme keys |
| Hot reload without recompile | no (theme reload only) | re-register | preview rule edit | yes (best) | theme reload |
| New parser / file format | none | none | `.kry` grammar growth | new `.kss` parser | INI growth |
| Runtime cost per frame | zero | one lookup | cached match | cached match | zero |
| Frontend/tooling effort | low | medium | high | high | low |
| Parity risk | low | low-medium | medium | medium | low |
| Fits immediate mode | n/a | fully | scope stack | fully | fully |
| Expressiveness for real apps | low | medium | high | high | medium (family-level) |

## 11. Recommendation

Stage the proposals; do not choose one.

1. **Adopt A unconditionally.** It is the actual "separation" — every other
   proposal needs the semantic defaults, the vocabulary, and the scanner
   anyway. A alone already satisfies S1–S7 for the theme-driven apps.
2. **Adopt B as the v1 override channel.** It is small, compiles to data, and
   gives downstream apps a legal replacement for the deleted inline
   `ControlStyle` tables before the inline field is removed.
3. **Adopt C as the target app-facing styling surface** — this is the
   "limited CSS" this plan exists to add. Land it after A and B so the
   cascade only ever merges over semantic defaults, and so the engine, the
   parser work, and the parity harness arrive when the rest of the system is
   already clean. C subsumes B's registry as one more merge input; both share
   `MergeValues`.
4. **Hold D.** Revisit only if edit-without-recompile becomes a real design
   workflow. If adopted, D compiles into C's rule table through the same
   parser library — never a fourth resolution path.
5. **Fold E's minimal form into A's milestones** (class token groups for the
   families that shipped themes actually want to differentiate). E keeps
   user-facing theming deep while A+B+C keep app code clean.

Final state under A+B+C(+E): widget props contain zero visual values; every
visual value is a theme token or a rule; rules live next to the structure
they style; one `.kry` engine resolves them identically on every backend;
scanners and capture boards hold the line.

## 12. Phased roadmap

Every milestone lands on kryon `master` as complete commits with tests in
the same change (Test Rule), then downstream apps bump their `vendor/kryon`
pointers. Nothing here edits vendor copies.

### M0 — Audit and enforcement (foundation, no behavior change)

- Add `scripts/check-style-separation.py`: flags raw visual fields in public
  props headers and paint-time `GetTheme*` assembly in `src/ui/*.c`; starts
  with the current leak list as a baseline report, wired into `make test` as
  non-failing inventory.
- Update `docs/BOUNDARIES.md`, `docs/ARCHITECTURE.md`, and `docs/API.md` with
  the separation contract (S1–S7) and this plan's status.
- Exit gate: scanner runs in CI; baseline counts recorded (38 `GetTheme*`
  calls; props leaks enumerated in section 3.2).

### M1 — Proposal A for the input and text families

- Delete `TextInputStyle` from `TextInputProps`, `TextFieldProps`,
  `TextAreaProps`, `ReadonlyTextBoxProps`; add tone/size semantics; write
  `Default<Widget>Style` input policy in `runtime/text_input.kry`.
- Finish Text roles so `TextProps.color`/`.border` can go; migrate examples.
- Replace input-family `GetTheme*` paint assembly with resolved `StyleData`.
- Exit gate: parity green; `make test` green; input capture boards in light
  and dark; scanner count for these families drops to zero.

### M2 — Proposal B (registry) then finish A

- Land `runtime/style_registry.kry`, host APIs, and `k2c`/`k2cpp`/`k2go`
  lowering for top-level `Style` declarations.
- Add `style` name reference to props; migrate examples; then remove the
  inline `ControlStyle` field from `ButtonProps` and every other props
  struct. Delete alpha-zero fallback patterns in widget `.c` code.
- Port row/page/href color props to semantic tones (section 3.2 items).
- Exit gate: scanner S1 baseline reaches zero exemptions; full capture board
  sweep (`make dropdown-capture` pattern extended per family); C/Go parity.

### M3 — Proposal C (cascade) engine and tools

- Land `runtime/style_cascade.kry`, `Style` child nodes, `class` identity
  field, scope stack in hosts, retained resolved-`StyleData` storage, KRB
  rule-table serialization, and `kryon-preview` inspector support.
- Exit gate: cascade determinism unit tests; cascade-heavy parity board;
  golden captures; `termi` degradation board; hot-reload rule-edit frames.

### M4 — Proposal E (theme class scopes)

- Layered theme scopes with closed vocabulary, `@ref` indirection, metric
  keys; port shipped themes; user-theme compatibility through aggregates.
- Exit gate: theme round-trip tests; per-family restyle captures; theme
  picker works unchanged.

### M5 — Close out

- Flip the scanner to fully failing (zero exemptions); document the final
  surface in `docs/WIDGET_STYLING.md`, `docs/API.md`, and the widget catalog;
  decide Proposal D from real preview workflow evidence, not speculation.
- Exit gate: S1–S7 hold; `make release-preflight` green.

## 13. Testing and enforcement

| Gate | What it proves | Where |
|---|---|---|
| `scripts/check-style-separation.py` | S1 (no raw visual fields in props) and S5 (no theme-getter paint assembly) | `make test`, CI |
| Generated-output scanners | `k2c`/`k2go`/`k2cpp`/`k2js` emit clean style surfaces, no stale names | existing scanner targets |
| C/Go scripted parity | identical resolved frames and merges under identical input | existing parity harness |
| Capture boards | every family x tone x emphasis x state in light and dark | `make <family>-capture`, golden images |
| Cascade determinism tests | specificity order, source order, scope shadowing, token indirection | unit tests over `runtime/style_cascade.kry` |
| Hot reload | rule and theme edits restyle without recompile | `kryon-preview` scripted frames |
| Backend degradation | `termi` cells, `canvas`, `dom`, `null` smoke the same rule tables | per-backend test targets |
| KRB round trip | serialized rule tables resolve identically | `cmd/k2b` + renderer tests |

## 14. Non-goals

- Not implementing web CSS: no units, percentages, cascading inheritance of
  layout, arbitrary selectors, `!important`, or media queries. Light/dark and
  density live in the theme.
- Not replacing the theme system — tokens remain the source of all values.
- Not styling 2D scene content: `Sprite`/`Light2D` tints are content, not
  chrome; they stay in their props.
- Not removing raw colors from backend drawing primitives (`Rect`, `Circle`,
  `Line`, ...). They are native backend surface; app UI consumes `Surface`
  with resolved `StyleData`, and generated app code must not hand-pick colors
  for maintained UI.
- No per-widget animation definitions; motion stays on the shared transition
  tracks.

## 15. Risks and open questions

| Risk | Mitigation |
|---|---|
| Downstream breakage when props lose visual fields | Stage removals after B lands; one pointer bump per app; examples migrate first |
| Cascade scope creep toward real CSS | Grammar frozen in this document; additions require a language version bump and parity in the same change |
| C/Go divergence in rule handling | One `.kry` engine; both hosts consume its output; parity board is a release gate |
| Immediate-mode scope-stack mistakes in hosts | Scope push/pop emitted by the same code generators; unit tests around container re-entry and early exit |
| Vocabulary growth pressure (A's release valve) | Every new tone/role needs a runtime policy value and capture board in the same change |
| Theme INI format growth (E) | Versioned keys, closed whitelist, loader rejects unknown names |
| Name collisions for `#id` rules | Uniqueness checked per scope at compile time in `k2c`/`k2go` |

Open questions:

1. Should `class` accept multiple classes (`.a .b`) or exactly one? Multiple
   classes are CSS-familiar but complicate ordering; single class plus
   `style = "name"` may cover real cases. Decide with M3 examples.
2. Do cascade rules need a `Text` role shortcut (`Text.caption` in D's
   example) as a class-like target, or is `class` enough?
3. Should Proposal E's theme class scopes also feed the terminal backend's
   16-color mapping table explicitly?
4. Does `ButtonStateFocus` need a rule state slot distinct from the existing
   focus motion channel, or is the existing `focused` slot sufficient for
   rules?

## Appendix — Property vocabulary

The closed property set every proposal shares (from
`runtime/control_props.kry`; values are theme tokens or literals):

| Property | Type | Notes |
|---|---|---|
| `background`, `background_end` | color | gradient pair; end is optional |
| `foreground` | color | label, icon, and content ink |
| `border`, `focus` | color | `focus` feeds the shared focus track |
| `radius`, `border_width`, `opacity` | number | |
| `padding_x`, `padding_y`, `gap` | number | spacing tokens preferred |
| `font_size`, `icon_size` | number | type/icon scale tokens preferred |
| `content_offset` | vec2 | |
| `material` | enum | `flat`, `lightfield`, `glass` |
| `typeface` | string | registered typeface name |
| `tone` | enum | neutral, accent, danger, success, warning, info, link |
| `emphasis` | enum | filled, soft, outline, ghost, link |
| `size` | enum | small, medium, large |
| `role` | enum | text roles (title, body, caption, ...) |

Rule state suffixes map to the seven `ControlStyle` slots: `normal` (default),
`hover`, `pressed`, `focused`, `selected`, `disabled`, `loading`.











# LLM-authoring audit: Kryon UI and the CSS package (2026-11-22)

Read-only audit of how hard it is for an LLM to build a good-looking Kryon app
fast. Paths are relative to /mnt/storage/Projects/kryonlabs. Status legend:
`supported` = styles native widgets; `web-only` = parsed but ignored by native
hosts (kept for the DOM export); `unsupported-count` = parsed but flagged in
`sheet.motion_unsupported`; `missing` = hard parse error, whole sheet refused.
A second helper covers the Ziran language; not covered here.

## 1. CSS coverage

| Group | Item | Status | Proof |
| --- | --- | --- | --- |
| selectors | type (`Button`,`Row`,`Grid`,`App`,`*`), one `.class`, one `#id` (`props.id = StyleId(...)`) | supported | packages/css/src/parser/selectors.zi:81-92,101-109,146-155; packages/css/README.md:53 |
| selectors | descendant, `>` child, `+`/`~` sibling combinators | missing (error) | packages/css/src/parser/selectors.zi:183-186 |
| selectors | attribute `[x=y]` for tone/emphasis/size/role/state only, `=` only | supported | packages/css/src/parser/selectors.zi:46-85 |
| selectors | any other attribute or `~=`/`*=` operator | missing (error) | packages/css/src/parser/selectors.zi:56-58,79-82 |
| selectors | `:hover :active :focus :focus-visible :disabled :checked` (+ `:selected` `:loading`) | supported | packages/css/src/parser/selectors.zi:12-24 |
| selectors | `:first-child :last-child :nth-child() :not() :is() :where() :has()` | missing (error) | packages/css/src/parser/selectors.zi:137-139,161-165 |
| selectors | `::before ::after ::placeholder` (any pseudo-element) | missing (error) | packages/css/src/parser/selectors.zi:131-133 |
| selectors | specificity (layer, then type 1 / attr 10 / class 20 / id 200, order; states add nothing); cascade last-wins; comma lists max 8 | supported (custom model) | packages/css/README.md:174-178,108-110; packages/css/src/parser/selectors.zi:199-201 |
| selectors | `!important` | missing (error) | packages/css/README.md:150; packages/css/src/parser/properties.zi:728-730 |
| selectors | inheritance: none; `em`/`%` rejected (no inherited font size) | documented limit | packages/css/README.md:119-120 |
| at-rules | `@media`: prefers-color-scheme, prefers-contrast, pointer, any-pointer, hover, prefers-reduced-motion, `-kryon-theme/-density/-pointer/-platform` | supported | packages/css/src/parser/media.zi:2-4,67-93; packages/css/README.md:145-149 |
| at-rules | `@media` width/min-width/max-width native subset (px, rem, unitless zero) | supported | packages/css/README.md:147,159-164; docs/ui-map.json:43 |
| at-rules | `@media` height/min-height/max-height | web-only | packages/css/src/parser/media.zi:31 |
| at-rules | `@keyframes` (from/to/percent, comma offsets) | supported | packages/css/src/parser/statements.zi:216-246; packages/css/README.md:232-234 |
| at-rules | `@import` (resolver callback, must precede rules) and `@layer` (max 12, flat) | supported | packages/css/src/parser/statements.zi:130-131,41-66,89; packages/css/README.md:46-47,145 |
| at-rules | `@container` `@font-face` `@supports` | missing (error) | packages/css/src/parser/statements.zi:249 |
| at-rules | nesting (`&`) | missing (error) | packages/css/README.md:57-58 |
| values | `var(--name)` / `var(--name, fallback)`; `:root` custom properties only | supported | packages/css/src/parser/values.zi:61-86; packages/css/src/parser/properties.zi:715-718 |
| values | nested `var()` in fallbacks | missing (error) | packages/css/src/parser/values.zi:482-483,551-552 |
| values | `calc()` `min()` `max()` `clamp()` as value functions | missing (error); sole exception `min(LENGTH,100%)` in native grid `minmax` | packages/css/src/parser/properties.zi:443-455; packages/css/README.md:95 |
| values | units px, rem (16px), s, ms; `%` for opacity/channels/line-height/keyframe offsets | supported | packages/css/README.md:115,63,112; packages/css/src/parser/motion.zi:340 |
| values | `em`, `%` lengths, `vw`, `vh`, `ch` | missing (error for em/%; others unknown property/value) | packages/css/README.md:119-120 |
| values | colors: hex, named (147), `rgb()/rgba()/hsl()/hsla()/hwb()`, `color-mix(in srgb, literals)`, `transparent` | supported | packages/css/src/parser/values.zi:178,184; packages/css/README.md:114,123-129 |
| values | `oklch()`, `currentColor`, custom-property color stops | missing | packages/css/README.md:132-133 |
| layout | `display: flex/grid/block/inline/inline-flex/none` | web-only | packages/css/src/parser/properties.zi:503; packages/css/README.md:74-76 |
| layout | flex-direction, flex-wrap, `flex`, justify-content, align-items, align-self | web-only | packages/css/src/parser/properties.zi:503-504 |
| layout | align-content, `order`, flex-grow/shrink/basis | missing (error) | absent from properties.zi:498-505; error path properties.zi:721 |
| layout | native flex layout | widget code only: `BeginFlexCursor`/`FlexStep` + JustifyContent/AlignItems enums | docs/HTML_TO_KRYON.md:112-115 |
| layout | gap / row-gap / column-gap (px, rem, `normal`) | supported | packages/css/README.md:79-83 |
| layout | grid-template-columns: `1fr`, `minmax(0,1fr)`, `repeat(N,...)`, `repeat(auto-fit,minmax(...))` | supported | packages/css/src/parser/properties.zi:437-490; packages/css/README.md:92-97 |
| layout | grid-template-rows, grid-column, grid-row | web-only; native spans via `GridItem.column_span` | packages/css/src/parser/properties.zi:504-505; docs/ui-map.json:44; docs/HTML_TO_KRYON.md:118 |
| layout | grid-area, place-items | missing (error) | absent from properties.zi:498-505; error path properties.zi:721 |
| layout | position relative/absolute/fixed/sticky; top/right/bottom/left/inset | missing (error) | absent from properties.zi:498-505; error path properties.zi:721 |
| layout | z-index | web-only | packages/css/src/parser/properties.zi:505 |
| layout | width/height (px, rem, `auto`, outer size incl. padding) | supported | packages/css/README.md:84-91; packages/css/src/parser/properties.zi:400-417 |
| layout | width/height %, var(), calc(), intrinsic keywords | web-only (native skips) | packages/css/README.md:89-91 |
| layout | min-height / min-block-size | supported | packages/css/README.md:108-112 |
| layout | min-width, max-width, max-height | web-only | packages/css/src/parser/properties.zi:499 |
| layout | aspect-ratio; box-sizing | missing (error) | absent from properties.zi:498-505 |
| layout | overflow hidden/scroll/auto | web-only; per-axis overflow-x/y missing; native scroll is the `Scroll` widget | packages/css/src/parser/properties.zi:505; docs/ui-map.json:41 |
| layout | margin (incl. `auto`, negative) | web-only; native margins via layout APIs | packages/css/src/parser/properties.zi:500-501; docs/HTML_TO_KRYON.md:295 |
| layout | padding + padding-inline/block + per-side names (opposite sides must match) | supported | packages/css/README.md:63-64; packages/css/src/parser/properties.zi:588-592 |
| visual | background-color; `background` color | supported | packages/css/src/parser/properties.zi:539-546 |
| visual | `background: linear-gradient(...)` two stops, vertical only | supported | packages/css/src/parser/properties.zi:295-302; packages/css/README.md:60 |
| visual | `url()` background-image, radial-gradient, conic-gradient, background-size/position/repeat | missing (error) | packages/css/src/parser/properties.zi:345-347; absent from properties.zi:295-302,498-505 |
| visual | border width/style/color; `solid` or `none` only; no per-side border-top-* | supported (shorthand only) | packages/css/README.md:61-62 |
| visual | border-radius (1-4 values, opposite corners must match; single radius field) | supported | packages/css/src/parser/properties.zi:572-574; packages/css/README.md:63-64 |
| visual | box-shadow (multiple, inset, spread) | web-only; native elevation via `-kryon-elevation: none/rest/raised/floating` | packages/css/src/parser/properties.zi:502; packages/css/README.md:71; kryon/src/ui/theme.zi:303-316 |
| visual | outline | only outline-color; outline-width/style missing | packages/css/src/parser/properties.zi:551-554; packages/css/README.md:68 |
| visual | opacity (number or %, clamped 0-1) | supported | packages/css/README.md:63,112 |
| visual | filter (blur/brightness/drop-shadow), backdrop-filter, mix-blend-mode, clip-path, object-fit | missing (error) | absent from properties.zi:498-505; error path properties.zi:721 |
| typography | font-family (fallback list: first family wins, rest silently dropped) | supported (partial) | packages/css/src/parser/properties.zi:372-395 |
| typography | font-size; font-weight 1-1000; font-style (no angle); line-height `normal`/factor/%/length; letter-spacing px/rem | supported | packages/css/README.md:67,100-106,103 |
| typography | text-align, text-decoration, text-transform, white-space | web-only; native via `Text.Props.align` / `.strong` | packages/css/src/parser/properties.zi:501-502; docs/HTML_TO_KRYON.md:43 |
| typography | text-overflow ellipsis, word-break | CSS missing; native via `.wrap = TextWrapEllipsis/TextWrapAnywhere` | docs/HTML_TO_KRYON.md:43 |
| typography | line-clamp, text-shadow, font-variant-numeric | missing (error) | absent from properties.zi:498-505; error path properties.zi:721 |
| motion | transition: property list, duration, delay, `linear/ease*/cubic-bezier()` | supported, one duration/delay/easing per selector, comma list must share timing | packages/css/README.md:222-228 |
| motion | unequal transition timings, `steps()` | unsupported-count (`motion_unsupported`) | packages/css/src/parser/motion.zi:280,287; packages/css/README.md:237-240 |
| motion | animation + `@keyframes` (one named track per selector; repeats, direction, fill modes) | supported | packages/css/README.md:223-228; packages/css/src/parser/statements.zi:216-246 |
| motion | `transform` in a normal rule (and transform-origin, scale, rotate, skew) | missing (error); only `translate: x y` supported | packages/css/src/parser/properties.zi:721; packages/css/README.md:231,239 |
| motion | `transform` inside `@keyframes` | unsupported-count (kept raw for browser export) | packages/css/src/parser/motion.zi:311-312; packages/css/tests/css_motion_behavior.zi:142 |
| interaction | cursor | web-only; native cursor is a host contract | packages/css/src/parser/properties.zi:502; kryon/README.md:71-72 |
| interaction | pointer-events, user-select | missing (error) | absent from properties.zi:498-505 |
| interaction | scroll-behavior, scroll-snap | missing (error); scrolling is the `Scroll` widget | docs/ui-map.json:41 |
| interaction | shipped look presets: classic, lightfield, material packs | supported (opt-in, compiled) | packages/css/README.md:5-7,297-305; packages/css/styles/ |

## 2. Unsupported CSS behavior

Anything unknown fails the whole sheet with one diagnostic: unknown property ->
`CssFailName(p, "unknown property", ...)` (packages/css/src/parser/properties.zi:719-722);
`CssSheetParse` returns false and the app keeps its previous styles
(packages/css/src/css_sheet.zi:394-422; packages/css/README.md:36-44). The same
hard failure applies to unknown widget type (selectors.zi:87), pseudo-elements
and selector functions (selectors.zi:131-139), unknown pseudo-class
(selectors.zi:161-165), unknown at-rule (statements.zi:249), unknown
attribute/value (selectors.zi:79-84), and unknown values for known properties
(e.g. em/% lengths, README.md:119-120).
Ignored declarations are only counted, never listed: `sheet.web_declarations`
increments per web-only declaration (properties.zi:492-496; css_sheet.zi:99) and
`sheet.motion_unsupported` counts rules/keyframes containing browser-only motion
(css_sheet.zi:98,338,371; README.md:237-240). No API returns WHICH declarations
were ignored; the only tool surface is `css_tool export | check | format |
format-check | pack | pack-file | pack-check` (packages/css/tools/css_tool.zi:494)
- there is no lint or ignored-declaration report. `CssSheetDiagnostic` returns
the single failure string (css_sheet.zi:187).

## 3. How an app uses CSS with Kryon today

Smallest path, all runtime (dashboard starter, examples/dashboard/src/styles.zi):
pin and add the package with `ziran add https://git.waozi.xyz/kryonlabs/css.git`
(packages/css/README.md:13-20); embed the sheet in the binary via
`dashboard_css :: #string CSS` (styles.zi:10-162); at startup call
`InstallDefaultTheme(dark)`, `CssSheetParse(*sheet, css, name, CssNoImports)`,
then `CssSheetInstall(*sheet, env, null)` with an environment built from
dark/density/pointer/platform names (styles.zi:172-198; README.md:28-33); each
widget opts in with `.class_name = CSS.StyleClass("kpi-card")` (styles.zi:164-166;
dashboard.zi:210) and `.id = StyleId(...)`; reinstall on dark/density changes and
feed viewport width via `CssEnvironmentWithViewportWidth` + `CssSheetSameMedia`
(packages/css/README.md:159-172).
Compile-time alternative: `css_tool pack-file NAME IN.css OUT.zi` compiles a pack
apps load with `CssSheetLoad` without parsing (README.md:242-243,297-305).
Shipped users: only the dashboard example calls `CssSheetParse`/`CssSheetLoad`
in kryonlabs/kryon (rg: examples/dashboard/src/styles.zi:181,
examples/dashboard/previews/responsive.zi, docs/HTML_TO_KRYON.md); the gallery,
`kryon new` templates and every other example use typed styles with no
stylesheet (templates/app/src/app.zi:1-40); kryonlabs/showcase is metadata JSON
linking external repos (showcase/projects/inner-breeze/project.json:9-15).

## 4. Widget API ergonomics without CSS

Screen A - dashboard starter: src 853 lines + styles.zi 199 lines
(examples/dashboard/src/dashboard.zi, styles.zi). Color/radius/elevation/state
live in the embedded CSS (styles.zi:52-70,83-113,155-161); spacing and layout
are hand-computed `Rectangle` arithmetic (dashboard.zi:217,224,231,301-306).
Screen B - gallery scenes: 573 lines, 41 widget imports
(examples/gallery/src/scenes.zi:2-42); spacing via theme tokens
`Layout.Gap(.ThemeMetricSpace3)` (scenes.zi:169,366); containers measure
children then compute bounds (scenes.zi:168-186); wrapping via `Flow.Draw`
(scenes.zi:453). Most verbose / surprising constructs:
1. Absolute geometry for every widget (dashboard.zi:301-302):
```zi
Text.Draw(session, Text.Props.{.key = 607, .bounds = Rectangle.{c.x, stat_y + 18.0, col_w, 20.0}, .text = lat_str, .size = 14})
Text.Draw(session, Text.Props.{.key = 608, .bounds = Rectangle.{c.x + col_w + 8.0, stat_y, col_w, 16.0}, .text = "Uptime", .size = 11})
```
2. Per-widget style wiring plus synthetic keys (dashboard.zi:208-216):
```zi
card := Card.Draw(session, Card.Props.{
    .key = key, .class_name = ClassId("kpi-card"),
    .bounds = bounds, .variant = .CardOutlined })
Text.Draw(session, Text.Props.{.key = key * 10 + 1, ...})
```
3. Grid items need up-front heights and spans (previews/responsive.zi:97-99):
```zi
items: [3]Grid.GridItem = .[.{.height = 106, .column_span = -1}, .{.height = 86}, .{.height = 86}]
grid := Layout.Grid(session, Grid.GridProps.{.key = 30, .class_name = CSS.StyleClass("charts")}, items[:])
```
4. Measure-then-compose container dance (scenes.zi:169-175):
```zi
gap := Layout.Gap(.ThemeMetricSpace2)
button_size := Button.ButtonMeasure(button)
sizes: [2]Rectangle = .{button_size, button_size}
form := Layout.Draw(session, Layout.Props.{.key = 1, .gap = gap,
    .bounds = Layout.LayoutContentBounds(b, .FlexColumn, sizes[:], gap)})
```
5. Scoped containers: draw children then `End(session)` only when `opened`
(docs/HTML_TO_KRYON.md:18,91-94), plus `gap_override`/`padding_override` flags
to force zero against theme defaults (docs/HTML_TO_KRYON.md:108).
Default app, zero styling: the `kryon new` app template imports no CSS and no
Theme; the desktop host applies the system palette (templates/app/src/app.zi:1-40;
docs/THEMING.md:3-7) and `InstallDefaultTheme(dark)` installs palette, metrics,
fonts, shadows and motion (docs/THEMING.md:9-11). Default tokens: radii
6/8/10/14/18/999 (kryon/src/ui/theme.zi:285-287); spacing 4/8/12/16/20/24
(theme.zi:288-289); elevation shadows rest/raised/floating (theme.zi:303-316);
type scale 11/12/13/15 + headings 36/28/22/17 (theme.zi:293-297); fonts
Inter/Geist/Lucide (theme.zi:273-279); accent #00864b light, #53dca6 dark
(theme.zi:178,203); dark mode via `InstallDefaultTheme`/`SetThemeMode`/
`ThemePolicy.ResolveDark` (docs/THEMING.md:9-17) and `prefers-color-scheme`
(packages/css/README.md:146); extra named palettes need `-kryon-theme` media
(kryon/themes/catalog_light.css:12-23). Verdict: tokenized and consistent, but
flat - no cards, shadows or accent usage until code or CSS adds them.
Ready-made: card, box, avatar, badge, tabs/TabBar with top/bottom/left/right
placements (docs/HTML_TO_KRYON.md:41), NavigationBar bottom nav
(docs/ui-map.json:10), Toolbar/TitleBar app bar (docs/HTML_TO_KRYON.md:39),
Modal/Dialog (docs/HTML_TO_KRYON.md:58), Sheet (src/ui/sheet;
docs/COMPONENTS.md:92-98), Toast (docs/API.md:59-71), Loading/LoadingScreen
(docs/HTML_TO_KRYON.md:63), Toggle switch (docs/ui-map.json:22), Slider
(docs/ui-map.json:26), SvgIcon (docs/HTML_TO_KRYON.md:61), plus command palette,
tooltip, hovercard, carousel, checklist, fieldset, progress, segmented control,
collapsible, breadcrumb. Missing: a first-class chip (dashboard fakes it with a
styled Button, styles.zi:94-113), a rich list-item composite (docs recommend
Scroll+Button rows, docs/HTML_TO_KRYON.md:55), a skeleton placeholder distinct
from the spinner. docs/COMPONENTS.md is ZX component hosting, not a widget
catalog (docs/COMPONENTS.md:3-7); docs/WIDGET_CONFORMANCE.md is test policy
(docs/WIDGET_CONFORMANCE.md:3-11); no widget catalog document exists.

## 5. LLM-facing documentation

docs/API.md is a 2923-line feature log with no compact index (docs/API.md:1-46);
docs/COMPONENTS.md 105 lines is ZX component hosting (docs/COMPONENTS.md:3-7);
docs/HTML_TO_KRYON.md 436 lines is the real porting guide with `ziran api`
discovery (docs/HTML_TO_KRYON.md:23-31,417-436); docs/ui-map.json 46 lines is
the machine-readable mapping (docs/ui-map.json:4-46); README.md 136 lines
(kryon/README.md:50-52); the CSS truth lives in packages/css/README.md:49-178
(353 lines total).
No compact cheat sheet exists; a gallery with screenshots and live editable
previews requires `make site` (Emscripten) (kryon/README.md:87-103); starter
templates plus `kryon new DIR --template app|tui|pages|web` exist
(src/project/cli_linux.zi:322,329,604; templates/app/) but ship no stylesheet.
kryon-start pitfalls (skills/kryon-start/SKILL.md:174-188) and classification:
| Pitfall | Class |
| --- | --- |
| No `ziran.lock`, no build (SKILL.md:175) | fixable in Kryon: templates ship a lockfile or `new` runs lock |
| Never start Xvfb yourself; OpenPrivateDisplay/StartOnDisplay (SKILL.md:176-177) | fixable by a clear diagnostic |
| App built without `KRYON_TEST_DRIVER` exits under the driver (SKILL.md:178) | fixable by a clear diagnostic |
| Frame replay fails without `RecordFrameFailure` capability binding (SKILL.md:179) | fixable by a clear diagnostic |
| Typed text lives in `TextField.State` bytes; stale-state gotchas (SKILL.md:180-182) | inherent; cheat-sheet docs |
| Focus moves after press; re-select role before next `DriverType` (SKILL.md:183-184) | inherent; docs |
| Return views must borrow caller-owned storage (SKILL.md:185-187) | inherent (language rule); compiler diagnostic |
| Text field package is `kryon/text_field` (SKILL.md:188) | fixable by cheat sheet / ui-map |

## 6. Feedback loop for an agent

| Route | Commands | Result | Proof |
| --- | --- | --- | --- |
| `kryon preview` fixture | 1, after registering a `previews/` module in `module_roots` | offscreen Cairo PNG + node JSON, fixed palette, deterministic | docs/FRAME_REPLAY.md:5-15,17-21,37-42 |
| `kryon run shot` (pixmap profile) | 1 | headless fixed 960x600 PPM, byte-identical across C/Go/ZX; with `--png FILE --width W --height H --theme T` any app entry at any size as PNG + node JSON | docs/PROJECTS.md |
| Frame replay + inspect | 3 (capture build, `tools/frame-replay.sh`, `tools/frame-inspect.sh --diff`) | JSON tree/paint commands, no raster pixels | docs/FRAME_REPLAY.md:62-75,104-105 |
| Test driver + private Xvfb | 2-3 (`ziran lock`, `KRYON_TEST_DRIVER=1 ziran build`, run test) | real window driven by label/role | skills/kryon-start/SKILL.md:49-56,123-155 |
| Browser gallery | `make site` (Emscripten) | editable examples with live preview | kryon/README.md:87-103 |

An edit reaches pixels in 1 command once a preview fixture exists, but the
fixture must be a separate `previews/<name>.zi` module (docs/FRAME_REPLAY.md:17-21);
a whole-app screenshot without a fixture is the fixed-size pixmap profile
(docs/PROJECTS.md:166). There is no hot reload or live preview outside the
browser gallery (only a historical Krait IDE note, kryon/CHANGELOG.md:515-518).
No timing numbers are recorded in these docs to cite.

## 7. Open work already planned

- plans/2026-11-18-web-apps.md:11-17: five shipped apps lack web builds, each
  blocked in app code (PTY shell, curl HTTP, gh CLI, XDND, X11 imports).
- plans/2026-11-21-style-codec-size.md:33-40: StyleData hosted codec grown to 34
  fields carrying width/height (done locally, codec + push-check suites passed).
- plans/2026-11-21-style-codec-size.md:41-43: open - publish those hunks to
  master, then update the CSS package and Harmony to the same Kryon identity
  before publishing the native stylesheet milestone.
- kryon/TODO.md:1 and packages/css/TODO.md:1 are bare `# TODO` headers: no
  tracked styling, appearance or ease-of-use items anywhere.

## Top gaps

1. No flex/grid in CSS: `display/flex/justify-*` parsed but ignored, so layout can never be CSS - evidence properties.zi:498-505 + ui-map.json:38-41 - fix: resolve CSS layout declarations onto Layout.Row/Column/Grid at install, or loudly diagnose them - files: packages/css/src/parser/properties.zi, packages/css/src/css_sheet.zi, kryon/src/ui/style.zi - test: a sheet with `Row{gap:12}` + `display:flex` changes the dashboard grid digest and driver node bounds.
2. Ignored declarations are invisible: two counters but no listing - evidence properties.zi:492-505, css_sheet.zi:98-99, css_tool.zi:494 - fix: `css_tool check` prints every web-only/ignored declaration with line:col - files: packages/css/src/css_sheet.zi, tools/css_tool.zi - test: check on a margin+box-shadow sheet lists both names.
3. One unknown property kills the whole sheet - evidence properties.zi:719-722, README.md:36-37 - fix: collect up to N diagnostics and add a lenient parse mode - files: packages/css/src/parser/properties.zi, src/css_sheet.zi - test: a sheet with two typos returns both diagnostics; lenient mode installs the rest.
4. Combinators and nesting rejected, so every web-derived stylesheet needs manual flattening - evidence selectors.zi:183-186, README.md:57-58 - fix: accept and flatten descendant/child combinators into the existing flat rule model - files: packages/css/src/parser/selectors.zi, src/css_sheet.zi - test: `.card .title { color }` matches a Text.title inside a Card class in css_parse_behavior.
5. No calc()/min()/clamp() and no % lengths natively - evidence README.md:89-95,119-120 - fix: evaluate these functions over px/rem at install time - files: packages/css/src/parser/values.zi, src/css_sheet.zi - test: `padding: min(16px, 4rem)` and `width: calc(100% - 32px)` parse and install.
6. Hand-computed Rectangle math dominates app code (853-line dashboard) - evidence dashboard.zi:206-244,301-306 - fix: make measured auto-stacking Layout the documented default and port examples to it - files: kryon/src/ui/layout*, docs/HTML_TO_KRYON.md - test: rewrite DrawKpiCard with Layout.Draw; frame-inspect node bounds unchanged.
7. Grid items require up-front heights/spans - evidence docs/HTML_TO_KRYON.md:117-118,148, responsive.zi:97 - fix: auto row heights from measured items when `.height` is unset - files: kryon/src/ui/layout (grid), examples/dashboard/previews - test: a responsive.zi variant with heightless items renders equal bounds.
8. The CSS supported-property matrix lives only in packages/css/README.md:49-178 (the one-page app cheat sheet landed as docs/CHEATSHEET.md, built by `make cheatsheet-test`) - fix: a generated property matrix linked from docs/CHEATSHEET.md - files: packages/css/README.md, kryon/docs/CHEATSHEET.md - test: the matrix matches the parser property list.
9. Theme tokens and CSS do not share a vocabulary - evidence theme.zi:285-298 vs the properties.zi list, themes/catalog_light.css:12-23 - fix: expose `--space-2`, `--radius-medium`, `--accent` etc. as CSS custom properties at install - files: packages/css/src/css_sheet.zi, kryon/src/ui/theme.zi - test: dashboard styles.zi uses var(--space-3); conformance digest unchanged.
11. No hot reload / live native preview - evidence README.md:95-103 (browser only), no watch in docs - fix: `kryon run --watch` rebuild+relaunch on source change - files: kryon/src/project/cli_linux.zi, ziran run - test: edit a label with the window open; text updates within one rebuild.
12. font-family fallback lists silently truncate to the first family - evidence properties.zi:372-395 - fix: keep the list, resolve the first installed family, warn when none match - files: packages/css/src/parser/properties.zi, kryon/src/ui/text_typeface.zi - test: a sheet with `"Geist", Inter` uses Inter when Geist is absent and reports the unresolved family.

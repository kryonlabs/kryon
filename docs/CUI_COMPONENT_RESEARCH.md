# CUI-to-Kryon Component Parity Research

This is research only. It does not change widget implementations or declare CUI as Kryon's default look. The complete family-level deliverable is the 127-row matrix in [`docs/CUI_COMPONENT_PARITY.tsv`](CUI_COMPONENT_PARITY.tsv); this file records evidence, architectural contracts, priorities and cross-cutting acceptance criteria.

## What is CUI, what is claimed, and what was measured?

### Takeaway

CUI is a compact C99 immediate-mode desktop UI library with an unusually broad documented component catalog, an MIT license, and embedded third-party assets. Its public claim of “about 170 components” does not equal the catalog's 127 documented component families or 289 distinct referenced APIs; both numbers are useful but measure different things.

### Cited Findings

- **Claimed scope and size:** CUI describes “About 170 components with spring motion and full keyboard support,” Windows/macOS/Linux from one C99 codebase, and a no-dependency core around 240 KB. — [CUI README, stable commit 3d60199](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/README.md#L12-L16)
- **Claimed binaries and performance:** the README reports `hello` around 240 KB, the full library around 930 KB, and full-screen build/layout/paint at 0.09–0.75 ms, most under 0.25 ms. — [CUI performance table](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/README.md#L315-L321)
- **Claimed platforms:** Win32, Cocoa, X11, and SDL2/GLFW for Wayland or others, with OpenGL or software rendering. Android is not listed. — [CUI platform table](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/README.md#L331-L337)
- **Claimed limitations:** no accessibility tree; optional, limited Arabic-script shaping/bidi; inline IME shows committed text but not inline composition. — [CUI stated limitations](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/README.md#L341-L345)
- **Catalog measurement:** the generated catalog at this commit contains **14 categories, 127 component families, 343 API references, 289 unique API names, 148 option groups, 811 option fields, 121 keyboard notes, 138 family scene references, 156 top-level scene entries, 240 shots and 6 unlisted entries**. This was measured from [`build/scratch/cui/catalog.json`](../build/scratch/cui/catalog.json) and the extracted [`build/scratch/cui/item-api-map.tsv`](../build/scratch/cui/item-api-map.tsv); it is not a CUI-provided component count.
- **Catalog discrepancy:** the GitHub repository description still says about 150 components, while the README and live site say about 170. No generated counter defining “component” was found; 127 is a **documented family measurement**, not a refutation of every API-level counting method. — [CUI repository metadata](https://github.com/yappologistic/CUI/tree/3d60199b253c882da982a56e7cbeae4ce7e2682a) and [component claim](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/README.md#L12-L13)
- **Canonical Kryon inventory measurement:** at Kryon commit `a139f22c5eb3c5b97994a727cfca94ab63104d79`, `Widgets/module.zi` has 100 public `using` imports, `WidgetKind` has 56 concrete kinds before the count sentinel, `src/ui` contains 275 `.zi` files, and `src/ui/modules.txt` compiles 225 modules. Many of those files are internals rather than parallel widget APIs. — [Kryon Widgets module](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/ui/Widgets/module.zi#L1-L140), [WidgetKind](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/ui/widget_kind.zi#L2-L59)
- **Working-tree caveat:** Kryon already had many unrelated modified and untracked source/test files before this research. This report adds only research documents and does not interpret those active changes as completed or canonical behavior.

### Inferences

- Kryon should use “127 documented families / 289 APIs” for exhaustive parity work, while retaining CUI's “~170 components” as a claim. A family can contain several APIs and visual variants.
- API-count parity alone would be misleading. CUI parity requires props, result events, retained state, pointer/keyboard behavior, styling, motion, accessibility and host capability contracts.
- CUI performance numbers are credible upstream benchmark claims but were **not rerun here**. They must not be presented as Kryon measurements.

### Gaps

- No authoritative CUI definition of “component” was found, so an exact reconciliation from 127 families to the claimed ~170 is impossible without choosing a counting rule.
- No CUI Android implementation exists to compare; Android work is a Kryon portability requirement, not CUI parity.

## What exact parity gaps and disjoint implementation contracts emerge?

### Takeaway

Kryon already has strong foundations for text, images, buttons, fields, basic controls, menus, modals, scroll/layout, tabs, trees/tables, reorder and swipe behavior. The dominant gaps are specialized inputs, picker surfaces, overlays, editable data grids/editors, chart breadth, planning surfaces, social/media surfaces, motion effects and AI components. The complete status, exact gap, target file/API and acceptance test for every family are in [`docs/CUI_COMPONENT_PARITY.tsv`](CUI_COMPONENT_PARITY.tsv).

### Cited Findings

- **Strong or near-existing families include:** text, image, button/icon button, split button, swipe actions, text field/area, segmented control, checkbox/toggle/radio, slider, select, color/time picker, tabs, toolbar, document tabs, dropdown/context menu, popover, dialog, toast, progress/loading, table/list/tree, accordion, reorder, Kanban, candlestick charts and carousel. Their current surfaces and missing behavior are enumerated row-by-row in the matrix. — [`docs/CUI_COMPONENT_PARITY.tsv`](CUI_COMPONENT_PARITY.tsv)
- **CUI documents each family with usage, options and keyboard behavior across the two component guides.** — [CUI components](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/docs/components.md#L1-L70) and [more components](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/docs/components-more.md#L1-L80)
- **Kryon's ownership rule requires one public widget concept and no wrappers or parallel legacy APIs**, so implementation must extend existing canonical APIs or replace them rather than add CUI facades. — [Kryon repository rules](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/AGENTS.md#L10-L49)
- **Linked package coverage:** `css` parses selectors/custom properties/layers/media/keyframes and compiles style packs; `plot` supplies retained lines/bars/markers/axes plus OHLC, volume, SMA/EMA and band charts; `data-views` supplies TableView, TreeView and Kanban with virtualization/keyboard selection and stable IDs. — [CSS README](https://github.com/kryonlabs/css/blob/6aad71c34fffec40ec903cc8f9d5c95f02572d35/README.md#L1-L14), [Plot README](https://github.com/kryonlabs/plot/blob/3f888653b6b97010163d824c4cc72476be3214f3/README.md#L1-L10), [DataViews README](https://github.com/kryonlabs/data-views/blob/942fea7809063407e2b4fa9611a2247ab6b3b894/README.md#L1-L29)
- **Package limitations:** Plot does not cover CUI's full chart set; DataViews does not implement CUI's editable data grid or tree table; neither is currently required by Kryon's manifest for every app. The matrix marks these as `package-promote` or `partial-package` rather than pretending dependency presence is parity. — [`docs/CUI_COMPONENT_PARITY.tsv`](CUI_COMPONENT_PARITY.tsv)

### Contract Rules

1. **One canonical implementation.** A row marked `extend` modifies the existing props/result/state module. A row marked `new` creates one family module under `src/ui/`. Do not add a CUI wrapper beside a Kryon widget.
2. **Standard call shape.** New family APIs use `Family(session, props) -> FamilyResult` or `Family(session, props, state) -> FamilyResult` when state must survive frame boundaries. Borrowed data stays caller-owned; results identify changed data by stable IDs/paths and never take ownership.
3. **Explicit event/state split.** Props describe input and presentation; results describe pointer/keyboard mutations; state contains only retained public state. Business actions stay in the application.
4. **No hidden host behavior.** Widget behavior/layout/styling/accessibility stays in `src/ui/**/*.zi`. Hosts only provide input, files, fonts, images, clipboard, persistence or raster surface capabilities.
5. **Independent module boundaries.** Each matrix family can be compiled and exercised independently, making a family small enough to package as a `.zx` component without dragging unrelated widget code.
6. **Replace rather than preserve legacy.** Breaking changes are allowed. Where a promoted DataViews/Plot implementation becomes canonical, retire the parallel package-facing API rather than maintaining two implementations.

### Prioritized Disjoint Task Groups

1. **Foundations and safety:** CUI token/theme pack, style continuity, component identity/state migration contract, accessibility semantic contract, and portable input/result codec.
2. **Basics/actions/inputs:** text and icon parity; badge/avatar/keycap; metric; button lifecycle variants; specialized text, numeric, tag, OTP, shortcut, file and signature inputs.
3. **Selection/pickers/navigation/overlays:** toggle groups, chips/rating/radio cards; date/range/wheel/ruler/knob/emoji pickers; sidebar, menu bar, status bar, docking; popover/tooltip/sheet/palette/hover/tour.
4. **Feedback/data:** alert/banner/checklist/undo; promote table/tree/Kanban; implement data grid, tree table/path bar, file browser and all editor/viewer surfaces.
5. **Charts/planning/social/motion/AI:** chart core plus missing chart families; calendars/Gantt/node editor; chat/media/motion; AI composition surfaces.
6. **Cross-host integration only after family behavior is nonvisual-complete:** Android, Canvas2D/DOM and desktop input/file/raster capability adapters and golden suites.

### Gaps

- Exact visual golden tolerances and glyph metrics cannot be finalized until Kryon embeds or selects the matching Inter/Geist/Geist Mono/Lucide assets. The source-derived token and font requirements below are sufficient for contract design.
- The renderer/VM strategy is intentionally outside this component report and belongs to the separate rendering/dependency audit.

## What style, platform, licensing, state and validation contract is required?

### Takeaway

A 1:1 default look needs a distinct versioned CUI-compatible semantic token pack, exact Inter/Geist typography, Lucide icons, OKLab theme interpolation, CUI radii/heights/shadows/motion, and website-chrome tokens kept separate from widget defaults. Kryon already has all three requested host families, including Canvas2D without WebGL, but live module replacement needs a new explicit public state/identity boundary.

### Cited Findings

- **License:** CUI code is MIT. — [LICENSE](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/LICENSE#L1-L3)
- **Embedded licenses:** Inter and Geist are SIL OFL 1.1, Lucide is ISC, generated Unicode 17 data is Unicode License v3, and stb helpers are public domain or MIT. — [THIRD_PARTY.md](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/THIRD_PARTY.md#L5-L13)
- **Widget theme colors:** CUI uses neutral light/dark backgrounds, surfaces, foreground/secondary/muted text, border/subtle/strong, success/warning/danger/info, accent variants, control on/glyph/track/thumb, hover/active/scrim/focus and six rotated series colors. The exact light/dark values and eight accents are in `src/cui_theme.c`. — [semantic colors](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/src/cui_theme.c#L128-L234) and [accent table](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/src/cui_theme.c#L106-L124)
- **Dimensions and type:** controls use radii 10/14/18, heights 30/36/44, text sizes 11/12/13/15 and headings 36/28/22/17. — [CUI theme scale](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/src/cui_theme.c#L236-L248)
- **Shadows and motion:** light resting/raised/floating shadows and darker variants are explicit; reduced motion zeroes the multiplier; theme changes interpolate all colors over 0.32 seconds with ease-out in OKLab. — [shadows/scales](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/src/cui_theme.c#L250-L266) and [theme transition](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/src/cui_theme.c#L268-L329)
- **Typography:** Inter Regular/Medium for body/control, Geist Medium for headings 28+, Geist Mono for code, Lucide for icons. — [CUI theming docs](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/docs/theming.md#L75-L96)
- **Website agreement:** the live site says its colors are read from the library and confirms control/item/panel/surface radii 10/8/14/18. Its light/dark CSS values match the semantic neutral palette. The site's own 14px body, 56px topbar and 264px sidebar are documentation-chrome values, not widget theme defaults. — [site token agreement](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/web/style.css#L1-L58) and [site body/chrome](https://github.com/yappologistic/CUI/blob/3d60199b253c882da982a56e7cbeae4ce7e2682a/web/style.css#L91-L123)
- **Kryon current metrics differ materially:** radii 4/12/28/999, heights 32/40/48 and font sizes 14/16/22. — [Kryon Metrics](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/ui/theme.zi#L257-L300)
- **Kryon style engine already has selector facts and style fields** for kind/name/class/role/tone/emphasis/size/state/validation/orientation/placement and colors, radius, borders, opacity, spacing, font, icons, gradients, material and letter spacing. It lacks the complete CUI semantic token pack, multi-layer elevation tokens, motion duration/multiplier tokens, series rotation, OKLab transition and font-role tokens as first-class installed theme state. — [selector facts](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/ui/style_sheet.zi#L340-L378) and [style merge fields](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/ui/style.zi#L47-L190)
- **Host targets:** Kryon's desktop backend is SDL2+Cairo, `raylib` web is WebGL1, and `canvas` is Canvas2D; DOM is a semantic layer over Canvas2D. Android builds use NativeActivity, Android viewport/input policy and shared raylib host. These satisfy desktop, web-without-WebGL and Android targets without making WebGL a component dependency. — [backend table](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/docs/BACKENDS.md#L62-L92) and [Android host](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/docs/BACKENDS.md#L545-L578)
- **Current retained identity:** a Kryon node survives only when parent key, widget key and widget kind match. Built-in node state is only `open`, `index`, `offset` and `focus_generation`. — [identity](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/ui/tree.zi#L24-L31) and [WidgetState](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/ui/tree.zi#L240-L247)
- **Current `.zx` player retention:** the portable player owns the window; widgets/layout/state execute in the bundle instance; bundle globals/session persist while the same instance runs. No live module-instance replacement or state migration API was found. — [zx player](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/backend/zx_run.zi#L1-L48) and [reusable player](https://github.com/kryonlabs/kryon/blob/a139f22c5eb3c5b97994a727cfca94ab63104d79/src/backend/bundle_player.zi#L1-L8)

### Required Style Contract

- Add a versioned **CUI-compatible style pack**, not silently replace Kryon Classic unless the owner later chooses CUI as the global default.
- Token groups: backgrounds/surfaces; foreground/text; borders; status; accents; control on/glyph/track/thumb; hover/active/scrim/focus; six series; neutral label tones; radius control/item/panel/surface/small; control heights; text and heading scales; font roles; three two-layer shadow groups; motion durations and multiplier.
- Preserve user-customized non-color metrics when switching mode/accent, matching CUI behavior.
- Implement exact OKLab interpolation or a precomputed equivalent for the 0.32-second theme transition; reduced motion snaps color state.
- Keep website chrome tokens (`14px` site body, topbar/sidebar dimensions, bezel/code palette) outside widget defaults while deriving shared semantic values from the same source.
- Package full CSS parsing as optional `kryonlabs/css`; core needs only compiled CUI token/style tables and runtime resolution so components can render without a Git CSS dependency.
- Add license manifests for any embedded Inter/Geist/Lucide/Unicode data. Prefer existing Kryon assets for Classic; use CUI-compatible assets only in the CUI pack.

### Required Portable Component / Hot-Swap Contract

1. **Stable external identity:** host/component identity is `(owner key, stable component key, public component kind)` and remains constant across implementation replacement. Internal child keys may change without losing public state.
2. **Versioned public state:** every component exposes a serializable state schema/version. Public state includes editing cursor/selection/IME, scroll, focus generation, open popup, active item, drag operation, undo stack and animation-neutral semantic state as applicable.
3. **Migration and rollback:** replacement declares accepted source versions. Successful migration commits before the first replacement frame; failure leaves the previous instance/frame state available and reports a typed error. A failed next frame cannot destroy already committed state.
4. **Private state is disposable:** implementation caches, glyph caches and transient animation phase may reset. Focus, editing value, scroll, open state, public selection and user-visible theme must not.
5. **Caller-owned data:** arrays, files, model data and business actions are borrowed by the component and remain outside the hot-swapped module.
6. **Stable result ABI:** events use stable component/function IDs and typed codecs; result enums and IDs are part of the public contract.
7. **Theme continuity:** installed semantic token generation and user customizations survive replacement.
8. **No renderer special case:** component modules emit Kryon tree/style/semantic commands; Canvas, DOM, Cairo and Android hosts render the same command stream.

### Meaningful Cross-Cutting Acceptance Tests

- **Style exactness:** compare Cairo pixmap, Canvas2D and Android-safe raster frames for every tone/accent/mode, including theme transition first/middle/last frames and reduced motion.
- **Keyboard/focus:** replay the documented CUI key sequence for each family and assert focus, consumed keys, result event and accessibility semantics.
- **State retention:** replace a component implementation between frames and assert focused field caret/selection, scroll, open menu, selected tab/item, drag state, undo state and theme survive or migrate exactly.
- **Failure rollback:** corrupt/missing replacement modules and failing migration must keep the previous committed frame and report the exact failure.
- **Portable bundles:** each family's `.zx` sample runs headless through the pixel player with no renderer-specific imports; required host capabilities are declared explicitly.
- **Host matrix:** nonvisual tree/input tests everywhere; exact raster parity on Cairo, Canvas2D and Android stub; desktop window tests only under private Xvfb/Xephyr.
- **License:** build fails if any embedded asset lacks its matching license file and generated source provenance.

### Inferences

- The safest dependency direction is: core carries a compiled CUI style pack and all canonical component behavior; full CSS remains optional; Plot/DataViews implementations should be promoted only when their family becomes canonical, then removed from the dependency path rather than duplicated.
- CUI's lack of an accessibility tree is not parity to preserve. Kryon should keep and extend its semantic/accessibility model while matching CUI's visual and keyboard behavior.
- MIT permits reference and porting, but Kryon's own 100%-Ziran rule means the implementation should be authored in Ziran rather than copying C source. Embedded assets must follow their separate licenses.

### Gaps

- Exact file-drop, Android file picker, font fallback and clipboard capability adapters need per-family host tests; CUI docs alone do not establish their behavior on Kryon Android/Canvas.
- Unicode emoji data size and whether it belongs in core or an optional generated module is a dependency decision not settled by component behavior.
- No CUI Android baseline exists, so Android acceptance must compare Kryon hosts to Kryon's own expected command/raster output, not to CUI.

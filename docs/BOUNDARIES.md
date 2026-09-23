# Kryon Boundaries

Kryon is a reusable UI runtime. It may contain generic runtime primitives,
generic examples, generic test fixtures, and documentation for integrating the
runtime into downstream applications. It must not contain product behavior,
branding, copy, assets, or fixtures from a downstream application.

The retained UI tree is authored in `.kry` for composition, state, layout,
interaction, style, and capture decisions. Handwritten C provides platform,
memory, and renderer effects called by generated code. Generated C is build
output. Other C widget implementations still need migration.

The Ziran migration includes `zi/geometry.zi`, `zi/layout.zi`, and
`zi/group.zi` for portable geometry and layout, plus `zi/widget_kind.zi`,
`zi/accessibility_props.zi`, and `zi/accessibility_policy.zi` for widget-kind
and accessibility decisions. `zi/drag_drop.zi` provides the drag and drop
source and target decisions. `zi/theme.zi` provides theme policy, palette,
scheme, and metrics. `zi/popup_policy.zi` provides popup lifecycle decisions
using `zi/geometry.zi` for rectangle and point values. `zi/popup_ownership.zi`
provides popup order, ancestry, capture, retirement, and focus decisions.
`zi/transition.zi` provides transition timing and fade decisions. These are
tested as ordinary imported modules.
Current widgets still consume the corresponding `.kry` policy until the Ziran
library cutover.

Secondary-window placement, drag movement thresholds, and reachable work-area
bounds are authored in `src/ui/window_policy.kry`. The native window adapter
collects OS events, moves windows, and presents their rendered content.
Active theme and theme-family selection state is authored in
`src/ui/theme_state.kry`; platform theme detection remains a host service.
Node initialization and typed property values are authored in
`src/ui/node.kry`, with the existing C declarations serving native callers.
Route list/stack operations and app-shell measurement are authored in
`src/ui/app_shell_route.kry` and `src/ui/app_shell_layout.kry`.
Application screen selection, route parsing, and host callback dispatch are
authored in `src/ui/screen_routes.kry`; app drawing callbacks remain supplied by
their application.
Theme and orientation preference decisions are authored in
`src/ui/preference_policy.kry`; host callbacks and platform functions apply
the selected orientation.
Frame lifecycle, post-frame callback scheduling, and active/idle frame pacing
are authored in `src/ui/frame_lifecycle.kry` and `src/ui/frame_pacing.kry`.
Platform drawing and swap remain backend effects.
Built-in locale fallback labels for theme controls are authored in
`src/ui/locale_defaults.kry`. Catalog entry and language-list parsing are
authored in `src/ui/locale_parser.kry`; native locale storage loads catalogs
and keeps their allocated entries. Locale-code selection and normalization,
default English registration, and catalog fallback lookup are authored in
`src/ui/locale_policy.kry`; the host
collects the OS language preferences.
Capability naming and safe-content geometry are authored in
`src/ui/capability_layout.kry`.

Linux libdraw key releases, modifier samples and focus events are backend host
translation. The private X11 observer listens only to the runtime's own window;
it does not implement desktop commands, selection policy or file operations.
Those remain application behavior. See [libdraw input](libdraw-input.md).

KSS grammar, environment-name interpretation, overlay decisions, cascade policy,
and formatting belong to maintained `runtime/*.kry` modules. CSS declaration
classification, scalar parsing, token lookup, and color overrides now run there
as well. Native import storage, parse driving, pack registration, theme replay,
and built-in pack selection live in `src/ui/*.kry`; generated C is a build
artifact. Specificity weights and priority comparisons are shared with web
resolution and inspector traces. Attribute operators and nth-position formulas
also run in `.kry`, as do state predicates and basic structural pseudo rules;
hosts provide values, sibling lists, and relationship/content/focus facts.
Main selector tokenization and atom parsing also live in `.kry`; the web adapter
builds its selector objects from borrowed spans and tagged atoms. CSS export and
structural matching also use those atoms to read functional pseudo arguments;
they do not maintain separate regular-expression argument grammars. Remaining
compound matching, specificity conformance, and CSS export decisions are tracked
in the completion ledger. `KssPseudoMatch` owns structural/positional dispatch,
including sibling-axis and direction selection and unknown-form rejection. `KssSelectorGroupMatches` owns positive
and negative selector-list reduction; web storage retains separate groups and
supplies their alternative-match counts. `KssIdentityMatches` owns ID/name/key
comparison. The web adapter retains ordered ID and attribute conditions, rather
than letting storage keyed by attribute name discard repeated constraints.
`KssSelectorChainStep` owns chain traversal, retry, and acceptance decisions.
Hosts retain node references and a dynamic frame stack, report actual parent
and previous-sibling relationships, and evaluate requested simple selectors.
Relative `:has` matching uses the same driver with a subject anchor; hosts
provide frame candidates without selecting the relationship axis themselves.
Relative-prefix parsing and nested-`:has` rejection also belong to `.kry`.

Host
adapters own source/import storage, native string views, and publication of
resolved sheets; they call generated code to interpret those sources. The Go pack
registry retains source and variant identity for theme changes, staging all
resolved sheets before publishing them. Browser DOM text-node creation and attachment remain host
services; they preserve the authored text alongside mounted child elements.

Positional formula validation and canonical CSS text generation belong to
`KssParseNth` and `KssNthText` in `.kry`. Hosts consume their results rather
than sanitizing formula text. All KSS language decisions, including remaining
CSS conversion, diagnostics, and formatting, are intended to live in `.kry`;
platform adapters supply storage, I/O, DOM observations, and output sinks.

The CSS property allowlist and aliases use `KssCSSPropertyName`; numeric units
and border shorthand classification use generated decisions too. Browser code
must not restore its own property map, unitless list, or whitespace classifier.
Ordinary-rule and keyframe declaration/effect decisions now come from
`KssCSSExpandDeclaration` and `KssCSSEffectAt`; the host joins their fragments
and writes CSS. Inline application consumes that same stream; it has no independent property
map, composite effect builder, vendor-fallback list, or border-style policy.
`KssCSSBorderDefault` owns the authored-border default, and effect facts include
explicit background-image presence. Selector serialization and other remaining
CSS decisions still need migration; these helpers do not close all KSS work.

Generated browser modules (`web/instance.js`, control/drawing props, editor,
style/surface policy, and KSS parser/formatter) are ignored build artifacts.
Edit their `runtime/*.kry` owners and run `make generate-web-runtime`; never
track or hand-edit the generated JavaScript. Handwritten DOM/storage adapters
remain tracked. Distribution packages include the generated modules so users
of packaged tools do not need a source build to obtain browser runtime files.

## Belongs In Kryon

- reusable widgets, layout, text input, focus, theme, DPI, modal, scroll, and
  input-capture behavior
- reusable renderer, backend, format, preview, packaging, update, sync, and
  desktop primitives
- generic examples that demonstrate one Kryon feature at a time
- generic tests and fixtures that assert Kryon behavior across backends
- documentation for Kryon APIs, formats, backends, and downstream update flow

## Belongs In Applications

The preview executable owns watching, build/reload orchestration, diagnostics,
and inspection. These are native developer tools, not public widget APIs.
Applications supply the existing host ABI and own their state initialization
and persistence; preview does not add app-specific state migration logic.

- product screens, routes, onboarding, workflows, and state machines
- product names, screenshots, icons, store metadata, app IDs, and domains
- app-specific copy, locale keys, settings semantics, and persistence policy
- business rules for sessions, habits, documents, projects, accounts, or other
  product concepts
- downstream integration code that only one application needs

## Forbidden Downstream Terms

`make kryon-boundary-check` rejects known downstream product material. The hard
forbidden list lives in `tools/check-kryon-boundaries.sh`, which is excluded
from its own content scan so the banned terms do not appear elsewhere in this
repository. When another downstream product leaks into Kryon, remove the
material from Kryon and add that product term to the checker in the same
change.

## Runtime Primitive Test

Before adding a public API or example, ask whether another unrelated
application could use it without inheriting product assumptions. If the answer
is no, keep it in the downstream app. If the answer is yes, name it after the
domain concept directly and cover it with Kryon-owned tests.

Instance storage is a host primitive: it allocates typed state by key and
releases it with its render host. Widget declarations own the record shape and
its updates. Generated runtime methods may bind to their owning host; generated
application functions use the active host API without an injected runtime
parameter. Product-specific state remains in application declarations.

## Generated And Showcase Material

Generated files should describe their generator and source inputs. Do not hand
edit generated outputs except to repair or replace the generator in the same
change.

Showcase material may reference external users of Kryon only when it is clearly
documentation about adoption, not an input fixture, public API example, or
runtime asset. Product examples used for rendering, parity, screenshots, or
coverage belong in the product repository.

Keep `examples/manifest.json` current when adding, removing, or renaming
examples. The manifest is the stable inventory for example metadata and curated
render-exact fixtures.

Widget props contracts belong in the declaration source. Button's C header and
native Go props type are generated from `runtime/button_props.kry`; native
`.kry` widget code consumes these types. The C retained ButtonSpec embeds ButtonProps
and the generated Style directly; it must not maintain another field schema or
reconstruct props from paint metadata. Geometry and texture fields are declared in
`runtime/drawing_props.kry`. Its `struct #extern` contracts reuse C/C++ host
definitions and generate native Go definitions; graphics resource ownership
remains with the host. Control style records use their own shared declaration.
Button's measurement consumes these actual props and style records directly;
hosts supply font measurements and available space, while `.kry` owns bounds
and shape decisions. Borrowed label/typeface fields retain host-owned storage;
shared code does not turn length-aware string views into C pointers.
Button's image field is the canonical `ImageProps` record. The native Button
and SegmentedControl implementations are `.kry` sources compiled into C;
remaining `src/ui` widget C sources are migration work, not an
alternative place for new UI behavior.
Runtime-generated Go binds host services to the owning runtime receiver.
Its arithmetic support belongs to the compiler and is emitted once per runtime
package in `numeric_support.go`. Widgets must not duplicate those helpers or
replace them with handwritten approximations. Ordinary application generation
keeps its independent per-module numeric support.
Generic `#extern` calls propagate this dependency through shared helpers, while
direct Go package imports stay stateless. Application host bridges remain
separate from runtime implementation; no runtime-wide mutable service setter is
generated. MeasureTextWidth supplies font-specific measurement without making
widget size decisions and restores the C host's previous typeface after use.
Accessibility action capabilities and value size/control-character rules belong
to `runtime/accessibility_policy.kry`.
Hosts project semantic nodes, validate snapshot generations and target identity,
and own bounded next-frame request queues. Focus/activation requests enter the
ordinary control input path after live eligibility and popup-ownership checks;
they do not mutate application values out of band. Value/selection requests
enter editor input handling, where hosts validate UTF-8, apply the shared
byte/scalar limit policy, normalize grapheme offsets, and cancel composition.
Hosts own and clear bounded queued payloads. OS screen-reader adapters
remain platform services and must marshal requests to the UI thread.
ReadActivation supplies pointer, keyboard, and accessibility samples using the
host's focus and popup ownership rules. `runtime/input_props.kry` owns the sample
contract.
Button interprets that sample in `.kry`, including disabled/loading gating and
explicit visual states. Deferred painting resolves its stored sample without
polling the host again. Its retained motion consumes that same resolved input.
Button content selection and placement also belong to `.kry`. The paint module
defines Drawing commands for text, icons, textures, rings, and chevrons. Native
hosts rasterize commands; they do not choose which content a Button displays.
Font resource lookup, glyph measurements and width calculation remain device
services. `src/ui/text.kry` aggregates glyph bounds for text height and baseline
placement and decides whether drawn glyphs need a clip; hosts apply clip
commands and blend pixels. C supports texture commands; the Go frame stream still lacks
texture resource rendering.
Material painting follows the same boundary. `runtime/material.kry` assembles
SurfaceDrawing commands from resolved styles, retained fills, and interaction
amounts. Backends own clipping and pixel submission; C may batch equal adjacent
pixels without changing the shared layer, color, or coverage rules.
Button frame assembly connects these stages in `.kry`. Hosts provide input samples, palette and metric values, style overrides,
frame timing, and scale. AdvanceFrame owns retained motion and appearance
resolution before BuildFrame assembles the result. BuildFrame owns normalized content flags, font fallback, opacity, content bounds, and
repaint eligibility. Content drawing consumes the same resolved StyleData,
without converting it back to public Style solely for drawing.

The native theme APIs derive their color roles through `runtime/theme.kry`.
Hosts supply the current palette, dark-mode state and shared disabled alpha,
then unpack `SchemeFor` results. The native color helpers for contrast,
mixing, and derived UI roles live in `src/ui/theme_color.kry`. OS palette
discovery and active theme state remain native services. The shipped color
catalog is authored in `themes/catalog_*.kss`; `src/ui/theme_catalog.kry`
parses and caches its 26 palettes.

Inset scaling and child centering also belong to the shared declaration code.
Tree traversal and layout-scope storage remain host responsibilities, while
explicit-position bypass and missing-size behavior use the same functions.
Measured flex distribution also belongs to `runtime/layout.kry`. Callers supply
item count and total main-axis extent, then consume `FlexStep` rectangles for
both painting and hit testing. Apps own item measurement and actions, not the
start/center/end, space-distribution, or cross-axis alignment calculations.
ContentBounds now returns the shared Rectangle contract; the duplicate
ContentBox record has been removed.

Button's modern paint pass is sequenced by `PaintButton` in `.kry`: surface
layers precede the mark and label commands. `SurfacePainter` and `Painter`
are borrowed synchronous rasterizer callbacks. The retained C renderer invokes
this pass after layout; Go invokes it when rasterizing its operation stream.
Their callbacks execute commands without choosing Button content or layer order.
The Go operation stream carries ButtonFrame by value, including its original
props and full style/material contracts. It must not reconstruct that frame
from flattened fields during rendering. Generic host placement and clip state
remain operation metadata. Older control producers adapt once when recorded;
that adapter remains temporary until those controls use shared declarations.

Typed child-content parameters belong to KIR. `#slot` declares a synchronous,
void-returning callable signature; generated C/C++ uses a typed callback plus its borrowed context,
native Go uses a function type, and JavaScript passes the callable unchanged.
The shared emitter performs argument evaluation and record copies before invoking
it. A slot call is a lexical binding, so backends must not reinterpret its name
as a widget or host function. Slots cannot escape through records, module state,
globals, or return values. Host-provided callables, initialized local aliases,
and matching `.kry` function values are supported. Contextual function binding,
callback adapters, and closure construction are shared compiler work; backend
resolvers supply symbol spelling and hidden state/host arguments. Inline `.kry`
slot bodies are lifted and checked in shared KIR. C/C++ environments borrow
pointers to used lexical values; Go and JavaScript use native closures. Nested
captures preserve reference updates and retained instance identity. Slot writes
are limited to the declaring block, and hosts must invoke borrowed callbacks
synchronously. Built-in widget composition still needs migration onto this path.
Named widget blocks already bind props and named slot values in shared KIR.
Required-slot validation, name conflicts, defaults, and source evaluation order
are compiler responsibilities. Backends receive ordinary declarations,
assignments, and a call; they do not maintain a second slot-argument mapping.
Resolved function and lexical slot calls take precedence over built-in widget
name classification.

The compiler's embedded runtime contracts contain `.kry` source text, not a second
parsed schema or handwritten field table. KIR parses embedded and file sources
through the same frontend. Runtime types are a fallback after lexical and
explicitly imported application types.

Control style records and constants belong to `runtime/control_props.kry`. Native
hosts consume generated C/Go interfaces and the web host re-exports generated
JavaScript constants. Backends must not add independent control enum or field
tables; the compiler reads this contract through its embedded declaration source.

Dropdown interaction and layout policy belongs in `runtime/dropdown.kry`. Native
hosts own input collection, popup storage, clipping, and painting through shared
Button frames and materials; they must not add independent dropdown theme paths.

Composed popup transitions belong to `runtime/popup_policy.kry`.
`PopupLifecycleBegin`, `PopupLifecycleRelease`, `PopupLifecycleKeyboard`, and
`PopupLifecycleFinish` own admission, hover visibility, context activation,
open state, dismissal, input bounds and backdrop policy. Native `src/ui/popup.kry`
and Go `popup.go` apply those results. Hosts retain paint/input tokens, event queues,
open-pointer storage and scope restoration. Keyboard ownership is sampled
**after** beginning the popup input scope; moving that query earlier changes
nested Escape behavior. `runtime/popup_ownership.kry` owns ancestry traversal, branch ordering, capture,
autofocus, focus restoration and owner retirement. Hosts supply parent cursors,
opaque identity equality and registry storage; they validate token lifetimes.
`FocusTraversalFor` owns Tab target indices and wrapping after the host filters
its stored focus IDs. `ScrollScopeFrameFor` owns scroll geometry, wheel use,
thumb dragging, release consumption and cancellation when ownership is lost or
the scrollbar disappears. Hosts retain clip/paint stacks and drag identities.

## Keyboard and release policy

Read-only text tokenization and line assembly belong to `runtime/paragraph.kry`.
`ParagraphTokenNext` yields borrowed UTF-8 byte ranges; `ParagraphLineAdvance`
owns hard breaks, overflow, empty lines, and the completed line ranges. Native
`text_layout.kry` and Go `text_layout.go` retain strings/arrays and supply font
measurements, including the shaped joined candidate. They do not contain a
second whitespace grammar or wrapping algorithm. `paragraph_host.go` applies the same token, spacing and wrap decisions to Go
Paragraph inline icons. Built-in icons paint through the native icon renderer;
raw texture upload remains a host capability.
`runtime/text_rows.kry` owns source-preserving logical lines, visual row breaks,
heading font choice, caret affinity and point-to-row selection.
`src/ui/text_rows.kry` and Go `text_rows_host.go` provide borrowed strings, grapheme
boundaries, measured prefixes and row storage. Native TextArea measurement,
painting and hit testing share that iterator; selectable blocks request its
word-break mode. Cross-line selection/composition paint ranges remain owned by
`runtime/text_input.kry`.
The native Go wrapped-text cache stores bounded host measurement results, not
another layout algorithm. Its font-generation invalidation belongs to the font
registry; color, alignment and clipping remain live per-frame paint decisions.

Accessibility projection is a host service over committed retained nodes (C)
or completed frame operations (Go). It reuses widget state and checkbox flag
policy, strips secure values and selection offsets, and publishes snapshots through the host sink.
It must not infer duplicate controls from paint decoration or mutate editor
buffers. Action queues are a separate host input service. Linux AT-SPI object
identity, D-Bus interfaces, cache/event publication, screen geometry, and scalar
offset conversion belong to the native host adapters, not generated widget
policy. C callbacks run on the UI thread through a private GLib context; Go
D-Bus workers enqueue owned requests for UI-thread delivery. The public snapshot array carries parent indices and semantic keys, independently
of this transport. Other OS adapters and remaining composite
selection/value interfaces remain unimplemented; see [ACCESSIBILITY.md](ACCESSIBILITY.md).
ListBox selection is implemented through the same validated queue. Shared policy
decides selected state; hosts match stable item keys, update caller-owned
selection arrays, and reveal rows. Linux adapters own Selection wire indices
and change signals, while the snapshot's option parent and item index remain
backend-neutral. Tree/table/dropdown/range selection is still separate work.

Menu Escape/dismissal suppression, Collapsible arrow priority/keyboard actions,
and text shortcut/edit-command decisions belong to `runtime/*.kry`.
`runtime/text_input.kry` also owns navigation and deletion intent, word-boundary
classification, and composition phase/range decisions. C and Go use the generated
word-boundary rule; Go applies generated navigation, deletion and composition
decisions. Hosts resolve character movement through private Unicode grapheme
adapters (utf8proc in C, clipperhouse/uax29 in native Go); neither duplicates the
Unicode segmentation tables or the shared editing policy. Byte-offset storage,
scalar-count limits and platform IME preedit offsets are separate contracts.
Native and Go event loops sample keys and apply those decisions; clipboard IO, buffer
storage, focus registration and pointer-event queues remain host services.
The paused web implementation historically used generated text-input policy,
with JavaScript limited
to string storage, UTF-8 offset traversal, event routing and clipboard transport.
Go and web defer input after backward Tab until the destination's next frame,
discarding it if focus changes in between. Composition events do not transfer
to the next field during Tab navigation.
Historical JavaScript keyboard and browser composition tests are future-roadmap
reference only. JS/web remains paused. Native generated parity executes C and
Go. Linux/X11 IBus composition and real pointer/keyboard delivery are tested on
a private virtual display; candidate coordinates are checked against the shared
wrapped-row geometry. Other OS adapters remain separate platform work.

Canvas camera selection and coordinate transforms belong to `runtime/canvas.kry`.
C stores backend matrix snapshots per lexical scope; Go adapts frame-operation
coordinates. Clip stacks, renderer flushes and restoration of saved native
objects stay in the host. `TreeFocusBegin` / `TreeFocusAdvance` own retained
hierarchy traversal; native hosts supply stored node depths and resolve IDs.
See [native ownership evidence](NATIVE_POLICY_OWNERSHIP.md) for the audited
surfaces and the remaining platform boundaries.
## Native text and image resource ownership

Editable row caches and decoded image caches are bounded native host resources.
They do not own wrapping, fit, selection or style policy: row and fit decisions
come from shared `.kry` modules. Font measurement, Unicode iteration, file
decoding, pixel sampling and cache synchronization remain native services.

Text content fallback and selection-alpha decisions belong to `runtime/text.kry`.
Native rasterizers preserve explicit foreground/focus transparency and opacity.
Unstyled editors retain text/caret content; they do not add widget fills, borders,
colored selection decoration or outlines around layout scopes.

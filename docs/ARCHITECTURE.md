# Kryon Architecture

Kryon is organized as a reusable runtime with a small set of public headers,
backend implementations, format/tooling paths, and conformance tests. The
runtime should expose primitives; downstream applications compose those
primitives into product behavior.

The Linux libdraw host observes physical keyboard/focus events on its own X11
devdraw window through a private connection. Plan9port continues to provide
composed text and rendering. Host event translation stays in the backend;
application shortcuts and file actions remain downstream. See
[libdraw input](libdraw-input.md) for transport limits and tests.

The KSS parser and formatter are generated from `runtime/kss_parser.kry` and
`runtime/kss_formatter.kry` for C, Go, and JavaScript. Named parse environments
also pass through the shared parser module; hosts supply their platform default
and import sources. Go's source pack registry retains source/variant identities
and stages theme re-resolution before replacing published sheets, so a missing
import cannot partially apply a theme change. Generated provenance checks cover
both KSS modules, including byte equality for locally generated JavaScript
artifacts. Generated browser modules are ignored build outputs, not tracked
sources. `make generate-web-runtime` creates all nine from `runtime/*.kry`;
web test, compiler-tool, and packaging targets depend on that generation.
Declarative consumers resolve each yielded rule against the current token table
through `KssResolveCSSValue`; keyframes use the same declaration stream. Earlier
rules retain their values and provenance when later overlays update tokens.
Structured JavaScript string indexing and length use UTF-8 bytes, matching C/Go
and the byte offsets consumed by `StringSlice`.

`runtime/style_sheet.kry` owns specificity weights and per-field winner decisions.
Native cascades retain a `StylePriority` tuple for each field; web hosts retain
the same tuples in property maps. The shared comparison orders layer,
specificity, and source order directly, avoiding collisions from scalar score
arithmetic. Scalar web scores remain diagnostic metadata only.
The shared KSS parser module also owns attribute operator predicates and
nth-position formula evaluation. Browser adapters provide attribute strings,
sibling indices/counts, and traversal direction; generated code decides matches.
`KssStateFacts` and `KssStructuralFacts` carry host observations into generated
state and structural predicates. The web adapter collects structural facts once
per selector/node match; it no longer reimplements the rules for those pseudos.
Declarative selector lists/chains and atoms are streamed by `KssSelectorPart`
and `KssSelectorNext`. Their borrowed UTF-8 spans avoid a second regex grammar
in the web object builder, CSS pseudo serialization, and structural matching.
Balanced groups preserve nested functional arguments;
malformed streams fail explicitly. Typed native selectors retain their closed
representation and are not widened by the declarative export lexer.
Selector-list pseudos retain individual groups in web selector objects.
`KssSelectorGroupMatches` reduces each group's observed match count using the
shared positive/negative rule, so repeated `:is`/`:where` groups cannot collapse
into a single OR list. CSS serialization preserves group names and boundaries.
Ordered ID/attribute condition lists likewise preserve repeated constraints.
Hosts supply `KssIdentityFacts` to generated identity matching; existing generated
attribute predicates evaluate each retained attribute condition. Summary maps
are not the matching or serialization source for parsed selectors.
`KssSelectorChainBegin`/`KssSelectorChainStep` drive a depth-first chain search
through host-owned frame storage. The generated driver chooses parent versus
previous sibling, restricts immediate relations, and retries descendant/general
sibling alternatives after a later condition fails. The browser adapter supplies
relationships and simple-selector observations; it no longer owns a greedy
combinator loop. `KssRelativeSelectorBegin` extends the same traversal with a
subject anchor. `KssRelativeSelectorPart` supplies explicit or implicit leading
relationships, while the host enumerates frame candidates. This handles full
child/sibling relative chains and prevents ancestry outside the subject from
satisfying an implicit descendant prefix. Node paths canonicalize copied host
node references. Shared lexing rejects nested `:has` without rejecting quoted
attribute values or comments that merely contain the same text.
`KssPseudoMatch` combines structural matching and nth-position policy behind
one generated decision interface. The web adapter gathers sibling/type indices
and content/focus facts, then handles the returned match/reject/traversal result;
it no longer dispatches the four nth forms or chooses their traversal direction.

`KssParseNth` is shared by matching and `KssNthText` canonical serialization.
The CSS adapter no longer strips punctuation from positional arguments, which
previously changed fractional input into a different integer selector. Shared
serialization removes KSS comments and normalizes formulas before CSS emission;
invalid formulas become always-false predicates. `KssPseudoFunctionInfo` keeps
functional classification consistent between export and matching.

CSS export's property vocabulary now reuses `KssCSSPropertyKind` through
`KssCSSPropertyName`, replacing the separate browser property map. The generated
module owns aliases, numeric-unit decisions (`KssCSSNeedsPixels`), and border
shorthand classification (`KssCSSBorderShorthand`). Inline application and CSS
export share the latter decisions. Hosts retain value storage and string output;
ordinary-rule and keyframe property expansion and effect recipes also run in
shared code. `KssCSSExpandDeclaration` yields property names, and
`KssCSSEffectAt` yields borrowed output fragments, avoiding fixed-buffer
truncation. One browser output adapter consumes those results for both contexts.
The same declaration stream now feeds inline application, removing its separate
property map and composite logic. The DOM sink handles property spelling, custom
property methods, tracked cleanup, and mounted inline overrides. Shared policy
selects explicit background-image over generated gradients and inserts an
authored-border default before explicit style declarations. Cross-rule alias
and shorthand cascade equivalence remains conformance work.

## Public API

Public headers live in `include/`. They define the app-facing Kryon surface:
window/runtime compatibility, UI widgets, platform services, formats, update,
sync, filesystem, desktop integration, and optional terminal primitives.

Public APIs should be named after the real domain concept. Do not add temporary
prefixes, compatibility aliases, or product-flavored names for new behavior.

The optional sync-node pool is local-first: paired LAN nodes rank ahead of
paired remote nodes, and a configured public node is the final fallback. Each
node has isolated bearer-token and clock-skew storage, so failing over cannot
send one node's credentials to another. Connectivity and authentication errors
advance to the next node; account, signing, and payload errors stop immediately.
Discovery may add candidates, but only explicit pairing grants trusted status.

## Runtime Implementation

Portable widget policy lives in `runtime/` as `.kry` source. C runtime code in
`src/` still contains widget orchestration alongside rendering helpers, platform
adapters, serialization, sync/update primitives, and backend-specific translation.
The native Go runtime lives in `go/kryon/`. Internal host helpers should remain
private unless an unrelated downstream application demonstrably needs them.

### Widget lifecycle consolidation

Read-only text layout uses a streaming protocol from `runtime/paragraph.kry`.
The shared tokenizer returns source byte ranges without allocating or copying
strings. The line driver receives the host's measured standalone element and
joined candidate, then emits completed line ranges/widths and the next line
state. Content presence is explicit, so zero-width glyphs still participate in
wrapping and an oversized first word does not create an empty leading line.
Hosts retain output buffers, font shaping/measurement, and paint operations.
Native Text/paragraph drawing and Go Text, Paragraph, and ParagraphText consume
this protocol. `make paragraph-policy-test` executes C/C++ decisions and the
native adapter; Go tests run the same JSON line fixtures through its adapter.
These fixtures compare layout with identical measurements, not font rasterizers.

#### Canonical `.kry` widget declarations — migration target

Declared records have portable typed initializers, for example
`(Props){.value = 5, .appearance = (Appearance){.inset = 7}}`.
Positional initialization such as `(Props){5}` is also supported; named and
positional fields cannot be mixed. Omitted fields are zero-initialized, and
explicit field expressions run once in source order. Imported records, nested
records, and passing initializers directly to declared widgets use the same
value-copy semantics across C, C++, Go, and JavaScript. Strict checking rejects
unknown, duplicate, excessive, or incorrectly typed fields. This applies to
declared `.kry` records, not arbitrary host-language aggregate syntax.

Portable emission captures each call argument once, in source order. Reading
a nested field captures the field value directly instead of copying every
enclosing record. Record arguments still have independent value semantics,
including when a later argument mutates the original record.

Portable policy records now support immutable UTF-8 `string` values, including
empty initialization, field assignment, copying, parameters, returns,
conditionals, and content equality (`==` / `!=`). Literals preserve embedded
nulls and normalize escapes across C, C++, Go, and JavaScript. Invalid UTF-8,
invalid Unicode scalar escapes, arithmetic, ordering, and numeric casts are
diagnosed rather than emitted as host-dependent operations. String creation,
concatenation, slicing, and a general ownership model are not implemented by
this change; this is not a claim of language completeness.

C/C++ generated headers expose `String { data, length }`, `StringView`, and
`StringEqual`; views borrow immutable storage which the caller must keep alive.
Zero-length views may have a null data pointer. Nonempty views must address
their declared number of valid UTF-8 bytes. Go and JavaScript use native string
values. Portable calls do not silently convert these views to null-terminated
foreign parameters. `runtime/style.kry` uses this support for typeface names;
native style adapters bridge registered, null-terminated host names.

Shared bodies also support existing `const char*` fields as immutable borrowed,
null-terminated UTF-8 strings. Copies borrow the same storage; C callers must
keep it alive. Null and empty compare equally. Literals, copies, parameters,
returns, conditionals, and content equality work across targets. Borrowed
literals reject embedded null bytes. Length-aware `string` values cannot be
implicitly assigned or cast to borrowed strings; a matching Go/JS host value
must likewise contain no embedded null. This makes public ButtonProps and
Style usable in shared bodies without changing their native C layouts.

This is the intended ownership boundary, not a claim that the migration is
complete. The parser currently recognizes built-in widget names and props
types in `cmd/kir/kir_parse.c`; native Go also carries a props-field mapping in
`cmd/k2go/k2go_lower.c`. Shared functions in `runtime/button.kry`,
`runtime/text.kry`, `runtime/style.kry`, and `runtime/surface.kry` remove some
duplicated policy, but do not yet constitute complete widget declarations.

`runtime/control_props.kry` owns the public Button tone, emphasis, state, size,
icon placement, material, and Style field constants, plus `Style` and
`ControlStyle`. C and Go use generated interfaces; JavaScript re-exports the
generated constants. Go retains the public enum types and the `uint32` flag
constants. The duplicate internal policy enums have been removed. `SizeValue`
remains in `runtime/style.kry` for reuse by measurement and other widgets.

`runtime/style.kry` owns `StyleData` and the nested `StyleStates` record used
by state resolution. C and Go convert their public `ControlStyle` values into
that record; state selection and merging remain in `.kry`. Button's
`ResolveAppearance` selects one effective state for both its defaults and
overrides, so hosts no longer assemble that resolution sequence. Likewise,
`MeasureContent` in `runtime/button.kry` supplies both natural measurement and
content placement with the same fitted icon size and gap rules. These are
shared data and policy building blocks, not a substitute for declaration
resolution or per-instance widget state.

`ResolveInteraction` returns the effective control state together with its
hover, press, and focus signals. C and Go consume this shared result for button
appearance and motion instead of assembling state precedence and explicit
preview behavior independently. Input collection and instance storage remain
host responsibilities; this is not yet a per-instance `.kry` lifecycle.

`runtime/surface.kry` separates the inward material volume from dark focus
edge bloom. The latter uses a finite blurred stroke with an `outside_only`
coverage mask, leaving the face and label center untouched. Both hosts execute
the same mask and focus track; light themes and disabled controls suppress this
layer, and an explicit transparent focus color remains transparent.
Tall saturated faces in light surroundings gather a lifted chromatic lower
reflection inside the face. Height and chroma blend its strength continuously;
pressure attenuates it on the shared motion track. Pale and compact surfaces
retain their restrained reflection, without an added external fog layer.
The layer record also identifies the material face with `is_face`. C and Go
apply custom fill styles to that role, not a hard-coded position in the layer
sequence; the `.kry` material remains authoritative when layers are reordered.

Ordinary function resolution is shared in KIR: local functions shadow imports,
only public functions of explicitly imported modules are visible, and competing
imports are diagnosed as ambiguous. C, C++, Go, and JavaScript lowering use the
same resolved owner instead of searching a process-wide function-name table.
Tests execute same-named providers in both input-file orders. Custom blocks
resolve through that same lookup to `#ui` functions with a props record followed
by optional typed slots. KIR zero-initializes the record, evaluates prop fields
and slot values in their source order, then emits one ordinary typed call.
Declared record fields supply property validation and nested-initializer types;
slot parameters supply names and signatures. Every slot is required, and its
name must not conflict with a props field or another slot. The same lowering
handles blocks without slots, replacing the previous props-only constructor
path. Ordinary calls that resolve to declarations or lexical slots also bypass
the compiler's built-in widget statement classification.
Leaf blocks with built-in names participate in that same lookup: an explicit
local or imported `Button` or `Text` declaration owns its props type. Host
props are a fallback only when no declaration resolves; an invalid declaration
is an error, not a reason to silently select the host widget. Composed scope
blocks still use the built-in parser path and need migration with child slots.
Named child-content signatures now have a KIR type: `Content :: (bounds:
Rectangle) #slot`. A declaration may accept `content: Content`, invoke it, and
forward it to another declaration. Calls check slot arity and argument types;
record arguments retain value semantics across C, C++, Go, and JavaScript.
Slots accept host-provided callables and matching `.kry` functions. KIR checks
function values against the expected slot signature, including imported record
identity and private visibility; lexical values shadow function declarations.
C/C++ adapters implement the borrowed-context callback ABI. Go and JavaScript
closures preserve the resolved function's hidden module state and host arguments.
Host use propagates through function references so native Go binds the originating
runtime receiver. Inline `#slot` bodies capture enclosing lexical bindings by
reference and use the same callable ABI. Anonymous nested widget syntax without
a named slot still uses the built-in scope path.
Unknown, duplicate, and incorrectly typed props are errors even without strict
mode. Tests execute local and imported declarations in both source orders in
C, C++, Go, and JavaScript, including ordinary calls and record value semantics.
Declarations may return an action result. A block invocation discards that
result, like a call used as a statement; an ordinary expression call can consume
it. Cross-target tests exercise true and false results, discarded results,
props copying, and imported declaration order without a second widget entry point.
JavaScript's direct action-call lowering also uses the widget runtime when a
result is initialized, assigned, or returned, rather than constructing an inert
description object. Button tests cover activation and evaluated lexical bounds
in each form. Arbitrary nested action-call expressions still need migration.
Ordinary calls to resolved `#ui` declarations also reject known argument-type
and argument-count mismatches without strict mode. Using function syntax does
not bypass the declaration's signature. Unresolved host-header APIs do not yet
provide that declaration metadata to KIR; their migration remains necessary.
Braced `case` and `default` labels remain control flow, not widget declarations;
cross-target tests execute both switch paths around ordinary widget calls.
Declared child content uses explicit named slots; implicit child blocks remain
unsupported for custom declarations.
Explicit-key instance bindings are described below. Built-in widget migration
is still pending.
Portable `#ui` bodies use the same checked emitter as ordinary functions;
the annotation does not force backend-specific arithmetic or record handling.
Strict tests execute narrow-integer overflow inside a declared widget through
both block and ordinary-call syntax in C, C++, Go, and JavaScript. Bodies with
unsupported host operations still require the existing backend path.
The built-in path accepts named `Button` blocks. The Lightfield example uses
those blocks with explicit IDs and shared Button props. Childless Button
blocks lower to the ordinary `Button` call; only blocks with child content open a
lowered host content scope. Explicit IDs remain necessary for stable identity;
the block name does not yet supply instance identity.
JavaScript diagnostics record their evaluated nested props and geometry; menu
interaction and material raster parity in that host are not implemented by
this registration.
The indexed open-state references retain their backing state-array elements.
Uninitialized state arrays receive independent zero-initialized elements,
including nested arrays and declared records, instead of scalar placeholders.
The generated-runtime parity fixture also composes actual themed Buttons
inside two instances of a props-taking declaration, with distinct IDs and
action amounts. C, Go, and JavaScript check that activation remains independent
after declaration order reverses; native keyboard checks remain enabled.
This covers explicit stable IDs, not automatic instance-key scoping.
JavaScript emits evaluated Button initializer objects so lexical props reach
the runtime as values, rather than unevaluated source text. Its automatic frame
selection accepts zero-argument screens or a single host `Rectangle` viewport;
other props-taking helpers are not screen entry points. The JavaScript host
supplies current target dimensions, falling back to application dimensions for
headless or zero-sized targets. Local initializer declarations and assignments
now use the same evaluated initializer lowering as Button props. Named fields
become objects and positional initializers become arrays. The actual Lightfield
source is checked for finite Button geometry and representative font sizes.
The check clicks its actual Light/Dark controls and verifies light-to-dark-to-light
switching, including the per-theme geometry and icon-button emphasis.
JavaScript Style presence-bit bindings are checked against the shared `.kry`
merge policy, including zero-valued overrides. This is not browser visual
parity or complete typed-record lowering: remaining host enum coverage,
positional record field access, other widget argument lowering, and rendering
still need migration.

The JavaScript web path also carries source-level node metadata from named
`.kry` UI blocks. `Button save: { dom = "button"; class = "primary" }`
lowers through KIR as a widget statement whose web metadata is separate from
native widget props. The runtime exposes those facts through
`webDocumentFrame(rt)`: Kryon kind, node name, DOM tag/id/class/role, event
binding names, state, bounds, text/link/image/input metadata, and app metadata.
This is the first stable bridge for the hybrid native DOM path: `.kry` remains
the structure source, future KSS rule tables resolve against those facts, and
`mount()` consumes the frame to create native browser elements instead of debug
placeholder widgets. See `docs/WEB_DOCUMENT_IR.md`.

Strict portable emission supports declared `.kry` enum values in function
parameters, returns, locals, conditionals, and record fields, including imported
types. Values use signed 32-bit storage: C emits an `int32_t` typedef, C++ a
fixed-underlying-type enum, Go a named `int32` type, and JavaScript checked
numeric values. Explicit numeric casts use the shared integer conversion rules.
Native runtime Go modules use one generated `numeric_support.go` for checked
arithmetic and conversions. The compiler emits that support from the same
numeric emitter used by ordinary applications. Application files retain their
module-local support so a generated file can still compile independently.
The runtime generator reserves the `numeric_support` output basename.
Zero initialization and record copies preserve enum fields. The shared checker
rejects implicit integer-to-enum and cross-enum assignments, record-to-enum
casts, and enum compound arithmetic. Enum member constants retain their existing
integer expression behavior; use an explicit enum cast when assigning them to
a named enum property. `tests/imported_cast_test.py` executes this contract in
strict mode on all four targets.

JavaScript fallback expressions resolve casts to declared `.kry` enums through
the shared type lookup, including runtime-call arguments. Numeric operands are
truncated toward zero. Theme system/light/dark resolution uses the generated
`ThemePolicy` enum in `runtime/theme.kry`; host-facing theme APIs may still
bridge from native `ThemeMode` at their boundary.

Each widget must have one canonical declaration in `.kry`. That declaration
owns its typed props and defaults, per-instance state, events, measurement and
layout policy, child composition, and appearance. Generated C and native Go
must consume the same declaration. Adding an application-defined widget must
not require editing compiler name lists, backend props tables, or handwritten
widget implementations.

Button size is a prop, not another widget: compact callers use `Button` with
`ControlSizeSmall`. The separate small-button entry points have been removed
from C, native Go, and the compiler's built-in call registry.

Shared abstractions beneath declarations have separate responsibilities:

- `Style` describes material and typography, with explicit field presence so
  transparent colors and zero dimensions are not mistaken for missing values.
- Surface and motion primitives describe reusable drawing and transitions;
  there is no separate public button-paint abstraction.
- Text measurement and text editing are distinct services. TextField and
  TextArea remain separate widgets but share editing, selection, and composition
  behavior rather than duplicating an editor.
- Hosts provide font shaping/rasterization, drawing, input delivery, clipboard,
  IME, time, and storage. Widget policy decides how these services are used.

The compiler must represent declarations and typed child slots in KIR before
backend lowering. Declaration resolution must work across modules and source
order, with diagnostics for unknown props, invalid event/slot types, duplicate
declarations, and invalid state access. A block invocation and a function-style
invocation must resolve to the same widget; a second implementation or alias
does not satisfy this requirement.

State belongs to a stable widget instance within a window, not to a declaration
global. Reordering keyed children must preserve their state; separate windows
and separate instances must not share interaction or animation tracks.

`runtime/button.kry` declares `ButtonInstance`, including its motion tracks.
C's host-owned `ToolkitStore` and Go's runtime allocate typed instance records
by a 64-bit key. Types and render hosts have separate namespaces; inserting or
reordering other instances preserves existing records and their addresses.
Existing C frame bindings select the store for native windows, nested rendering
hosts, and headless UI frames. Closing a host releases its instance storage.
`runtime/instance.kry` supplies the shared twelve-frame retention rule. Each
host sweeps its own store even when no buttons are drawn; another host's frames
cannot expire its entries. Button uses this storage in both native runtimes.
The shared compiler lowers `name: Record #instance(key)` to a borrowed typed
record in C, C++, Go, and JavaScript. Keys evaluate once; ordinary copies retain
value semantics, while field and whole-record assignments update the retained
value. JavaScript uses the generated `instance.kry` expiration policy too.
Integration tests execute custom blocks and ordinary calls against all four
real hosts, including type isolation, reordered and wide keys, whole-record
replacement, lexical shadowing, and expiration. Instance bodies must pass the
shared portable checker, even without `--strict`; a fallback must not turn an
instance binding into a local or module global. Hierarchical keys and migration
of built-in composition onto typed slots remain necessary before this is a
complete declaration lifecycle.

With `k2go --runtime-implementation`, functions that access instances or call
host `#extern` services
are methods on their owning runtime. The checker propagates this requirement
through calls, so composed helpers preserve their receiver without consulting
another window's active host. Runtime builds call host methods directly and do
not generate a module-global Host interface or setter. Direct Go package imports
remain ordinary stateless calls. Host services cannot run in runtime global
initializers because no owning runtime exists there. Application declarations
retain their explicit host-service bridge and use the active host's
generic instance service. Button's retained motion update now lives in `.kry`
and uses this binding; C and Go no longer look up or mutate its stored tracks.

`runtime/button_props.kry` now owns Button's public props fields. C and Go use
generated declarations instead of separately maintained structs. The retained
C ButtonSpec also embeds that complete props record and the shared Style;
conversion tables and duplicate tone/emphasis fields have been removed. Paint
uses the declared pill/circle flags rather than reconstructing shape flags
from a resolved radius. The remaining native ButtonSpec holds legacy paint and
retained surface metadata until the host lifecycle is migrated.
`runtime/drawing_props.kry` declares the fields of Vector2, Rectangle, Color,
and Texture2D as `struct #extern` contracts. C and C++ reuse the graphics host's
types; native runtime generation emits their Go structs. The shared checker can
therefore validate field access, copies, zero values, and instance bindings for
these records without per-type compiler tables. Style types also have their own
shared contract. Button measurement now reads ButtonProps and Style directly,
including label presence, icon presence, shape precedence, and scaled bounds.
The duplicate ButtonMeasure/MeasuredSize interface has been removed. Hosts
provide font measurement, available space, and physical scale. The declaration
now requests its label width through the MeasureTextWidth host service; native
C and Go do not perform that request separately before calling the declaration.
Button input interpretation and motion inputs now live in the declaration.
ReadActivation acquires the host's pointer/focus sample; ResolveButtonInput
applies widget flags and explicit states. Deferred C painting supplies its
stored sample to the same resolver without consuming input again. Content
painting now emits shared Drawing commands: `.kry` selects the loading ring,
disclosure, texture or icon, and label, and positions them in physical space.
C and Go execute these commands with their text and pixel rasterizers. Material
layer assembly lives in `runtime/material.kry`: shared surface selection, fill
overrides, physical layer bounds, visibility, and content displacement have one
implementation. Hosts submit resolved styles and rasterize SurfaceDrawing
commands. Button's AdvanceFrame sequences retained motion, style resolution,
and BuildFrame from host input samples, theme values, timing, and scale.
BuildFrame assembles normalized props, its material,
content insets, physical font fallback, foreground opacity, and repaint state.
The C styled paint path consumes that frame directly; native Go stores it in
FrameOp.Button without flattening its props, style, or material fields.
Composed content inherits its font, color, and bounds from that same frame.
The renderer applies final host placement to a copy before painting. Legacy
control producers convert their old operation fields when recorded, after
inherited disabled-state processing; the rasterizer accepts only shared frames. Both rasterizers invoke `.kry` PaintButton, which owns
surface-layer and content command sequencing through synchronous typed Painter
and SurfacePainter callbacks. The native adapters supply glyph measurements
and execute commands in the receiving rasterizer. Style/theme acquisition, legacy C appearances, and child
lifecycle still need migration to complete the widget body.
Its content geometry uses shared InsetBounds and CenterChild functions: the C
retained child layout and Go layout scope no longer implement separate inset,
missing-dimension, or centering rules. Measured single-line distribution lives
in `runtime/layout.kry`: `BeginFlexCursor` resolves main-axis free space and
`FlexStep` resolves each item's cross-axis alignment before drawing or input.
Both generated runtimes consume this policy without host-side spacing math.
User-defined widgets now accept typed
slots with inline captured bodies. Shared KIR lifts nested bodies, resolves
lexical captures, checks signatures and borrowed lifetimes, and emits callback
environments for C/C++ or native Go/JavaScript closures. Built-in Button still
uses its existing host scope and needs migration to this declaration path.
The compiler embeds the declaration sources
from `runtime/*_props.kry` and parses them with the same KIR frontend as application
files. Button's Go field-order entry and type-name entry have been removed; native
props lookup now reads that declaration. Explicit local or imported declarations
take precedence. Other legacy host types still use the old registry until their
contracts move into declaration sources.

Runtime generation discovers `runtime/*.kry` as one checked module set. Adding
a shared module does not require another per-widget C or Go generation rule.

Migrate Button end-to-end as the first composition/state test, followed by Text.
Do not call this complete merely because a new declaration parses: tests must
prove user-defined widgets compile without name-table changes, generated C/Go
props agree, two instances remain independent, children inherit and clip
correctly, and existing input, transparency, theme, and motion behavior remains
covered. Remove each old implementation only after its maintained callers have
migrated. TextField/TextArea implementation follows this foundation; visual
proposals are not evidence that their declarations or editing behavior exist.

#### Current retained runtime

Captured C retained paint includes blend state through the private
`ui_blend_internal.h` snapshot. OpenGL 3.3/GLES2 backend hooks preserve both
effective GPU factors/equations and rlgl's cached mode/custom configuration,
including changes awaiting activation. The hooks are injected into the build
copy by `prepare-raylib-source.sh`, not into the raylib submodule or public API.
Each captured node restores its declaration state for painting and restores the
caller's state afterward. The snapshot itself does not own render resources.

The private `ui_paint_layers.c` context now owns temporary C render textures,
collects mixed immediate/retained layers, and composites them in opening order.
Nested layers remain above subsequent parent paint; hidden parents suppress
descendants. Capture and composition restore drawing state, and texture resource
operations preserve the active framebuffer. Native `NativeWindow` hosts lazily own
their context and handle its frame, composition and destruction. The private
active-window accessor exposes that owner to future composed widgets without
adding app-level lifecycle calls. Main UI frames own a separate lazy context:
`SetFrameCamera` starts it, `EndInterfaceFrame` composites it, and `CloseWindow` releases
its resources before graphics shutdown. The private frame accessor routes to
the active NativeWindow when appropriate. Presenter readback preserves the calling
framebuffer. Layer scopes suspend the retained Row/Column path, keeping popup
children in the screen tree without consuming the owner's layout slots. Scope
exit checks balanced child layouts and restores the owner's path. Disabled
scopes likewise inherit the owner's state behind a protected local depth floor,
then restore the parent's scope on exit. Input clips and scroll depth are
isolated similarly, while modal capture stays active. The private C popup input
registry supplies nested branch capture to the shared hit-test path. Each paint
host owns its registry and advances, binds, retires and destroys it automatically.
Auxiliary frames switch to their own registry and restore the parent's binding
on completion; hiding a layer closes its input branch. Retained nodes snapshot
popup ownership for deferred hit testing and button hover/press state; snapshots
are kept with the committed tree while a replacement is declared. Closed,
missing, previous-frame or destroyed owners cannot receive those hits.
Deferred pointer-focus registration uses the same ownership snapshots without
reopening scopes, preserving modal, clip, disabled and inspection capture.
Retained hit testing and button hover/press state now use that same full capture
predicate, so modal blocking also prevents deferred click events.
The public `Popup` block binds these paint, layout and input contexts for
arbitrary native children. App-facing dismissal updates the caller-owned `open`
state; the lower-level close hook is internal host support. `PopupTooltip`
reuses that paint/layout scope while intentionally skipping input ownership.
`PopupModal` uses the same scope with a full-view input/backdrop policy, and
`PopupContext` uses it with right-release activation over a retained trigger.
Presentation variants remain flags on the one popup implementation rather than
parallel widget trees. C and Go execute the same `PopupLifecycle` transitions
from `runtime/popup_policy.kry`: begin validates props and resolves visibility;
release processes each sampled pointer release; keyboard runs after the host
claims input ownership; finish resolves caller closure and explicit dismissal.
The hosts retain resource allocation, event queues, painting and balanced
scope restoration. Tooltip triggers respect clipping, disabled scopes and
popup capture; closing a tooltip drops its paint without changing an optional
caller-owned open value. The former standalone Escape adapters are removed.
`make popup-policy-test` runs generated C/C++ transitions; the native UI and Go
popup suites exercise trigger blocking, nested Escape and focus restoration.
Modal outside releases remain non-dismissing. The input registry saves focus
when a top popup first opens, focuses its first eligible child, and restores the
parent or background after nested close, root close, or missing-owner retirement.
Popup registries execute ancestry and branch-order traversal through the
streaming `PopupOrder` / `PopupAncestry` drivers in `popup_ownership.kry`.
C supplies linked parent cursors and Go supplies map entries. Both apply shared
capture, autofocus, restoration and retirement decisions. A child opened after
its parent closes remains inactive and retires at frame end. Tab traversal uses
`FocusTraversalFor`, with host-owned focus-ID storage and token validation.
Composed Scroll scopes use `ScrollScopeFrameFor` for bounds, wheel handling,
thumb movement and closure. Their host drag tokens retain popup ownership, so a
closed owner or a disappearing scrollbar cannot keep changing the offset.
Long-lived drag values, sliders, splitters and table resizers store the same
persistent owner identity, allowing out-of-bounds continuation only while that
branch remains topmost and cancelling on dismissal or ownership changes.
C and Go now have a pointer-independent top-popup keyboard predicate. Closed
dropdowns use it before keyboard opening, preventing a focused parent/background
dropdown from opening behind a child popup. Focus registrations now retain their
popup owner: C filters and deduplicates Tab destinations at focus finalization,
before releasing the host input binding; Go filters its current/previous-frame
focus order during traversal. Native tests cover forward/reverse wraparound,
parent/background exclusion and traversal after explicit dismissal. Go also
exercises actual editor Tab events. Native lifecycle tests cover automatic
first-child focus acquisition and parent/background restoration. Complete
keyboard routing remains unfinished. Generic accelerator dispatch is isolated
to the top live popup branch in C and Go, including immediate restoration when
an explicitly closed popup is next declared.
TextField/TextArea editing now also respects top-popup keyboard ownership in
both runtimes. C's immediate keyboard-enabled check uses the active scope;
retained editor routing uses its declaration snapshot after scopes close.
Tests cover blocked parent and eligible child typing, including immediate and
retained C paths. Immediate C TextArea skips painting without a graphics window.
Ordinary Go Button now registers focus, focuses on pointer activation and
handles Enter/Space and Tab. C's activation check also consults registered popup
ownership, including deferred button events. Unclaimed Go Tab events route at
frame end so a disabled, absent or blocked focus owner cannot swallow traversal.
Generated C/Go button tests cover Enter/Space, disabled rejection and Tab skipping
a disabled control; popup tests cover parent/child keyboard activation.
Missing C popup input owners are now retired before frame-end focus filtering,
not only during paint cleanup. Matching C/Go tests omit a child and then its
parent and verify that Tab reaches the surviving parent/background that frame.
Popup keyboard capture is remembered for the C host frame. Unhandled character
input and queued text-edit commands expire at frame end after such capture,
including same-frame dismissal, instead of leaking into an underlying editor
later. Tests cover injected/platform-queued text, queued Backspace/Enter and
fresh input after dismissal. Non-popup queue behavior is unchanged.
C retained IME commits now respect read-only editors, and popup-captured
composition events expire with other blocked text input. Preedit is cancelled
on focus loss, read-only transition or popup keyboard capture. Regression tests
cover cancellation and no commit replay after dismissal. Native Go now owns a
composition queue and per-editor preedit records. TextField/TextArea display
preedit separately from committed buffers and insert UTF-8 commits; focus loss,
removal, disabling and popup capture discard preedit. A native generated
composition fixture verifies non-mutating preedit, commit and cancellation in
C and Go. Device-level Go IME routing remains unfinished.
Native Go text props now carry ReadOnly, with mutation guards separate from
focus/selection/copy handling and read-only caret metadata. C retained editing
also guards ordinary typing, cut/paste and deletion, not just composition
commits. The generated composition fixture verifies read-only TextField and
TextArea buffers and copying in both native runtimes; Go tests cover preedit
cancellation, byte-for-byte buffer preservation and re-enabling without replay.
Native editing resolves movement intent into Unicode 17 extended grapheme
boundaries. The private C adapter compiles the pinned utf8proc submodule; the
native Go adapter uses clipperhouse/uax29 without cgo. Both run the same 766-case
official Unicode conformance corpus, including byte-offset cursor checks.
Generated C/Go forms additionally exercise deletion of combining text, joined
emoji, flags, skin-tone modifiers and Indic conjuncts. Committed selection and
navigation use graphemes; codec helpers, capacity limits and IME preedit retain
their existing byte/codepoint contracts. C click placement and editable wrapping
traverse clusters, while measurement and visible-line buffers preserve long
UTF-8 ranges instead of truncating them at a fixed byte count.
Native Go wrapped `Text` retains measured line results in a per-runtime cache
bounded by both entry count and text storage. Text, width, font identity/size and
spacing form the key; font registration and active-font changes invalidate it.
The cache does not own wrapping policy, which remains generated from
`runtime/paragraph.kry`. Workloads, limits and measured results are documented in
[`PERFORMANCE.md`](PERFORMANCE.md).
Accessibility snapshots now have matching C/Go editor metadata, explicit focus
IDs and password-value protection. C projects the retained tree; Go projects
semantic frame operations without treating checkbox/toggle decoration as extra
controls. Composed button text is attributed to its owning button. Host sinks
are called for empty frames (including C's implicit screen root) so consumers
can remove stale controls. Generated-form
assertions compare editor roles, values, focus and secure-field behavior in both
runtimes. `runtime/accessibility_policy.kry` owns shared action eligibility;
hosts own bounded request queues, snapshot-generation validation, and next-frame
delivery into ordinary focus/activation sampling and editor input handling.
Text actions own bounded payloads, replace atomically against live byte/scalar
limits, and normalize selection to grapheme boundaries. Applying them cancels
stale composition and preserves ordinary editor change reporting. Snapshots
publish committed selection offsets, but omit secure values and offsets.
Removed or newly ineligible controls cannot replay pending requests. Native OS
accessibility object trees and platform adapters are not yet implemented.
Native Go runtime creation now resolves Kryon's Noto Sans UI face from packaged,
development-tree, or standard system locations before falling back to the
minimal bitmap renderer. This matches the C host's default-font policy while
preserving explicit `RegisterTextFontData` / `UseTextFont` overrides.
Private paint scopes share a drawing-order guard across host contexts while
keeping texture ownership host-local. Token generations survive host destruction
and allocation reuse. Invalid closes are rejected before restoring drawing state.

C dropdown identity records now have stable dynamically allocated storage,
rather than a fixed 24-control array whose overflow reused the first control.
Frame-end overlay processing retires records for owners no longer declared.
C and Go tests declare 41 controls, open/select/reopen the first, and remove it
while checking independent selections and capture release. C dropdown storage
is still process-global; per-window ownership remains to be consolidated.
Options now use dynamically sized owned records and strings, removing the
128-option, 255-byte label and 31-byte font-name truncation limits. Unchanged
strings reuse their allocations; shrinking lists and retired controls release
their owned storage. Overlay painting visits visible rows only, and content
height saturates at the runtime's integer coordinate limit.

The private C `ui_scrollbar` helper handles interaction independently of window
painting. Dropdown overlays use base input capture so their own popup capture
does not block the thumb, while ordinary scrollbars retain full popup capture.
Dismissal and owner retirement cancel a dropdown's active scrollbar before its
offset storage is released. Scrollbar drags are distinct from popup content
drags and cannot select an option on release. The shared C scrollbar drag slot
is still process-global; this does not complete per-window input ownership.
Dropdowns process and paint the scrollbar after the panel background but before
emitting rows, so both use the updated offset in the drag frame. Row painting
is clipped to the content area, excluding popup padding and the scrollbar.
The shared `runtime/dropdown.kry` popup policy constrains horizontal placement
and width to the UI view. Hit testing, capture and painting share the popup
rectangle rather than assuming that its horizontal bounds match the owner.

The shared KIR parser lowers `.kry` `Disabled { when = condition ... }` blocks
to existing runtime calls and lexical cleanup. The same cleanup pass used for
`defer` closes the scope on normal exit, return, break and continue before any
backend emits code. The generated buttons parity fixture exercises those exits
and nested disabled inheritance in C, Go and JS; C++ syntax coverage checks the
same shared lowering. Native `Scroll` blocks use the same lexical cleanup and
lowered host scroll support. An optional block name binds the returned content
rectangle without leaking it beyond the block. Native generated scroll parity
covers nested clips/input and restoration after return, break and continue.
Native `Popup` blocks similarly lower caller-defined popup contents to
conditional begin calls plus cleanup-managed end calls, removing manual scope
bookkeeping from `.kry` sources while retaining the small runtime contract.
Other begin/end APIs have not all gained block syntax.

The C widget implementation is still being consolidated. Some constructors
submit retained paint, while others register generic nodes and draw immediately.
Do not treat the presence of a tree node as proof that a widget participates in
deferred painting.

`Button` and `Slider(SliderProps)` resolve
their declaration bounds before handling input and submit typed paint data when
building a tree. The slider painters do not process input. Labels and formats
are copied into node-owned storage; value arrays remain caller-owned. Without
a tree, the same input and paint helpers run immediately. This preserves the
public API while removing the separate prefixed slider draw entry points.
Angle sliders retain the caller's radians pointer and convert to degrees only
within input or paint execution; no pointer to a temporary conversion survives
declaration.

`Drag(DragProps)` follows the same split, including owned label/format storage
and resolved bounds. Range mode composes two ordinary typed drag nodes inside a
Row, plus retained Text for the label. Children preserve the existing input IDs
and borrow the caller's endpoint pointers; no range-specific renderer or
prefixed drag draw entry point remains.

`Text(TextProps)` uses one typed retained text node with owned text, captured
font selection, and resolved bounds. Its painter measures intrinsic lines or
wraps bounded text, clips to positive bounds, and applies alignment, color, and
disabled presentation from the same property object. Headless declaration does
not invoke a graphics backend. There are no separate public colored, disabled,
wrapped, or in-rectangle text widget implementations.

The remaining families must migrate with evidence for layout, input timing,
paint order, disabled/clip scopes, and data lifetime. Captured render textures
remain private implementation machinery behind the public dropdown scope, not a
low-level public paint API or the completed widget architecture. Native Go keeps
its own implementation and must retain matching behavior through
generated-runtime parity coverage.

Numeric editor state uses collision chains keyed by numeric type, widget ID,
and component index,
with stable record addresses. A bucket is not a single replaceable editor slot:
distinct keys must preserve independent text, cursor, and focus. C and Go tests
cover 129 keys that share the C bucket index. Each component reserves three
separate numeric control IDs for its field, decrement, and increment controls;
vectors larger than 16 components do not alias neighboring numeric widgets.
Tests cover 396 component keys and the emitted IDs for a 17-component input
beside another input. C's process-wide numeric state still needs
per-window lifetime integration as numeric widgets are consolidated.
Numeric ID allocation is not yet a shared identity service for every widget
family or arbitrary application-supplied control IDs.

Numeric input wrappers resolve their bounds before invoking the editor and do
not retain pointers to stack-local props. Their increment/decrement controls
use standard `Button` submissions, so step handling is independent of graphics
availability and participates in retained painting and disabled scopes. C and
Go tests cover normal, fast, and disabled stepping inside Row layout. Numeric
text editing also runs without a graphics window and returns changes during
declaration. Its painter receives an owned display-text snapshot plus caret,
selection, scroll, style, and font information; retained painting does not
process editing input again. Disabled typing is discarded, not replayed on
re-enabling the field. The ordinary retained TextField input-routing path and
numeric editor input path are still separate; consolidation is not complete.

Native Go collects deferred popup paint in `go/kryon/paint_layers.go`, using
ordinary frame operations rather than dropdown-specific drawing records.
Dropdown placement comes from `runtime/dropdown.kry`; capture and painting share
the resulting constrained, optionally upward-facing rectangle. Popup rows use
the ordinary scroll container, with runtime-owned stable offset storage that
is released on dismissal or owner removal. Keyboard navigation reveals rows
without overriding wheel scrolling on idle frames; only visible rows are emitted.
Nested layers paint above their parents and restore the surrounding layout,
clip and disabled state. Existing dropdowns use this collector and the private
`popup_input.go` registry for scoped click ownership. That registry tracks
parent/child and sibling order, preserves capture before owner declaration in
the following frame, and removes descendants on closure or owner removal.
Native Go scroll scopes, lists, trees and tables share a pointer
reachability check for wheel input, respecting popup ownership, disabled state
and parent clips. Generated C/Go tests verify that a background scroll scope
declared before an open dropdown cannot steal its wheel input.
Go drag-and-drop sources and targets also use that pointer check for starting
and accepting a drag; an already active source retains its payload when the
pointer leaves its original bounds. Rejected targets do not consume the release
or payload. Generated C/Go tests cover clipped sources/targets and copied data
lifetime across press and release.
The public arbitrary-content dropdown scope integrates these paint and input
registries in C and Go. Generated C/Go execution and k2cpp syntax coverage use
the same clean calls. Popup focus acquisition/restoration and active scalar,
slider, splitter and table-resize ownership are covered in both native runtimes;
device-level integration remains unfinished.

## Backends

Backend selection is controlled by `KRYON_BACKEND`. The default native backend
uses raylib through Kryon's compatibility surface. Other backends are expected
to implement Kryon behavior through the same public/runtime contracts, with
their support documented in `docs/BACKEND_CAPABILITIES.json`,
`docs/BACKENDS.md`, and `docs/FEATURE_MATRIX.md`.

## Formats And Tools

The `.kry`, KIR, generated C, generated Go, and KRB paths are Kryon-owned
tooling surfaces. Tool changes should update the matching specs, conformance
matrix data, examples or fixtures, and generated documentation when applicable.

## Examples And Fixtures

Examples under `examples/` should remain generic and readable. They demonstrate
Kryon capabilities, not downstream product screens. Exact rendering and parity
fixtures should be generic enough that they can run in any app-independent
Kryon build.

## Live Preview

`cmd/kryon-preview/session.c` owns build processes, private session directories,
diagnostics, and loaded app hosts. The watch loop polls builds without blocking
rendering. A candidate is copied to a unique library path and must load, expose
both host lifecycle symbols, accept the ABI, and provide a draw callback before
the old host is destroyed. Failures preserve the old host; successful swaps do
not migrate application state. Closing cancels the build process group and
removes session files. Arbitrary side effects inside an application's host
constructor are outside this replacement guarantee.

`watch.c` fingerprints project source metadata, ignoring generated and vendored
trees. `cmd/kir/kir_diagnostic.c` owns shared frontend diagnostic formatting;
compiler drivers select text or JSON Lines, and preview reads the structured
records without parsing human error strings. Session and diagnostic regression
tests run in `make fast-test` and `make test`. `make preview-watch-test` uses
Xvfb to exercise a real generated app through a syntax failure and recovery.

## Tests And Matrices

Kryon uses several test layers:

- boundary and naming checks for repository hygiene
- public API snapshot checks for app-facing identifier drift
- public header compile checks for app-facing include hygiene
- examples manifest checks for example inventory and exactness fixtures
- backend capability checks for backend inventory drift
- generated-file checks for docs, compatibility headers, icons, and matrices
- parser, runtime, sync, update, platform, and widget tests
- conformance and visual matrix checks across renderers and runtime paths

`make generated-runtime-parity-test` executes generated C and Go fixtures with
interaction assertions and final-state comparisons. It runs in `make test`,
separately from the API inventory check `make runtime-parity-check`. Context
menu activation and outside-release dismissal in Go use the shared popup/menu
policies, including clipping, disabled content, and popup capture decisions.

Use `make preflight` before committing focused Kryon changes. Use `make test`
for the broader local regression suite. Use `make test-asan` or
`make test-ubsan` for sanitizer-backed regression runs, and
`make release-preflight` before publishing release artifacts.

## Downstream Integration

Downstream applications vendor Kryon as a submodule. Permanent Kryon changes
must be made and committed in this repository first, then brought into apps by
updating the submodule pointer. Never edit a downstream `vendor/kryon` tree as
the source of a Kryon change.

### Material raster cache

The raylib host caches rasterized surface layers in a bounded LRU (128 entries,
16 MiB, at most 1 MiB per layer). The key contains the resolved layer colors,
geometry, scale, segment, and fractional placement. Integer translation reuses
the raster; style, focus, animation, size, or scale changes produce a new entry.
The shared `.kry` sampling functions still define every pixel. Other backends
and oversized layers use direct drawing. Textures retain straight alpha and
are drawn under the caller's current clipping and blend state. Window shutdown
releases them before destroying the graphics context.

This avoids resampling unchanged materials and submitting their individual
scanline rectangles. It does not skip declaration, reconciliation, or whole-frame
composition. `BeginTree` still requests paint because the host may clear its
framebuffer each frame; removing that invalidation would erase unchanged UI.
Retained subtree composition is a separate architectural change.

### Declarative web scope teardown

Generated web nodes retain parent-path metadata for their declarative scope.
Native paint-scope end markers are consumed during JavaScript lowering; they
are not emitted as recorded statements or exported runtime wrappers. Disabled
content additionally uses a runtime input stack and keeps its paired teardown.
Nested input/restoration behavior still requires backend parity tests beyond
checking the generated names.

Native composed Canvas scopes now save their camera and clipping state per
scope. An untransformed nested canvas cannot pop its parent's camera, and
closing an explicit nested camera restores the previous backend matrices.
Go applies generated Canvas coordinates to its paint operations and uses the
same nested clipping rules. The compiler's return/break/continue cleanup is
executed by the generated Scroll/Canvas fixture.

`runtime/text_rows.kry` owns editable visual-row decisions and selectable-block
word wrapping. Both native hosts supply grapheme boundaries and measured text
prefixes without normalizing the source bytes. C measurement, caret scrolling,
pointer placement and painting use one native iterator; Go rendering and
pointer placement use the matching adapter. Shared fixtures cover CRLF, blank
and trailing lines, whitespace, long words, combining marks and emoji families.
Go Paragraph's inline-icon adapter also consumes shared paragraph tokenization,
spacing and line assembly. Retained tree focus traversal now runs through
`TreeFocusBegin` and `TreeFocusAdvance` in C and Go; Go table sorting and tree-row
activation call their existing generated decisions.
## Native Go raster resources

`image_cache.go` owns bounded filesystem decoding; `image_render.go` samples and
blends pixels. Image fitting remains in `runtime/image.kry`. Frame operations
preserve source rectangles, fit, origin and rotation rather than discarding
them before rasterization. `text_rows_cache.go` shares immutable measured row
snapshots between editor painting and hit testing, invalidated by text, width,
font identity/size and the font registry generation.

The native Go runtime keeps configuration, instance storage, frame lifecycle and
input queues in `runtime.go`. Focused `*_host.go` modules adapt button, numeric,
menu, table, text, layout and other widgets to their generated `.kry` policy.
Generated policy files retain their module names. This split changes host code
organization without adding public widget entry points or another policy layer.

Native Go selectable text maps measured paragraph runs back to original UTF-8
byte offsets. Shared `.kry` pointer/drag/copy decisions control native range
storage; Unicode segmentation, hit measurement and clipboard storage stay in
the host. Linux X11 releases end a drag without injecting a second press. IBus
focus lifetime follows the eligible editor, and candidate geometry reuses the
painted row layout, font and scroll offset.

Editor and surface rasterization consumes resolved style presence bits. Native
Go does not add debug widget fills or borders; explicit zero alpha and opacity
remain transparent. C immediate text areas, avatar tiles, menu panels and
navigation bars use resolved material painting. The constant default-style
switch and its unreachable alternative renderers have been removed.

# Composed popup implementation requirements

Status: the public native `BeginCombo` / `EndCombo` / `CloseCombo` and generic
`BeginPopup` / `EndPopup` / `ClosePopup` scopes are implemented in C and Go,
with k2c, k2cpp and k2go generated coverage. The
remaining lifecycle and backend gaps below still apply. This is an
implementation checklist, not a completion claim or a restriction of the
native ImGui widget goal.

## Contract

Dear ImGui's [combo API](https://github.com/ocornut/imgui/blob/master/imgui.h)
explicitly separates the option-list convenience helper from a begin/end scope
whose contents and selection are caller-controlled. The upstream header was
checked on 2026-09-05. It also exposes popup alignment and height choices,
arrow/preview suppression, and fitting the width to the preview. Existing
Kryon option arrays do not establish support for that scope or those flags.
The same upstream header exposes arbitrary begin/end popup and tooltip scopes;
Kryon's generic popup scope now covers caller-defined non-modal popup contents,
non-input-capturing hover tooltips through `PopupTooltip`, and arbitrary modal
contents through `PopupModal`. A context-popup begin scope remains open work.

The clean native scope must accept ordinary native controls and nested layouts,
not a second set of popup-specific widget aliases. C, C++ codegen over C, and
native Go generated callers must share the same behavior. Selection and editing
remain in caller state. A successful begin must have one matching end; a closed
popup must not submit children.

## Verified architectural constraints

- `src/ui/ui.c:DrawUIFrameOverlays` runs dropdown, menu and text-context overlays
  after ordinary UI content and resets the drawing clip first.
- `src/ui/ui_tree.c:EndTree` performs layout, input and painting after the app's
  widget calls. A content scope must therefore survive beyond its lexical end
  for retained painting and hit testing, without retaining borrowed props.
- Native controls also paint immediately. Reordering retained nodes alone is
  insufficient: it would omit immediate controls from the popup paint layer.
- `src/ui/ui_window.c:BeginUIWindow` already enters a render texture before
  `BeginUIFrame`; its end path calls `EndUIFrame` before ending that texture.
  An overlay implementation must not restore the default framebuffer when the
  destination is an existing UI-window target.
- `SaveUIFrameState` preserves UI camera/input state, not the active graphics
  render target. It is not sufficient for nested paint-target restoration.
- Go now collects deferred `FrameOp` records in runtime-owned nested paint
  layers. Existing option-list dropdowns and the public combo scope use that
  collector and a private nested popup click-ownership registry. Full
  keyboard/active-drag routing is not implemented. Go's scrollable widgets
  now gate wheel input through popup ownership and the active content clip.
- C retained nodes now snapshot input clips and disabled scopes. Those snapshots
  must also be respected when retained nodes are painted into a popup layer.

## Required implementation sequence

1. Implement internal per-window paint-layer capture/restoration. Verify an
   existing offscreen target and nested layers before exposing a public scope.
   The capture must include immediate drawing and subsequent retained painting.
   Do not assume the destination is the main framebuffer.
   Transparent capture must accumulate alpha with source factor one, separately
   from RGB's source-alpha factor. Composite that premultiplied result without
   multiplying RGB by alpha a second time. Restore the caller's blend mode and
   custom factors, including around deferred retained painting.
2. Associate retained submissions with their layer and frame generation. Keep
   child props/state ownership valid through deferred painting; do not keep
   pointers to by-value props or temporary generated arrays after their lifetime.
3. Generalize Go's deferred popup records and both runtimes' pointer capture to
   nested scopes. Preserve the parent's layout, clip, disabled state, focus and
   paint destination on exit. Handle a missing owner and window destruction.
4. Add the clean combo scope and explicit close behavior in C and Go together,
   then add k2c/k2cpp/k2go generated fixtures and output-scanner coverage.
5. Exercise the upstream presentation choices instead of assuming the presence
   of an option-list helper covers them.

## Implemented prerequisite: nested raylib targets

The raylib-backed `BeginTextureMode` / `EndTextureMode` implementation now tracks
targets entered through Kryon's API and restores the parent target, its full
viewport, and the saved projection/modelview matrices. Tracking targets avoids
depending on `rlGetActiveFramebuffer`, which returns zero in the GLES2 build.
Every begin must be balanced with an end before closing the window.

`tests/texture_scope_test.c` verifies three nested offscreen targets with real
pixel readback, including drawing resumed under a translated camera. It also
opens a real `UIWindow`, nests two targets inside it, and checks the actual X11
presenter's texture readback for preserved background and subsequent drawing.
A test-only linker wrapper observes that readback without substituting a mock
or exposing the window's private target. This integration test requires the
raylib/X11 window path and a linker supporting `--wrap`.
Run it with `make texture-scope-test` on a display, or build the test binary and run it under
`KRYON_SHOT_ARM=1 xvfb-run -a` on headless Linux. Screenshot capture must be armed
to check the completed main framebuffer as well as the offscreen textures.

Retained nodes can now snapshot a borrowed paint destination selected by the
internal `ui_tree_set_paint_target` hook. Each captured node also records the
projection and modelview active at declaration. A private frame-local table
shares consecutive identical snapshots; ordinary nodes only carry a zero index.
`DrawTree` enters the captured target and restores those matrices for the node's
deferred painting, then returns to the caller's target and matrices. The hook
returns the previous destination for nested restoration, resets at `BeginTree`,
and forces repaint so clearing a captured texture does not lose unchanged nodes.
The texture must remain alive through `EndTree`; the hook neither allocates nor
owns it. Immediate drawing is still captured by the caller's texture scope.

The pixel test combines immediate drawing and retained `Rect` submissions,
including a nested destination and translated retained content before and after
that nested scope, followed by opaque ordinary retained content.
It composites the captured target last and checks both frames of an unchanged
tree. This proves that both kinds of paint reach the chosen destination, not
that a popup automatically owns or composites that destination.

This is not a complete paint-layer implementation. It does not restore
arbitrary custom viewports or raw framebuffer bindings, or provide per-window
popup ownership. Scope-level clip, shader, blend and input-state isolation
still need explicit handling in the future popup layer. The SDL and Win32
window presenters, final OS-window pixels, and per-window popup ownership have
not been verified by this test.

## Verified C translucent compositing prerequisite

`tests/texture_scope_test.c` now checks half-transparent immediate and retained
rectangles captured over transparent black. It verifies the captured RGBA values
and compares the composite over an opaque parent against direct source-over
drawing, pixel for pixel. The GLES2 test passes with separate RGB/alpha capture
factors and premultiplied compositing. The backend's ordinary alpha mode uses
one factor pair for both RGB and alpha, so it is not sufficient for transparent
layer capture: a source alpha of one half becomes approximately one quarter.

This test sets backend blend factors explicitly through a test-only declaration;
it does not add a public low-level API or implement automatic layer ownership.
Retained target captures now also snapshot blend state and restore it for each
captured node, then restore the caller's blend state. The test ends the capture
blend scope before `EndTree`, activates a different caller blend configuration,
and configures another pending set of custom factors. It checks exact restoration
of both active GPU state and pending renderer configuration after deferred paint.
Kryon's build-preparation hook compiles private helpers inside the copied rlgl
implementation; the raylib submodule and public generated APIs are unchanged.
This implementation covers the OpenGL 3.3/GLES2 paths, with real pixel evidence
for GLES2. The future owned layer still needs scope-level blend restoration and
automatic composition, plus the remaining resource/window ownership work.

## Implemented prerequisite: private C owned paint layers

`src/ui/ui_paint_layers.c` owns render textures in an explicit host context.
Opening a layer reserves its order and captures immediate drawing plus retained
submissions into the same texture. Nested layers composite above the parent's
later drawing. The context reuses textures, replaces them when dimensions change,
and releases unused entries after the frame is composited. Hiding a parent
suppresses its descendants; a frame without an owner emits none of its old paint.

Capture establishes separate source-over alpha factors. End restores the prior
target, clip and blend state; composition uses premultiplied alpha and restores
the caller's matrices, clip and blend configuration. Resource allocation and
destruction also preserve the active framebuffer, since the backend's texture
helpers bind and unbind framebuffers internally. The host must finish retained
painting before compositing or destroying the context. Scope tokens check frame
generation and last-opened/first-closed order across all active host contexts,
before touching backend state. Generations are allocated across context lifetimes
so a destroyed host's token cannot become valid if its memory address is reused.
Unix subprocess regressions require rejection of null, stale, double-close,
destroyed-host and cross-host out-of-order tokens; valid nested scopes continue
through the real framebuffer checks.

The real framebuffer test covers translucent immediate content, retained content
above later opaque main content, child-over-parent ordering, hidden parents,
missing owners, and resumed drawing under a transformed camera and clip. This
is exposed through the public combo scope. C layer scopes suspend the
owner's retained layout path through `ui_tree_layout_suspend` and restore it on
exit. Popup nodes stay attached to the screen root in the same retained tree,
but do not consume parent Row/Column slots. Declaration-generation and balanced
child-layout checks reject ending a layer after changing its tree or leaving a
child layout open. A headless node/position regression and real pixels verify
an independent popup Column origin and the resumed parent Row's next position.
C layers also isolate disabled-scope depth while inheriting the parent's disabled
state. An internal floor prevents a child's `EndDisabled` from unwinding its
parent. Ending the layer requires balanced child disabled scopes, then restores
the parent's depth and disabled origin. Headless tests cover enabled/disabled
parents and nested scopes; the framebuffer test checks inheritance across host
contexts and rejects an unclosed disabled child scope. C layer scopes now also
suspend input clips and scroll-scope depth, leaving modal/input capture intact.
Their child scroll scopes must balance before exit restores the owner's clips.
Ordinary button regressions verify interaction outside the owner's viewport,
restored background clipping and rejection by a modal capture. Real pixels
verify mixed popup content escaping a 1x1 scrolling owner; invalid-scope tests
reject an unclosed child scroll. Full focus ownership, active-drag routing
and shader isolation remain work.

The private C `ui_popup_input.c` registry now tracks persistent popup bounds,
parentage and branch ordering in explicit contexts. The shared input-capture
path consults its bound context, allowing ordinary controls to participate.
Records preserve capture before the next frame's owner declaration; closing a
parent disables descendants, and frame completion retires missing owners.
Headless button tests cover background controls before/after the owner, a child
receiving input instead of its parent, child dismissal, context rebinding,
missing owners and reordered sibling branches. Go's corresponding popup tests
remain a regression gate. C paint hosts now own their input registries: starting
a host frame advances and binds its registry, composition retires missing owners
and restores the previous binding, and destruction releases the registry.
Auxiliary windows establish their own context even before declaring a layer
when a parent context is bound. Hiding a layer closes its matching input branch.
The graphical test checks capture before redeclaration, auxiliary isolation,
parent restoration and missing-owner retirement. Retained nodes now keep private
declaration-time input snapshots, separate from paint snapshots, for deferred
hit testing and button hover/press state. Routing does not reopen lexical scopes.
The committed tree keeps its snapshots while a replacement is being declared;
registry generation/liveness checks reject old and destroyed owners. Headless
tests cover child precedence over later parent/background declarations and
dismissal restoring the parent then background. Deferred pointer-focus
registration now uses the same snapshots, retaining modal, clip, disabled and
inspection gates. A button regression clears immediate focus before the deferred
pass and verifies child focus versus modal blocking after both scopes close.
It also checks deferred click events. The test initially reproduced a click
leaking through modal capture; hit testing and hover/press state now consult the
same full capture predicate as pointer-focus registration.
This does not establish automatic popup focus acquisition/restoration or full
active-drag routing. The public combo scope uses this ownership registry.

C and Go now select the top live popup branch for keyboard eligibility without
testing pointer coordinates. Closed combos consult this check before accepting
keyboard opening. Matching native tests cover a focused combo in the parent
versus the top child, and keyboard eligibility after child and branch dismissal.
This check does not cover every button/editor input path or establish full
keyboard ownership or focus restoration.

TextField/TextArea editing now consults top-popup keyboard ownership in both
native runtimes. C's immediate keyboard-enabled query is scope-aware; retained
editors resolve their saved input owner and reject closed/stale owners before
editing. Matching typing tests cover blocked parent and eligible child editors,
with both immediate and deferred C variants. Immediate C TextArea skips paint
when no graphics window exists, matching TextField's headless editing support.
Tab order, complete shortcut coverage, focus restoration and input replay after
dismissal still require dedicated integration and tests.

Focus registrations now retain popup ownership separately from the lexical
scope. C stores frame-local registrations in the host input registry and
filters/deduplicates Tab destinations before releasing the host binding. Go
keeps ownership alongside current/previous-frame focus order and prunes removed
registrations at frame end. Matching tests cover forward/reverse wrapping,
parent/background exclusion, duplicate registrations and traversal after
explicit child/branch dismissal; Go additionally sends Tab through an editor.
These tests do not establish initial popup focus acquisition, automatic focus
restoration on dismissal or a complete cross-window keyboard lifecycle.

Ordinary Go Button now participates in focus order and handles Enter/Space and
Tab; unclaimed Tab events route at frame end after destination registration.
C's keyboard activation predicate checks the registered popup owner, so retained
button events work after lexical scopes close without activating the parent.
Matching popup tests cover Enter/Space in parent and child controls. The native
generated `buttons_layout.kry` runners additionally verify enabled/disabled
keyboard activation and Tab skipping a disabled button. Their shared JavaScript
comparison remains pointer-only for this fixture.

Missing C input owners are retired before focus finalization chooses a Tab
destination, while record/resource cleanup remains in its existing finish phase.
A regression reproduced focus being cleared when a missing child still blocked
its surviving parent. Matching native tests now omit the child and then parent
across frames and verify same-frame traversal to the parent and background.
Automatic restoration without a navigation event remains unimplemented.

Text input lifetime now has dismissal regressions. C reproduced blocked text
reappearing in the underlying editor after a popup closed. The host registry
remembers keyboard capture for the frame, and EndUIFrame expires leftover native
characters and queued text-edit commands before releasing that binding. The
marker survives same-frame dismissal and resets at the next host frame; ordinary
non-popup queue behavior is unchanged. Tests cover immediate TextField/TextArea,
injected and platform-queued characters, queued Backspace/Enter for TextField,
and fresh input after dismissal. Go's frame cleanup passes the matching text
test. IME composition and Android device delivery are not established here.

C retained composition now has focused regressions. Popup-captured composition
events expire at frame end alongside other unhandled text, preventing delayed
commit after dismissal. Commits cannot mutate read-only TextField/TextArea
buffers. Preedit is cancelled when the retained editor loses focus, becomes
read-only or is blocked by popup keyboard ownership; tests check cancellation
events and no revival after re-enabling. Immediate C composition and device-level
IME delivery remain unfinished.

Native Go now has runtime-local composition queues and per-editor preedit state.
TextField/TextArea display preedit without mutating caller buffers, and commits
use UTF-8 insertion/cursor handling. Tests cover queue isolation and limits,
multi-frame preedit, commits, cancellation, removed/disabled/unfocused editors,
and popup dismissal without replay. The native generated `composition.kry`
fixture verifies non-mutating preedit, UTF-8 commit and cancellation through C
and Go; the JavaScript runner does not execute it. Go OS-window event delivery,
detailed preedit cursor/selection rendering and C TextArea preedit rendering
are still incomplete.

Read-only editing now has matching native generated coverage. The composition
fixture declares read-only TextField and TextArea in C and Go, checks copying,
and rejects ordinary text, cut/paste, deletion and IME commits. Go props carry
ReadOnly through editing and frame metadata; focused read-only controls keep
their focus styling without an insertion caret. C retained mutation paths also
honor read_only. Go tests check preedit cancellation and no rejected-input replay
when the editor becomes editable again.
The same framebuffer test interleaves two host contexts using the same owner ID,
resizes one context from 32 to 64 pixels, reuses the other context's resources,
and destroys one while the other destination is active. It verifies both hosts'
immediate/retained pixels and subsequent drawing. The real X11 `UIWindow`
presenter readback now also checks an owned layer composited over later opaque
retained content. Native `UIWindow` implementations lazily own their layer context:
begin starts its frame, end composites before presentation, and close destroys
it. Closing the active window first finishes its frame. The private accessor
returns no context outside an active window; callers no longer manage its
frame/composition/destruction. Four presenter readbacks cover consecutive frames,
an omitted owner and closing an active frame. X11 runtime and SDL compile checks
pass; Win32 runtime behavior remains unverified. This does not isolate C widget
or input state.

The main UI frame now lazily owns a separate context through the same private
frame accessor. `SetUIFrame` starts its layer frame and `EndUIFrame` composites
it; `CloseWindow` releases its textures before closing the graphics context.
UIWindow frames route to their own owner and do not end the main context. The
pixel test covers main-frame immediate/retained content, a missing owner,
interleaving an auxiliary window, and closing/reopening the graphics context.
Presenter texture readback preserves its caller's framebuffer, so interleaving
an auxiliary presentation does not redirect the main frame's later composition.
Callers of private internals still must balance layer scopes and finish retained
painting before host finalization. The public combo scope owns that balancing
for ordinary callers and rejects an unmatched `EndCombo`.

## Implemented prerequisite: native Go nested paint collection

`go/kryon/paint_layers.go` replaces the dropdown-specific paint map with an
ordered collection of ordinary `FrameOp` layers. A layer reserves its position
when opened, so nested popup paint follows all parent paint, including parent
widgets declared after the nested scope. Independent later layers paint last.
Layer scopes isolate layout and scrolling clips, inherit disabled content, and
restore the parent's layout, clip and disabled stack on exit. Their tokens are
runtime- and frame-specific and enforce last-opened/first-closed order.

The frame-end visibility predicate suppresses a closed or missing owner and
all of its descendants. Beginning a new frame discards previous paint records.
Existing dropdowns submit through this collector and retain their own input
and dismissal implementation. `paint_layers_test.go` verifies nested and
sibling order with software-rendered pixels, parent layout/clip restoration,
disabled-state inheritance/restoration, removed-parent suppression, frame
reset, and rejection of foreign, stale and unbalanced scope tokens. The native
Go suite and generated-runtime parity fixture for the existing dropdown path
remain regression gates.

The public Go combo scope now builds on this collector. Native tests cover
ordinary buttons, checkboxes, editable fields and nested layouts, including
nested dismissal, missing owners, layout restoration and presentation flags.
The shared `.kry` fixture is executed through generated C and Go and is also
syntax-checked through k2cpp.

`go/kryon/popup_input.go` separately generalizes click ownership. Its persistent
runtime-local registry preserves capture before the owner is declared on the
next frame. Input scopes restore the active parent, and overlap priority follows
whole popup branches rather than map iteration or leaf timestamps. Closing a
parent removes descendants; a missing owner is pruned at frame end. Ordinary
button tests cover nested overlap, child closure, background controls before
and after the owner, sibling reordering, owner removal, and runtime isolation.
Existing dropdown click consumers use this registry. Scroll scopes, list boxes,
trees, source views and tables also use its ownership check for wheel input,
alongside disabled-state and clip checks. Unit tests verify that each family
rejects covered background scrolling while allowing its popup-owned counterpart
to scroll. Generated native C/Go regression coverage opens a combo over an
earlier scroll scope and checks that the latter's offset remains unchanged on
wheel input. Go drag-and-drop start/accept paths now also respect popup and clip
ownership; tests verify that rejected background targets leave the release and
copied payload available to the popup target. Scalar-slider/resize active-drag
ownership and automatic popup focus restoration remain unfinished. Composed
dismissal covers explicit close, Escape, outside pointer release and owner
removal.

## Acceptance evidence

| Case | Required evidence |
|---|---|
| Mixed contents | Generated button, checkbox, editable field and nested Row with independent caller state |
| Layer order | C and Go pixel checks with opaque content drawn after the combo owner |
| Existing render target | Native C offscreen/UI-window test proving the parent target and later drawing are preserved |
| Nested popup | One level closes without corrupting the parent's capture, layout or destination |
| Scrolling owner | Popup escapes the owner's clip while ordinary owner content remains clipped |
| Pointer ownership | Background controls declared both before and after the owner cannot steal popup input |
| Editing lifecycle | Focus, typing, disabling, re-enabling and no replay of discarded input |
| Dismissal | Escape, outside press, selection-close, missing owner and destroyed window |
| Presentation choices | Alignment, size policies, preview/arrow suppression and preview-fit width |

Headless state tests cannot prove paint-target restoration or layering. Existing
string-list popup tests remain regressions but cannot replace these checks.

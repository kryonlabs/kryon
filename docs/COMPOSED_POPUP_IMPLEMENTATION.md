# Composed popup implementation requirements

Status: unimplemented. This is an implementation checklist, not a completion
claim or a restriction of the native ImGui widget goal.

## Contract

Dear ImGui's [combo API](https://github.com/ocornut/imgui/blob/master/imgui.h)
explicitly separates the option-list convenience helper from a begin/end scope
whose contents and selection are caller-controlled. The upstream header was
checked on 2026-09-05. It also exposes popup alignment and height choices,
arrow/preview suppression, and fitting the width to the preview. Existing
Kryon option arrays do not establish support for that scope or those flags.

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
  layers. Existing option-list dropdowns use that collector and a private
  nested popup click-ownership registry. The public composed scope and full
  keyboard/active-drag routing are not implemented. Go's scrollable widgets
  now gate wheel input through popup ownership and the active content clip.
- C retained nodes now snapshot input clips and disabled scopes. Those snapshots
  must also be respected when retained nodes are painted into a popup layer.

## Required implementation sequence

1. Implement internal per-window paint-layer capture/restoration. Verify an
   existing offscreen target and nested layers before exposing a public scope.
   The capture must include immediate drawing and subsequent retained painting.
   Do not assume the destination is the main framebuffer.
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
popup ownership. Clip, shader, blend and input-state isolation
still need explicit handling in the future popup layer. The SDL and Win32
window presenters, final OS-window pixels, and per-window popup ownership have
not been verified by this test.

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

This private paint collector is not `BeginCombo`/`EndCombo` and does not
establish C/Go parity for arbitrary popup
children. C still needs ownership/compositing of captured paint destinations,
and both runtimes need the public scope, nested dismissal and generated tests.

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
ownership, keyboard focus and composed dismissal remain
unfinished.

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

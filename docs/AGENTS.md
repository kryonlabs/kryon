# File UI Toolkit Agent Guide

This document is for agents and maintainers using File UI Toolkit inside applications such as
Inbe. It describes the behavior File UI Toolkit owns, what application code should keep, and
how to update downstream projects without editing vendored copies by hand.

## Role

File UI Toolkit is a small immediate-mode UI layer on top of raylib. Applications should use
File UI Toolkit for shared UI behavior instead of duplicating pointer capture, modal backdrop,
button, tab, scroll, text input, theme, and DPI logic in each project.

Keep application code focused on product state and domain behavior. Move repeated UI
interaction rules into File UI Toolkit when more than one screen or project needs them.

## Frame Lifecycle

Call `BeginUIFrame(width, height, dpi)` once at the start of a normal
screen-space UI frame. If the application uses a transformed UI camera, call
`InitUI(width, height, dpi)` and then `SetUIFrame(camera)` instead. File UI Toolkit sanitizes
invalid cameras before using them; a zero-initialized `Camera2D` is treated as
`GetUIDefaultCamera()` so pointer input does not silently break.

Beginning a frame resets per-frame interaction state:

- input blocking
- modal capture state from the previous frame
- input clip stack
- pointer gesture state
- clip state

Do not persist `ui_set_input_blocked` or input clips across frames. Register them again
while rendering the UI that needs them. Modal capture is registered while drawing the
modal and File UI Toolkit carries it into the next frame. Until the current frame registers the
modal bounds again, File UI Toolkit captures all pointer input so controls underneath the modal
cannot receive clicks before the modal is drawn.

## Input Capture

Every clickable File UI Toolkit control should gate pointer interaction through
`UIInputCapturesClick(point)` or the lower-level helpers already used inside File UI Toolkit.
That keeps scroll drags, dropdowns, modal backdrops, and explicit input blockers from
leaking clicks to the screen underneath.

Use these APIs by responsibility:

- `UIInputCapturesClick(point)`: normal public check for app or component code.
- `ui_base_input_captures_click(point, include_pointer_drag)`: internal/specialized
  components that need to decide whether pointer dragging should count as capture.
- `ui_set_input_blocked(blocked)`: full-frame blocking for overlays that should stop
  all controls for the rest of the frame.
- `SetUIModalCapture(bounds)`: modal-specific capture that blocks background
  clicks, then allows controls inside the active modal rectangle after the modal
  registers its bounds. It applies immediately and to the next frame.

## Modal Rules

File UI Toolkit modal helpers own backdrop capture:

- `DrawUIActionModal`
- `DrawUIModal`
- `DrawUIModal3Button`
- `DrawUIModalFrame`

When an application uses one of those helpers, it should not add separate outside-click
guards or duplicate backdrop blocking. The helper registers the modal bounds for the
current frame and the next frame.

Prefer `DrawUIActionModal` for title, message, and action-button dialogs. It measures
button labels, fits text inside buttons, and wraps actions into multiple rows when
localized labels do not fit. Prefer `DrawUIModalFrame` for custom modal content.
Use manual
`SetUIModalCapture` only for specialized overlays that cannot use the shared modal
frame.

When an application draws a custom modal manually, it must call
`SetUIModalCapture((Rectangle){x, y, w, h})` immediately after calculating the modal
rectangle and before drawing modal controls. This prevents clicks outside the modal from
activating the underlying screen, prevents controls underneath the modal from receiving
clicks before the modal is drawn, and keeps buttons, sliders, dropdowns, and text fields
inside the modal usable.

Do not manage modal click blocking in application-level route code. The application
should decide whether a modal is open and what state changes happen after a modal
button is clicked; File UI Toolkit should decide whether pointer input is captured.

## Custom Modal Pattern

```c
int modal_w = ScaleUIPx(320);
int modal_h = ScaleUIPx(240);
int modal_x = (view_width - modal_w) / 2;
int modal_y = (view_height - modal_h) / 2;

SetUIModalCapture((Rectangle){
    (float)modal_x, (float)modal_y, (float)modal_w, (float)modal_h
});

DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
DrawRectangle(modal_x, modal_y, modal_w, modal_h, GetThemeSurface());
```

Before this call on a frame following an active modal, File UI Toolkit captures all pointer input.
After this call, normal File UI Toolkit controls inside the rectangle remain clickable because
their hit tests are inside the active modal bounds. Controls outside the rectangle see
the click as captured in the current frame and the next frame.

## What To Move Into File UI Toolkit

Move behavior into File UI Toolkit when it is general UI behavior rather than product behavior:

- repeated modal shell drawing or backdrop capture
- repeated button hit testing, hover, disabled, and focus handling
- repeated scroll clipping or scrollbar handling
- repeated dropdown open/capture behavior
- repeated text input editing and platform text input activation
- repeated theme color derivation or DPI scaling helpers

Keep behavior in the app when it is domain-specific:

- navigation route selection
- practice/session state machines
- settings persistence
- app-specific copy and locale keys
- app-specific icons and assets
- business rules for confirmation or cancellation

## Downstream Submodule Workflow

Applications should vendor File UI Toolkit as a git submodule. Do not edit
`vendor/flint` directly for permanent changes.

Use this flow:

1. Edit the real File UI Toolkit repository.
2. Commit and push File UI Toolkit.
3. In the downstream app, update the `vendor/flint` submodule to the pushed commit.
4. Commit the downstream app's submodule pointer and any app code needed to use the new
   File UI Toolkit API.

This keeps File UI Toolkit reusable and prevents project-local vendor edits from diverging.

## Documentation Rules

Keep `docs/API.md` as the public API reference. Keep this guide as the operational
guidance for agents and maintainers. Documentation in File UI Toolkit should describe current
APIs, current behavior, and the expected downstream workflow.

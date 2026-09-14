# 03 Input And Release Lifecycles

## Goal

Make pointer, release, keyboard, and ownership lifecycles deterministic and
transpilable by routing widget decisions through `.kry`.

## Current State

Several widgets already return decision records from `.kry`, including popup,
overlay, modal, link, tab bar, scroll, dropdown, profile header, swipe,
drag/drop, input, slider, table, and reorder lifecycle gates.

Remaining C scans still show direct sampling and side-effect application. Input
sampling is okay. Widget decisions are not.

## Tasks

1. Audit every direct `ConsumeRelease()` call in `src/ui`.
2. For each callsite, decide whether `.kry` already provides a consume flag.
3. If C is still deciding whether to consume, add a `.kry` decision record.
4. Audit `InputPointerInteractionFor(...)` use:
   - keep shared generic helper only where it is truly generic host support
   - prefer widget-specific `.kry` input decisions for real widgets
5. Audit direct keyboard mapping:
   - Escape close
   - Enter commit
   - Space activation
   - arrows/home/end navigation
   - delete/backspace actions
6. Keep pointer owner storage in C only where it is real retained host state.
7. Document every remaining host-owned lifecycle in the canonical surface doc.

## Priority Callsite Groups

- `src/ui/reorder.c`: finish release/commit/cancel decision.
- `src/ui/ui_tk.c`: retained-tree helper decisions and legacy immediate
  wrappers.
- `src/ui/ui.c`: old immediate text/input/window paths.
- `src/ui/dropdown.c`, `src/ui/button.c`, `src/ui/modal.c`,
  `src/ui/profile_header.c`: confirm remaining consume calls only apply `.kry`
  flags.

## Proof

```sh
rg -n 'ConsumeRelease\(|InputPointerInteractionFor\(|IsMouseButtonReleased\(|IsKeyPressed\(' src/ui --glob '!build/**'
make generate-runtime
make fast-test
sh tests/public_api_names_test.sh
```

The scan does not need to be empty. Each remaining hit must be classified.

## Done When

- C samples input and applies side effects.
- `.kry` decides activation, release consumption, commit, cancel, open/close,
  navigation intent, and ownership transitions.
- The classification is documented and tested.

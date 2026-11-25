# Enforce `main` for Kryon UI programs (landed)

Owner direction: a Kryon UI program must not be a special Frame-entry file
that only a host can start. Every UI program is an ordinary Ziran program
whose `main` runs `App.Run(window, Frame)` from `kryon/app`; `Frame` stays an
ordinary function the app passes to its own run call. The owner superseded
the original phased plan and ordered the whole Frame-entry mechanism removed
at once, and that removal has landed on master. No phased work remains.

## Landed contract

```zi
App :: #import "kryon/app";

main :: () -> s32 {
    window := App.Window.{ .title = "App", .width = 960, .height = 600 }
    return App.Run(window, Frame)
}

Frame :: (frame: App.Frame) { /* widgets; WindowQuit() from "Window" exits */ }
```

- `main` is the only program entry. A project entry without `main ::` fails
  the build with a migration message pointing at this contract.
- The Frame program export is gone entirely: no compat alias, no second
  entry mode, no `--bind frame_entry:ProjectFrame=...`, no
  `KRYON_FRAME_ENTRY` escape, no capture-only Frame exemption.
- `#program_export` itself stays a Ziran language feature for plain native
  C-ABI exports (window, notification and Cairo modules export functions,
  not programs). Removing it from the language is a Ziran-side decision,
  out of scope here.
- Quitting is a window operation (`WindowQuit` in `Window`), not a Frame
  return value; a Frame that draws nothing cannot fail the program.

## What landed

- `kryon/app` dispatches each target to its frame loop: `frames_desktop`,
  `frames_web`, `frames_dom`, `frames_zx`, `frames_terminal`,
  `frames_pixmap`, `frames_android`; the old `desktop_run`, `canvas_run`,
  `dom_run`, `terminal_run`, `pixmap_run`, `zx_run` and `frame_entry`
  host modules are deleted.
- `mk/ziran-project.mk` bundles the program's own `main` for portable ZX
  builds and wraps plain C mains with a generated `kryon_program_host`.
- `Window` (`src/ui/window.zi`) supplies the window controls programs need
  (close interception, hide/show, quit), so tests and apps run as programs.
- Templates, examples, docs and the whole test suite use the `main` +
  `App.Run` shape; `docs/CHEATSHEET.md`, `docs/APPS.md`,
  `docs/PROJECTS.md`, `docs/BACKENDS.md` and `docs/FRAME_REPLAY.md` state
  the rule.
- ziran-vscode lost its `KRYON_PROGRAM_NOTICE` special path, so Kryon UI
  programs build and run like any other Ziran program with `main`.

Live consumers migrate in their own repositories by bumping their Kryon pin;
ARCHIVE repositories stay frozen.

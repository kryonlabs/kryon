# Enforce `main` for Kryon UI programs

Owner direction: a Kryon UI program should not be a special Frame-entry file
that only a host can start. Every UI program is an ordinary Ziran program:
it declares `main`, and the #program_export host-binding machinery
disappears. Expected to reduce the codebase.

## Why (the bug that surfaced this)

Opening the Wasm target for a Frame-entry UI file (voyki `src/app.zi`) shows
`app.zi has no main; it's a module. Run a program that imports it.` —
ziran-vscode `targets.cjs` `mainLine()` (line ~330) matches only
`^main\s*::\s*\(`; `buildWasm` (line ~633) fails the target when hasMain is
false; `buildArgs` (line ~365) always passes `--entry <module>:main` for
wasm. The message is factually right for the compiler (wasm needs an entry)
but wrong for the user: a Frame-entry UI file is a program in Kryon's model,
and `run.cjs` already routes `kryon/app` programs to the Kryon preview notice
(`KRYON_PROGRAM_NOTICE`, run.cjs line 15). The real fix is upstream: remove
the Frame-entry program model so UI programs have `main` and every target
builds like any other program.

## Entry contract (phase 1)

Chosen contract (the one `docs/APPS.md` already blesses):

```zi
App :: #import "kryon/app";

main :: () -> s32 {
    window := App.Window.{ .title = "App", .width = 960, .height = 600 }
    return App.Run(window, Draw)
}

Draw :: (frame: App.Frame) { /* widgets; App.Quit() exits the loop */ }
```

Alternatives rejected:

- `main :: (argc: s32, argv: **u8) -> s32` — no Kryon host needs argv today;
  the compiler already adapts a parameterless entry. Rejected: extra surface.
- Compiler-synthesized `main` from `Frame` — keeps the special case but moves
  it into the compiler, which must never know Kryon. Rejected.
- Keep Frame and only add the wasm entry — leaves two program models and the
  confusing editor story. Rejected.

Backends and phases:

| Backend | Program path today | Phase |
|---|---|---|
| desktop c99 + portable zx | yes (`App.Run` -> `frames_desktop`, `zx_window`) | enforce now |
| web canvas | yes (`page_window`/`frames_web`) but fixtures use frame-mode host capabilities (`RequestWindowClose`) | phase 2 |
| terminal, dom, pixmap(shot), android | no app-side run helper; hosts own the loop | phase 2/3 |

Owner direction: the Frame program export is removed entirely — no compat
alias, no second entry mode, no `#program_export` UI program path. The only
migration aid is the phase 1 build error pointing at `main`.
`#program_export` itself stays a Ziran language feature for plain native C
exports (window, notification and Cairo modules export functions, not
programs); removing it from the language would be a Ziran-side change
outside this plan.

## Phase 1 (this change, kryonlabs/kryon)

- `mk/ziran-project.mk`: a desktop/portable project entry without `main ::`
  fails the build with a migration message, except capture builds
  (`kryon preview`/`run --png`, `PROJECT_CAPTURE=1`) and the deprecated
  `KRYON_FRAME_ENTRY=1` escape hatch for fixtures that still need frame-mode
  host capabilities.
- `src/project/cli_linux.zi`: capture builds pass `PROJECT_CAPTURE=1`
  (replaces the `PROJECT_PROGRAM=` override).
- `templates/{app,web,pages,tui}`: scaffolds gain `main` + `App.Run`; Frame
  bodies stay where captures or the terminal profile still read them.
- Tests that build desktop profiles from Frame-only fixtures pass
  `KRYON_FRAME_ENTRY=1` until phase 2 ports them.
- Docs: `docs/APPS.md` and `docs/PROJECTS.md` state the rule and the ladder.

## Phase 2 (capability ports)

1. Program-mode close interception and input-driven quit for desktop
   (`App.Quit`, window close request) so `tests/window_desktop_test.py` and
   `tests/desktop_project_test.py` port off frame mode.
2. Canvas program capabilities (page close/route probes) so
   `canvas_project_test.py` and `page_route_project_test.py` port.
3. `App.RunTerminal`, `App.RunDom`, shot capture through the program loop;
   extend enforcement to web/terminal/dom/pixmap; drop `KRYON_FRAME_ENTRY`.
4. Android glue follows the terminal/dom shape.

## Phase 3 (the deletions — the codebase reduction)

Once no consumer builds a Frame-entry program:

- delete `src/backend/frame_entry.zi` (13 lines) and the `--bind
  ...frame_entry:ProjectFrame=...` machinery (mk lines ~146-149, 444);
- delete the hosts' `main`+`Frame` bodies (desktop_run 45, canvas_run 81,
  dom_run 48, terminal_run 245, pixmap_run 37, zx_run 171, android_run ~400
  lines shrink to thin loop modules behind `App.Run*`);
- the end state has one program model: every Kryon UI file declares `main`.
  A Frame entry then fails the build with the phase 1 migration error
  (pointing at `main` and docs/APPS.md) — no compat alias, no
  `KRYON_FRAME_ENTRY` escape, no capture-only exemption;
- capture rework: plain `kryon run shot` (no `--png`) only ever reaches the
  frame-entry path, so it disappears with Frame — either render a program's
  `Draw` once through a generated host or drop the bare form; `--png`
  capture (`PROJECT_CAPTURE=1`) already runs program entries and stays;
- ziran-vscode: no special casing needed; the wasm/native targets unify on
  the hasMain path (`targets.cjs`), `isKryonProgram` keeps only the
  `kryon/app` import check (preview.cjs lines 70-76, run.cjs line 196); the
  never-added wasm special case (~20 lines) is avoided.

## Consumers (Frame-entry UI files across ~/Projects: 171)

- 130 kryonlabs/kryon (examples are preview fixtures on the capture path;
  about 10 test fixtures and 4 templates are the migration surface)
- 8 waozixyz/games (voyki, katalis, arcade, choano, wurm)
- 6 waozixyz/apps (werk, photos, relay)
- 4 harness/harmony (client/security/src/app.zi and plugins)
- 23 others (benchmarks, taskmgr, explorer, browser, packages, archives)

Voyki migrates in phase 1. Others migrate per repository by bumping their
Kryon pin; pinned consumers (harmony pins c728654) are unaffected until they
choose to move.

## Net delta

Phase 1 is a small net addition (roughly +60 lines: error block, capture
flag, template mains, docs) because the deletions are phase 3. The end state
removes 1,000+ lines of frame-entry host/bind code across the two repos and
deletes the editor special case before it is written.

## Tests

Per migrated area: `make test-focus TEST=tests/<test>` (watch, desktop
project, native assets, canvas, page route, window), `make` module check,
`make push-check` before pushing; voyki: `ziran lock`,
`kryon build --profile desktop`, `kryon run shot`, then commit and push.

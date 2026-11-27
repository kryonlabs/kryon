# Kryon repository rules

Kryon is a reusable UI library written in Ziran. The Ziran language, compiler,
`.zir`, `.zx` and portable runtime live in the separate Ziran repository
(`~/Projects/ziranlang/ziran`); applications import Kryon as an ordinary
package, and the compiler never recognizes Kryon widget names.

## Layout

| Path | Contents |
| --- | --- |
| `src/ui/<widget>/` | Widget packages: `Props`, `Draw`, behavior, `*_laws.zi` proofs |
| `src/ui/*.zi` + `src/ui/modules.txt` | Shared core (tree, session, style, input, paint, theme); the list compiles into `libkryon.a` |
| `src/backend/*.zi` | Hosts and platform integration: desktop, terminal, Canvas, Android, components |
| `src/project/*.zi` | The `kryon` project tool |
| `tests/`, `examples/`, `templates/` | Behavior tests, sample programs, `kryon new` scaffolds |
| `docs/` | Reference documentation, indexed below |

## Rules

- Native widget implementation, platform integration, application logic and
  internal behavior checks remain Ziran (`.zi`). Application composition and
  maintained screen examples use TSX (`.tsx`) through the generic Ziran frontend.
  Do not add another Ziran application composition API or a compatibility lane.
  No new handwritten C, C++ or Go; `build/` output is not source.
- Use current Ziran syntax and APIs directly. No compatibility wrappers,
  aliases, fallback runtimes, or removed KIR, `.kry` or `.krb` paths; a missing
  general capability is implemented in Ziran first, never as a Kryon compiler
  special case.
- One widget concept has one public implementation (`Text.Draw(Text.Props)`,
  `Image.Draw(Image.Props)`); no parallel wrappers or raw texture draw APIs.
  Nothing is imported by default: a program carries only what it imports,
  and optional parts load at import or first use through small slot modules
  (`frame_driver`, `image_lookup`, `text_editing`, `scroll_sampling`), never
  runtime flags.
- Hosts observe input, own windows and present pixels; focus, hit testing,
  layout, semantics, styling and paint decisions stay in `src/ui/`. App
  screens, assets, copy and state stay in the application repository.

## Commands

```sh
make                        # check modules, generate C/Go, build libkryon.a
make test                   # source, saved-.zir and .zx behavior tests, headless
make test-focus TEST=name   # one test script; skips the full rebuild
make desktop-project-test   # graphical capture and input on private Xvfb
make laws                   # prove every src/ui/**/*_laws.zi
make sanitize-test          # ASan/UBSan behavior tests
make docs-check             # docs/FEATURE_MATRIX.html is current
make push-check             # minutes; links the hosts; run before pushing
```

Never run tests or binaries that can send X/Wayland requests against the live
desktop. Scrub inherited display/auth/session variables and use a private
Xvfb/Xephyr display, as the test targets above do.

## Documentation index

| Question | File |
| --- | --- |
| Writing an app: one-page cheat sheet (start here) | docs/CHEATSHEET.md |
| Widget APIs, drawing, packages | docs/API.md |
| Architecture; Ziran/Kryon/app boundaries | docs/ARCHITECTURE.md, docs/BOUNDARIES.md |
| `kryon` CLI, manifests, profiles, ZX apps | docs/PROJECTS.md |
| Hosts, renderers, Android, web canvas, image codecs | docs/BACKENDS.md, docs/canvas.md |
| Supported coverage; widget verification policy | docs/FEATURE_MATRIX.md, docs/WIDGET_CONFORMANCE.md |
| Input components; overlays; navigation | docs/INPUT_COMPONENTS.md, docs/OVERLAYS.md, docs/navigation.md |
| Portable ZX components; accessibility | docs/COMPONENTS.md, docs/ACCESSIBILITY.md |
| Theming, zoom, cursor, clipboard, focus, IME | docs/THEMING.md, docs/ZOOM.md, docs/CURSOR.md, docs/CLIPBOARD.md, docs/window-focus.md, docs/COMPOSITION_INPUT.md |
| Frame capture/replay; raster workers; benchmarks | docs/FRAME_REPLAY.md, docs/THREADING.md, docs/PERFORMANCE.md |
| Examples and gallery | examples/README.md |
| Open work | plans/open-work.md |

## Shared checkout

- Work on `master` in this checkout. Never create or use clones, worktrees or
  temporary Kryon/Ziran checkouts; change dependencies upstream first.
- The git index is shared and often stale, so `git status` lists hundreds of
  false edits. Check `git diff HEAD -- <file>` before touching a file, skip
  files another lane is editing, and stage only your own paths
  (`git add <paths>`, then `git commit -- <paths>` when others staged files);
  never `git add -A` and never a private `GIT_INDEX_FILE`.
- Commit as Waozi <waozi@proton.me> with no AI attribution. Keep ordinary
  changes in `Unreleased`; release only on explicit request.
- Put one-off experiment builds in `build/scratch/<name>/`; `make clean-scratch` removes idle output.

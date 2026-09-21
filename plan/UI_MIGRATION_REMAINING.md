# UI migration: remaining work and evidence

Updated 2026-09-20 from the Kryon and Inbe working trees. This is an open-work
inventory, not a completion claim. The current `.kry` moves are committed on
Kryon `master` at `8f1a8608` and Inbe `master` at `64c1b6b`; the type moves in
`runtime/*_props.kry` pass `generated-runtime-parity-test fast-test`, and Inbe's
`make native` plus contract gates pass at that pointer. Older passing captures,
builds, and benchmarks establish evidence only for the revisions they used. See
[native language completion](NATIVE_LANGUAGE_COMPLETION.md) for compiler,
callable, and KSS milestones, [button review](button/REVIEW.md) for the exact
button matrix, and [Inbe's migration plan](../../inbe/docs/kry-migration.md)
for app-specific ownership.

## 1. Close the source boundary

| Area | Current state | Remaining action and completion evidence |
|---|---|---|
| Retained UI tree and controls | The maintained tree, widgets, button, image layout, text selection/layout, frame, routes, theme state, and window placement policy are authored in `src/ui/*.kry`; earlier handwritten C owners were removed. | Generate and build the current tree, run focused behavior and C/Go parity checks, then review the generated calls for duplicate C policy. The current working tree has no such verification. |
| Native window input | `src/ui/ui_window.c` still owns focus restoration and click/drag event arbitration alongside OS window effects. | Move shared event decisions to `.kry`; leave only event collection, native handles, and presentation in C. Check mouse press/release, drag, focus, keyboard and multiwindow behavior on a private display. The attempted `WindowInputState` slice was reverted and is not implemented. |
| Remaining `src/ui/*.c` | `ui_image_cache.c`, `ui_paint.c`, `ui_surface_cache.c`, `ui_text.c`, `ui_text_backend.c`, and `ui_window.c` remain. | Audit each retained branch against the ownership table in `NATIVE_LANGUAGE_COMPLETION.md`. In particular, check image clip conversion, cache degradation parity, text font-size fallback/metrics, and window event ownership. Move any reusable UI choice to `.kry`; record the narrow renderer/platform effect left in C. |
| Core host C | `src/core/app_storage.c`, `automation.c`, `embedded_assets.c`, `kryon_abi.c`, `kryon_mem.c`, and `locale.c` remain. | Confirm these contain only storage, automation transport, assets, ABI/memory, and locale loading/lifetime effects. Locale parsing, fallback, matching, and English control labels have new `.kry` owners; verify those generated boundaries and remove duplicate decisions in C. |
| Handwritten public UI contracts | Headers such as `include/ui_tree.h`, `ui_core.h`, `ui_window.h`, `ui_text.h`, `ui_style_sheet.h`, `theme.h`, `app_shell.h`, `app_runtime.h`, `kryon_frame.h`, and `locale.h` still declare UI types/functions, including ones implemented in `.kry`. Generated `*.generated.h` files are a separate category. | Inventory every handwritten UI declaration and its consumers, provide the needed `.kry` type/function definitions and generated public ABI, then delete the handwritten UI compatibility headers where their consumers can use generated contracts. Keep only true host-facing declarations, with a documented reason. Compile public headers and maintained downstream consumers after each removal. |
| KSS and themes | KSS parser, cascade, and much style policy have `.kry` owners, but `ApplyCurrentTheme`, palette getter contracts, bridges, and downstream callers remain in the native-language plan's Milestone 5. | Migrate live callers and any duplicated appearance decisions; remove obsolete declarations and scanner allowances only after the generated ABI and applications work. Check each built-in pack, no-style mode, light/dark, and Lightfield glow/flat, plus constrained renderer degradation. |
| Tests and capture ownership | New app framework, frame pacing, image, text, locale policy, native window, style board, and appearance capture fixtures are authored in `tests/*.kry`; old C fixtures for these were removed. Other UI-adjacent tests still use C, including `tests/control_appearance_perf_test.c` and `tests/locale_test.c`. | Audit remaining C tests one by one: move UI decisions/assertions to `.kry`, retaining only narrow platform fixture code where needed. Verify each migrated fixture is actually in the build/test graph and run it at the delivered revision. |

## 2. Verify the visible Button contract

The gallery and board source is available, but the new style board PNGs have
not been produced or reviewed. The earlier 24-page gallery was accepted for
its then-current revision. Its Classic benchmark predates the accepted flat
Classic restyle, and its accepted capture predates the later Info/swatch paint
refactor. Neither is a current pixel or performance baseline.

| Evidence needed | Acceptance condition |
|---|---|
| Complete visual grid | Capture every tone, emphasis, and declared state for Material, Classic, Lightfield flat, Lightfield glow, and no style, each in light/dark where supported. Review readable icon/text contrast, Info sizing, image content, swatch, shape, pressed/hover/focus/disabled/loading, and label accuracy. Approve any baseline only after seeing the current PNGs. |
| Interaction | Run the authored `.kry` pointer/keyboard trace on a private display. Add variant-specific checks where the common Button trace does not cover behavior. Verify focus and capture through the real widget path. |
| Policy and parity | After each law is approved, run the proposed Bend Button policies and the 512-row state precedence comparison against generated C; run generated C/Go appearance parity. A Bend model pass alone does not prove pixels or delivered events. |
| Performance | Rebenchmark each style/mode, emphasis, and state after the final Classic and paint changes. Report whole-frame and Button-call median/p95, warmup and sample count, machine/backend, and before/after values for each proposed optimization. Do not promise a universal speed target from the old llvmpipe baseline. |
| Regression | Turn only visually accepted, deterministic captures into pixel baselines; freeze or exclude animation time explicitly. Run the comparison after changes to Button, KSS, image, text, or renderer paths. |

The detailed candidate laws and manual check names are in
[button/REVIEW.md](button/REVIEW.md). No test or law in this section is
authorized merely by its appearance in this document; request the user's
approval for each one before execution.

For approved Bend work, first read `bend guide`, keep approved rules in
`LAWS.bend`, and run `bend PROOF.bend` before committing. Record which law,
production function, generated target, and input domain each proof covers;
keep visual and performance evidence separate from pure policy laws.

## 3. Integration and delivery order

1. Finish the native window and header ownership decisions upstream, and
   reconcile the remaining C files. Keep app-specific behavior in the app.
2. Generate/build the current Kryon sources and run focused tests, laws, style
   captures, and benchmarks only with the user's per-run approval. Record exact
   commands, exit status, revision, artifact paths, and visual review result.
3. Verify native C and Go generated parity plus the relevant platform/render
   matrix. GUI tests must scrub inherited `DISPLAY` and `WAYLAND_DISPLAY` and
   use private Xvfb/Xephyr. Never run an input/window test on the live desktop.
4. Commit the verified Kryon source on upstream master. Update only clean
   downstream `vendor/kryon` pointers, then verify Inbe against that exact
   revision. Do not edit a vendor tree.
5. Close the Inbe-specific rows in its migration plan: generated UI API,
   desktop/Android/Plan 9/web build and behavior evidence, screenshots, and
   background notification indicator policy. Record unavailable toolchains as
   unverified, not passed.

Completion requires source ownership, generated ABI, visible behavior, and
platform evidence at the same delivered revisions. This plan is intentionally
open while approvals and verification are pending.

# Backend parity proof and paused web target

`make generate-runtime` still generates C/Go policy modules, and focused policy
tests cover the active generated decisions. The old JavaScript/web runtime path
is paused: `k2js`, `web/kryon-runtime.js`, generated JS snapshot tests, and
C/Go/JS fixture parity remain in the repository only as experimental reference
material.

Remaining:

- Keep replacing handwritten policy in active native/Go host runtimes with
  generated decisions where that reduces duplication.
- Do not continue promoting `scroll_content` or `composed_popup` through the
  handwritten JavaScript widget runtime. Widget-by-widget JS runtime parity is
  no longer the plan.
- Future web transpilation should be redesigned as `.kry -> HTML/DOM + KSS/CSS
  + small JS`, using browser-native structure, style, focus, controls, and
  accessibility where possible. Track that work in `docs/WEB_JS_ROADMAP.md` and
  the DOM plan instead of this parity checklist.
- Add real lowering for every supported expression form in active targets.
  Unsupported lowering must fail visibly instead of emitting placeholder runtime
  calls.
- Report coverage per active backend and fixture; distinguish generating,
  executing, comparing state, and checking rendered output.

Use `make go-runtime-test`, not the old cross-module root Go command. See
`README.md` for the current validation sequence and completion requirements.

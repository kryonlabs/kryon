# Web JavaScript Roadmap

Kryon's JavaScript/web target is paused. It is no longer a supported release
status target, no longer part of the default tool bundle, and no longer part of
the default test or preflight gates.

The old path lowered `.kry` into JavaScript that drove a handwritten Kryon web
runtime. That made the browser behave like another immediate-mode widget host,
which was useful for experiments but slow to finish: every widget needed custom
JavaScript behavior for focus, routing, scrolling, popups, text editing, tables,
drag ownership, and accessibility.

Future web work should use a different target shape:

- `.kry` remains the application and component authoring language.
- Web UI structure should lower to native HTML/DOM nodes.
- KSS should lower to CSS or CSS-like browser styles.
- JavaScript should hold app state, event glue, routing, and behavior that HTML
  and CSS cannot express directly.
- Widget semantics should prefer browser-native controls and accessibility
  behavior instead of reimplementing every widget in `web/kryon-runtime.js`.

The existing `cmd/k2js`, `web/`, and DOM planning files remain in the repository
as experimental reference material. They should not be advertised as current
support until a future `.kry -> HTML + CSS + JS` target has clear scope, tests,
and release gates.

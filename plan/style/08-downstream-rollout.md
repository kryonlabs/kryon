# Downstream styling: remaining work

- Audit maintained Kryon examples and templates for explicit style attachment,
  bundled assets, and direct app-owned widget decoration. Migrate remaining
  callers and compile their generated output; do not rely on stale counts.
- Audit other apps using Kryon, including Kapsule and the remaining
  `vendor/kryon` consumers. Track each app's missing API migration, packaging,
  style selection/persistence, and native/web/mobile verification.
- Finish Inbe's migration from its theme catalog/getters to KSS theme overlays
  when the language supports them. `src/app/app_style.kry` currently maps the
  resolved app palette into color tokens; this is still a theme-migration target.
- Run Inbe Android and Plan 9 builds and platform checks for bundled style
  assets and appearance behavior. Linux verification does not cover them.
- Complete downstream preview/hot-reload validation where the host supports it.

For each remaining app/platform, record the upstream revision, unresolved
caller/asset paths, build result, and interactive verification. Generic Kapsule
UI may migrate here; its terminal application behavior remains in Kapsule.

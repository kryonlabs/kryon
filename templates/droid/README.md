# Android project templates

`scripts/android_scaffold.py` copies these into a Kryon application when
`kryon build --profile android` first runs. `__NAME__`, `__CMAKE_NAME__`
(the name without spaces), `__PACKAGE__`,
`__PACKAGE_PATH__`, `__ACTIVITY__`, `__ACTIVITY_CLASS__` and
`__ACTIVITY_SLASH__` are replaced with the project name and the
`[tool.kryon.android]` settings.

The generated files belong to the application. The scaffold upgrades known
stock Gradle, CMake and Activity files and preserves customized files.

Android builds use Python 3.11 or newer. Ziran resolves package and source
dependencies from `ziran.toml` and `ziran.lock`. The checked import graph
determines which package-owned native recipes are linked. The Image widget
uses Android's image decoder by default. Import `kryon/android_avif` to
decode AVIF on older Android versions with dav1d, or `kryon/android_webp`
to use libwebp's first-frame decoder. The AVIF decoder includes libaom only
when the app also uses AVIF encoding or the libaom engine.

A package declares its native recipe under `[tool.android.native]` in its
own `ziran.toml`. The `modules` table maps native binding modules that emit
C code to aliases in that package's `[dependencies]`. Kryon supplies each
selected recipe with `ZIRAN_ANDROID_TARGET` and the locked source paths in
`ZIRAN_SOURCE_<alias>`.

The selected profile's `assets` directory is copied into the APK's assets;
a file setting ships that file. Direct Gradle builds can set `-Papp.assets=...`.
Relative paths start at the application root, beside `droid/`. Each build
refreshes this asset snapshot, including removed files.

Android uses system text and emoji by default. Set `bundled_fonts = true`
under `[tool.kryon.android]` to preserve the bundled text renderer.
Set `frame_mode = "events"` to sleep between updates and pace active frames
with the display. Apps that change outside input, animation and lifecycle
events can import `kryon/android_frames` and call `AndroidRequestFrame()`
from a worker or schedule the next timed frame with
`AndroidRequestFrame(delay_ms)`. Timed requests coalesce at the earliest
deadline, and ordinary input and invalidation preserve that deadline.
Existing apps default to `frame_mode = "adaptive"`, which checks for changes
even when the app is idle.

Layout:

- `droid/` — the Gradle project (settings, app module, manifest, activity,
  native CMake glue that builds the whole Zi graph through `zi2c`).
- `src/main.zi` — the Android entry; delegates to Kryon's `AndroidRunMain`.
- `src/android_glue.zi` — `JNI_OnLoad`, registering the canonical natives.

The Google Play edition (`-Papp.edition=gplay`) applies
`templates/android-gplay.gradle` from the Kryon checkout, which adds Play
Billing and `droid/app/src/gplay`. The scaffold copies neither, so the
normal and F-Droid editions name no Google library;
`scripts/check-google-free.py` (or the `google-free` action) checks both
  the Gradle files and the APKs.

To offer "Continue as @name" between apps, an application's `build.gradle`
applies `templates/android-account-handoff.gradle` from the Kryon checkout and
its `JNI_OnLoad` calls Entity's `AccountHandoffJniOnLoad(vm)`. The allowlist
lives in Entity's `src/account_handoff.zi`. It uses no Google library.

This README is not copied into applications.

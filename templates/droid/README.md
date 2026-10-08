# Android project templates

`scripts/android_scaffold.py` copies these into a Kryon application when
`kryon build --profile android` first runs. `__NAME__`, `__PACKAGE__`,
`__PACKAGE_PATH__`, `__ACTIVITY__`, `__ACTIVITY_CLASS__` and
`__ACTIVITY_SLASH__` are replaced with the project name and the
`[tool.kryon.android]` settings.

The generated files belong to the application. The scaffold never overwrites
an existing file, so customize the activity, manifest and Gradle build
freely; Kryon's part stays in the pinned package.

Layout:

- `droid/` — the Gradle project (settings, app module, manifest, activity,
  native CMake glue that builds the whole Zi graph through `zi2c`).
- `src/main.zi` — the Android entry; delegates to Kryon's `AndroidRunMain`.
- `src/android_glue.zi` — `JNI_OnLoad`, registering the canonical natives.

This README is not copied into applications.

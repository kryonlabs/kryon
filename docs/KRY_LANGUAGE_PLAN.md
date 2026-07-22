# Kry Language Plan

Goal: Kry should be C-close app code for Kryon. It should reduce UI and
controller boilerplate while keeping direct C names, direct C headers, and
readable C output.

Kry is not a VM language and not a separate runtime. `kc` emits `.c` and `.h`
files into the app build directory, and the generated files are compiled by the
normal C toolchain.

## Style

Kry should take inspiration from Plan 9, Go, and raylib:

- small files and short names
- explicit exported function names
- simple structs and plain data flow
- early returns
- compile-time modules only, with no runtime package system
- direct calls into C
- generated C that is easy to inspect

## Shape

Kry code should look close to C, with cleaner declarations and control flow:

```kry
cimport "app.h"
cimport "breath_engine.h"

type Timer {
    elapsed: float
    running: bool
}

pub fn whm_update(app: InbeApp*, dt: float) -> int {
    if !app->session.running {
        return 0
    }

    r := breath_step(&app->breath, dt)

    switch r.phase {
    case BreathIn:
        app->label = "Inhale"
    case BreathHold:
        app->label = "Hold"
    default:
        app->label = "Rest"
    }

    return 1
}
```

The generated C should remain direct:

```c
#include "app.h"
#include "breath_engine.h"

typedef struct Timer Timer;
struct Timer {
    float elapsed;
    bool running;
};

int
whm_update(InbeApp *app, float dt)
{
    if(!app->session.running) {
        return 0;
    }

    __auto_type r = breath_step(&app->breath, dt);

    switch(r.phase) {
    case BreathIn:
        app->label = "Inhale";
        break;
    case BreathHold:
        app->label = "Hold";
        break;
    default:
        app->label = "Rest";
        break;
    }

    return 1;
}
```

## Interop Rules

- `cimport "header.h"` includes a C header directly.
- `mod settings_ui` gives a file a flat C symbol prefix.
- `mod settings.ui` gives a file the C symbol prefix `settings_ui`.
- `use "settings_ui"` includes `settings_ui.h` and allows direct qualified
  calls such as `settings_ui.toggle_row_height(...)`.
- `use "src/screens/settings/settings_ui"` includes
  `src/screens/settings/settings_ui.h`. If that file declares
  `mod settings.ui`, calls use the module prefix from the source:
  `settings_ui.toggle_row_height(...)` emits
  `settings_ui_toggle_row_height(...)`.
- `ui := use "settings_ui"` binds a short compile-time module handle, so
  `ui.toggle_row_height(...)` still emits `settings_ui_toggle_row_height(...)`.
- Public Kry functions inside a module emit prefixed C names, such as
  `settings_ui_toggle_row_height`.
- `pub export fn c_name(...)` keeps the exact C symbol even inside a module.
  Use it only for C ABI entry points that app C calls directly.
- `use` is the Kry-to-Kry dependency mechanism. The old header-only `import`
  form is removed so modules do not drift back into manual C wiring.
- C pointer spelling remains accepted: `InbeApp*`.
- C field access remains accepted: `app->field` and `value.field`.
- C constants, enums, structs, and functions are used directly after `cimport`.
- Raw C remains available through `c` and `raw`, mainly for preprocessor branches
  and platform edges.

## Syntax To Add

Add these before attempting broad app migration:

- `cimport "header.h"` for explicit C interop.
- `mod name`, `mod dir.name`, and `use "path/to/file"` for scoped Kry-to-Kry
  calls that still emit plain C symbols.
- `type Name { field: Type }` emitting `typedef struct Name Name; struct Name`.
- `const Name: Type = value` and inferred `const Name = value`.
- `switch`, `case`, and `default` with C-style fallthrough disabled by default;
  generated C should add `break` unless Kry says otherwise.
- simple expression parsing for better diagnostics and later formatting.
- module-level `var` for file-local state.
- focused compiler tests that compare important generated C lines.

Do not add a runtime package system. Kry modules are compile-time scope and C
symbol-prefix rules only.

## Inbe Migration Boundary

Move to Kry:

- screen layout and draw functions
- navigation/sidebar/bottom-nav behavior
- settings and profile UI
- practice session UI
- habits UI
- form state, status banners, modal orchestration, and per-screen page flow

Keep in C until Kry is stronger:

- app startup and platform entry points
- Android, tray, wakelock, share, timer, and web shims
- storage, database, import/export, sync, and crypto/account plumbing
- third-party sources
- core practice engines when C is still clearer and better tested

Move timing/session engines later only after Kry has structs, switch, module
state, and tests good enough that the generated C is boring to review.

## First Inbe Targets

1. Settings status helpers: move `settings_screen.c` state and draw helper after
   module-level `var` works.
2. Settings subsections: move device/data/about controller glue after `switch`
   and `type` work.
3. Habits edit/session screens: convert one paired `.c`/`.kry` screen at a
   time and delete the C file only after behavior matches.
4. Practice session screens: keep engines in C first, move the visual/session
   controller code to Kry.

Each conversion should leave app C smaller without committing edits inside
vendored Kryon copies.

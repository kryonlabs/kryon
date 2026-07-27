# Kry Language

Kry is C-close app code for Kryon. `kc` emits readable `.c` and `.h` files
that are compiled by the normal C toolchain; there is no VM, runtime package
system, or hidden ownership model.

## Style

- Small files, clear names, and plain data flow.
- Explicit module prefixes for generated C symbols.
- Direct C-compatible pointers, field access, labels, and cleanup `goto`.
- Compile-time modules only.
- Raw C escape hatches only for platform or third-party edges.

## Declarations

```kry
#module "practice.session"
#output "session_impl"
#import "app.h"
#import <stddef.h>
#import "private_dep.h" #private
#pragma "GCC diagnostic push"

SessionTimer :: struct {
    elapsed: float
    running: bool
}

SessionMode :: enum {
    SESSION_IDLE,
    SESSION_ACTIVE,
}

#enum {
    SESSION_FLAG_DIRTY = 1,
}

#enum #private {
    SESSION_LOCAL_FLAG = 1,
}

TickCallback :: int (*)(float) #type
LocalSize :: unsigned long #type #private

session_count :: int #global #export
local_counter :: int = 7 #global

platform_ping :: (value: int, tag: const char*) -> int #extern
platform_log :: (level: int, text: const char*, ...) -> void #extern
win_entry :: (hwnd: void*) -> int #extern #storage "__declspec(dllimport)" #abi "__stdcall"
weak_hook :: () -> void #extern #attr "__attribute__((weak))"

session_update :: (app: InbeApp*, dt: float) -> int {
    if !app->session.running {
        return 0
    }

    result := breath_step(&app->breath, dt)
    if result.done {
        goto done
    }

done:
    return 1
}

session_entry :: (app: InbeApp*) #export {
    session_update(app, 0.0f)
}
```

## Rules

- `#module "name"` sets the compile-time module name and generated C symbol
  prefix. Dots become underscores.
- `#output "basename"` changes only the generated file basename.
- `#import "path.h"` emits a public include in the generated header.
- `#import <system.h>` and `#import "path.h" #private` emit source-only
  includes.
- `alias :: #import "module/path"` imports another Kry module and binds a
  compile-time alias for qualified calls.
- `name :: (...) -> Ret { ... }` defines a function. Inside a module, the
  generated C symbol is module-prefixed by default.
- `#export` keeps the exact C ABI name and emits the declaration publicly.
- `#private` keeps a function or type alias source-local.
- `#global` creates a file-level variable; add `#export` for a public `extern`.
- `#extern` declares an external C function without defining it.
- External declarations accept precise interop tags when the platform ABI needs
  them: `#storage "..."` before the return type, `#abi "..."` before the
  function name, and `#attr "..."` after the parameter list.
- `...` is allowed as the final parameter of an external declaration.
- `#intrinsic "web"` emits a Kryon-owned platform wrapper for supported web
  operations.
- `#pragma "text"` and `#error "message"` emit guarded C preprocessor
  directives for compiler and platform integration.

Kry statements are deliberately minimal — declarations, assignments, calls,
and control flow. There are no widget keywords and no bespoke sugar verbs:

- `name := expr` for inferred locals.
- `name: Type = expr` or `name: Type` for typed locals.
- `name = expr` assignments (including compound `+=`, `%=`, `&=`, ...).
- plain function calls (any call used as a statement is a draw call and is
  automatically wrapped with source-inspection metadata for click-to-source).
- `if`, `else if`, `else`, `while`, `for`, `switch`, `case`, `default`,
  labels, `goto`, `break`, `continue`, `guard`, `defer`, and `return`.
- `c line` for raw C glue and `unused expr` to silence unused-value warnings.

`defer STMT` schedules `STMT` to run when the enclosing block exits, replacing
the cleanup-`goto` idiom:

```kry
load_asset :: (path: const char*) -> Asset* {
    f := fopen(path, "rb")
    defer fclose(f)
    if f == nil {
        return nil        # fclose runs here
    }
    ...
    return parse(f)       # fclose runs here too
}                         # and here, on fall-through
```

A deferred statement fires on scope exit however the block is left: by
falling off the end, by `return`, or by `break`/`continue` that leaves the
block. Multiple defers in one block run in reverse (last-registered-first)
order, and defers in inner scopes run before those in outer scopes. A defer
only fires if it was declared before the exit that triggers it.

`defer` is a compile-time transform: the statement is spliced into the
generated C at every exit point of its scope, with no runtime cost. `goto`
that jumps out of a deferred scope does not run the defer — use explicit
cleanup labels if you mix the two.

Widgets are ordinary library functions (WidgetText, WidgetRect, WidgetLine,
WidgetBackground, WidgetButton) declared in `ui_widgets.h`; the compiler
treats them exactly like any other call. Input handling is plain control
flow: `if (UIKeyPressed(KEY_A)) { ... }`, `if (UIKeyDown(KEY_A)) { ... }`.
Event-style blocks (`button ... {`, `on key ... {`, `event ... {`) and
arithmetic/declaration sugar (`advance x by N`, `clamp_min`, `c_rect`,
`texture`, `set_theme`) were removed in favor of their direct equivalents.

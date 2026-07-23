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

Kry statements use C-like expressions with cleaner declaration syntax:

- `name := expr` for inferred locals.
- `name: Type = expr` or `name: Type` for typed locals.
- plain function calls, assignments, `if`, `while`, `for`, `switch`, `case`,
  `default`, labels, `goto`, `break`, `continue`, and `return`.
- `native expr` and `c line` remain explicit escape hatches for low-level glue.

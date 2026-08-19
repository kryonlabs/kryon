# Kry Language

Kry is C-close app code for Kryon. Source always lowers into KIR, a debuggable
intermediate representation with source spans, resolved modules, state fields,
functions, expressions, widget calls, and host/capability imports. Backends then
consume KIR:

```text
.kry -> KIR -> C
.kry -> KIR -> KRB
.kry -> KIR -> Go
.kir -> C
.kir -> KRB
```

`k2c` emits readable `.c` and `.h` files for the normal C toolchain. `k2b`
emits a portable `.krb` cartridge for renderers that implement the Kryon runtime
contract. `k2g` emits Go source that drives a Go kryon runtime (the cgo
surface binding — the same layer app uses), so Go-native apps can be written
in .kry; it translates the declarative subset (state, app metadata, frames,
widget calls), enums (untyped Go constants with C counter semantics),
switch/case/default, C-style `for` headers, and `guard` (as a plain `if`;
the body must return). `#extern "pkg.Fn"` declarations lower to a generated
`<Guard>Host` interface plus `Set<Guard>Host`: every extern call in a frame
becomes `host.Method(...)` on the embedding Go program, with numeric argument
conversions so int/long/float widening always compiles. Unsupported
imperative constructs (goto, raw C lines) still surface as TODO comments.
`k2ir` exists for tooling, debugging, tests,
and Krait inspection.
Native C remains the direct path for platform, storage, and performance-sensitive
code; portable cartridges call those services through explicit capabilities or
host imports.

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
- `NAME :: #run INTEGER_EXPR` evaluates a small integer compile-time
  expression during Kry parsing and binds the result as a compile-time
  constant for later `#if`, `#assert`, and `#run` expressions.
- `#assert CONDITION, "message"` emits a compile-time check in generated C.
  Kry compile-time constants such as `WEB :: #defined(PLATFORM_WEB)` expand in
  the condition before lowering. Unguarded constant-false assertions fail before
  backend lowering.

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

KIR keeps the original statement text for backend compatibility and also stores
a conservative structured expression tree for simple identifiers, literals,
calls, and binary expressions. Backends can adopt the tree incrementally without
changing the source pipeline or losing C-close escape hatches.

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

Widgets are ordinary library functions (UIText, UIRect, Line,
Background, UIButton) declared by Kryon; the compiler
treats them exactly like any other call. Input handling is plain control
flow: `if (IsKeyPressed(KEY_A)) { ... }`, `if (IsKeyDown(KEY_A)) { ... }`.
Event-style blocks (`button ... {`, `on key ... {`, `event ... {`) and
arithmetic/declaration sugar (`advance x by N`, `clamp_min`, `c_rect`,
`texture`, `set_theme`) were removed in favor of their direct equivalents.

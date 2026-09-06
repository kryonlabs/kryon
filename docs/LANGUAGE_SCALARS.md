# Portable scalar programming

Checked scalar functions share one KIR emitter for C, C++, native Go, and
JavaScript. This is the tested portable core of Kry, not a complete systems
language. Use `--strict` to reject source outside that core.

```kry
step :: (value: i32) -> i32 {
    return value + 1
}

third :: () -> u64 {
    maximum: u64 = 18446744073709551615
    return maximum / 3
}
```

`step(2147483647)` returns `-2147483648` on all four targets. `third()` returns
`6148914691236517205`; JavaScript represents this result as `6148914691236517205n`.

## Types and operations

| Kry type | C/C++ representation | Go representation | JavaScript representation |
|---|---|---|---|
| `i8`, `i16`, `i32`, `i64` | corresponding `intN_t` | corresponding `intN` | Number up to 32 bits; BigInt for 64 bits |
| `u8`, `u16`, `u32`, `u64` | corresponding `uintN_t` | corresponding `uintN` | Number up to 32 bits; BigInt for 64 bits |
| `f32` | `float` | `float32` | Number rounded with `Math.fround` |
| `f64` | `double` | `float64` | Number |
| `bool` | `bool` | `bool` | Boolean |

The familiar spellings `int`, `unsigned int`, `float`, `double`, and standard
fixed-width C typedefs are recognized. Prefer explicit widths in portable code.
Pointer-sized integers and C layout-dependent types are outside this core.

- Integer addition, subtraction, multiplication, negation, and narrowing wrap
  modulo the destination width. Signed results use two's-complement values.
- Division truncates toward zero. Remainder has the dividend's sign. Signed
  minimum divided by `-1` returns signed minimum; its remainder is zero.
- Signed right shift extends the sign bit. Left shift wraps to the operand width.
  Negative shift counts and counts at least the width trap.
- Division or remainder by zero traps: C/C++ abort, Go panics, JS throws RangeError.
- Literal values must fit their contextual type. Use an explicit cast when
  narrowing is intended. Integer-to-integer casts retain the low destination bits.
- Float-to-integer casts require a value in the destination range before
  truncation: `[-2^(w-1), 2^(w-1))` for signed and `[0, 2^w)` for unsigned.
  NaN, infinity, and values outside that interval trap. Accepted values truncate
  toward zero.
- Numeric-to-boolean casts test against zero. Boolean-to-numeric casts yield 0 or 1.
  Conditions and logical operators require booleans; there is no implicit truthiness.
- JS i64/u64 callers should pass BigInt. Safe integral Numbers are accepted;
  unsafe or fractional Numbers are rejected. Integer parameters normalize to
  their declared width, and boolean parameters require actual Booleans.
- f32 expression results and compound updates round to f32. Full IEEE exception,
  literal-range, and conversion-rounding conformance is still unfinished.

## Evaluation and cleanup

Operands and call arguments evaluate from left to right, with each value captured
before evaluating the next expression. `x + x++`, with `x = 3`, produces 6.
`&&`, `||`, and `condition ? yes : no` evaluate only the selected operands.
Compound assignment captures the destination's previous value before its right
side runs.

`defer` runs when its lexical block exits, in reverse registration order. This
includes `return`, `break`, and `continue`. A return expression is evaluated and
saved before cleanup runs. The shared KIR pass expands cleanup once, before any
target emitter; there are no separate target defer implementations.
See the [language specification](KRY_LANGUAGE_SPEC.md) for cleanup restrictions.
Arithmetic traps do not promise cleanup or foreign-stack unwinding.

## Build and verify

Save the example as `numbers.kry`, then run from the repository root:

```sh
make k2c k2cpp k2go k2js
build/linux-x86_64/bin/k2c --strict --no-main -o /tmp/numbers-c numbers.kry
build/linux-x86_64/bin/k2cpp --strict --no-main -o /tmp/numbers-cpp numbers.kry
build/linux-x86_64/bin/k2go --strict --no-main -o /tmp/numbers-go numbers.kry
build/linux-x86_64/bin/k2js --strict --no-main -o /tmp/numbers-js numbers.kry
make language-test
```

Adjust `linux-x86_64` for your host build directory. C/C++ scalar translation units
need standard headers but no UI inspection header. Pure generated Go omits its
UI runtime import. JS output still includes its module/frame runtime scaffold;
fully freestanding JS modules remain work to do.

`language-test` compiles and runs the same fixture in all four target languages.
It covers cleanup, integer overflow, 64-bit exactness, signed division and shifts,
casts, f32 rounding, evaluation order, short-circuiting, and arithmetic traps.

## Current limits

The shared emitter handles scalar parameters, locals, returns, assignments,
function calls, nested blocks, if/else, while, break, continue, and unused values.
Strict mode rejects bodies outside this set instead of using text lowering.
Non-strict app code still uses target-specific lowering for unmigrated constructs.
Complex module initializers do not yet share the scalar expression emitter.

Arrays/slices, ownership and allocators, unsafe pointers, checked C layouts and
ABI exports, atomics, generic specialization, and compile-time functions still
need implementation. Track those gaps in [implementation status](LANGUAGE_IMPLEMENTATION.md).

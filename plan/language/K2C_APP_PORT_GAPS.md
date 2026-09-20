# k2c app-port gaps

Found while porting t9 to `.kry` (Linux, `--no-main --root src` builds).
Each one silently produced wrong C; app code works around them today.

1. Module-level fixed-array initializers are dropped. `table ::
   [N]T #global = {...}` emits `T table[N] = {0}` and discards the
   initializer text. Workaround: keep tables as function locals.
   A diagnostic (or correct emission) is required; silent zeroing
   segfaults at runtime (t9 terminfo lookup read NULL names).

2. Very long `if` conditions are truncated silently. A 140-clause
   disjunction produced a generated `.c` that ends mid-function
   ("expected declaration or statement at end of input"). The
   frontend should either accept the condition or fail with a
   located error. Workaround: table-driven loops.

3. A local named `state` mangles multi-line compound-literal
   initializers: `state: P = (P){` + element lines emits
   `P state = (P){;` — a stray `;` after the brace. The `state`
   block keyword leaks into local lowering. Single-line `{0}`
   initializers survive.

4. Non-exported functions get a module-name prefix on their
   definition, but references to the same function used as a
   VALUE (callback argument) keep the bare name, so the call
   site no longer matches the definition. Workaround: `#export`
   any helper passed as a function pointer.

5. Generated headers include imported headers by quoted basename.
   A module whose own basename equals an imported header's
   basename (engine/text.kry importing "terminal_text.h" is fine,
   but text.kry importing "text.h" is not) self-includes through
   the generated-tree include path. Workaround: module basenames
   must differ from every imported header basename. A collision
   check in k2c would prevent this.

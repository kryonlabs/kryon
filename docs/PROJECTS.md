# Ziran projects

Install the `kryon` command from a Kryon checkout next to a Ziran checkout:

```sh
make install-user
```

This puts a launcher at `~/.local/bin/kryon`. The launcher builds the command
from the current Kryon and Ziran source before it runs. Add `~/.local/bin` to
`PATH` if needed. Project builds also read the local Kryon UI and terminal host
sources, so editing either checkout updates the next build. No network fetch is
performed.

Put a `kryon.toml` at the root of a Ziran app:

```toml
[profiles.tui]
backend = "terminal"
```

The directory name becomes the app name. The entry defaults to `src/app.zi`,
the paths default to `../kryon` and `../ziran`, and codegen defaults to `c99`.
A sole profile becomes the default; with several profiles, set
`default_profile` under `[project]`. You may override `name`, `entry`, and
`default_profile` under `[project]`, `kryon` and `ziran` under `[paths]`, and
`codegen` under each profile. Paths are relative to the app directory.

The entry imports Kryon UI modules
and exports `Frame(session: Session, viewport: Rectangle) -> s32`. The selected host owns `main`,
frame boundaries, platform effects, and presentation. App code can stay
independent of its backend. A `TextProps` value needs only its `text` field for
content at the origin; key, measured size, and wrap use widget defaults.

From the app directory, use `kryon run`, `kryon build`, or `kryon check`.
`kryon run --profile tui` selects a named profile; otherwise the command uses
`default_profile` or the sole profile. A `Makefile` may simply forward `run`,
`build`, and `check` to these commands.

The current project route supports `terminal` with C99 code generation. More
backend routes can be added to the shared `mk/ziran-project.mk` without
changing the app's `Frame`. The terminal host currently renders a single
80 by 24 cell frame and writes it to standard output.
Project output is `build/<app-name>`. The build saves checked Ziran modules in
`build/generated/ir/*.zir` before producing C99 source by module from those
saved modules in `build/generated/c/`. The linker removes unreachable
declarations from the selected host entry; reachable runtime branches remain.
The `.zir` files are
binary.
Use `../ziran/build/bin/ziran inspect build/generated/ir/app.zir` to read a
saved module, or add `--hex` for its raw bytes.

The manifest parser accepts the tables and quoted string keys shown above,
plus additional `[profiles.NAME]` tables. Names use letters, digits, `_`, or
`-`; paths also permit `.` and `/`. Comments beginning with `#` are allowed.
String escapes and other TOML value types are not supported yet.

# Canvas2D status

The current Ziran Kryon package has no Canvas2D host. Its supported project
backends are `terminal`, `desktop`, `libdraw`, and `raylib`, selected through
`[tool.kryon.profiles.NAME]` in an application's `ziran.toml`. See
[Backends](BACKENDS.md) and [Ziran projects](PROJECTS.md).

The former C Canvas2D parity table and its `KRYON_BACKEND` build commands
described a removed runtime. They do not establish Canvas2D support for the
Ziran library. A future web host must implement platform effects in Ziran and
run the same widget and input behavior tests as the maintained hosts.

# {{name}}

A [Kryon](https://github.com/kryonlabs/kryon) application written in Ziran.

```sh
kryon run            # desktop window (SDL2 + Cairo)
kryon run tui        # the same interface in a terminal
kryon run web        # Canvas2D browser page (Emscripten)
kryon profiles       # list profiles; ziran.toml defines them
kryon install        # ~/.local/bin/{{name}} and a menu entry
```

`ziran run`, `ziran build`, and `ziran install` do the same, because
`ziran.toml` sets `tool = "kryon"`. `src/app.zi` holds the interface: its
`Frame` runs every frame and declares every widget from the app's state.

# {{name}}

A [Kryon](https://git.waozi.xyz/kryonlabs/kryon) application written in Ziran.

```sh
kryon run            # desktop window (SDL2 + Cairo)
kryon run shot --png build/shot.png --width 390 --height 844 --theme dark
kryon run web        # Canvas2D browser page (Emscripten)
kryon profiles       # list profiles; ziran.toml defines them
kryon install        # ~/.local/bin/{{name}} and a menu entry
```

`kryon run shot --png` draws the app offscreen at any size, light or dark,
and writes the widget tree beside the PNG: the quickest way to see a change.

`src/app.zi` holds the interface: its `Frame` runs every frame and declares
every widget from the app's state. Layout containers place their children
(`Layout.Draw` stacks a column, `Layout.Row` a row, `Layout.Stack` gives one
slot to a composite widget) and theme tokens size them, so no code computes
a position. `src/tasks.zi` holds the state and imports no Kryon module, so
tests can call it directly.

`kryon build portable` exports `build/{{name}}-portable.zx`. Open it with
`ziran run {{name}}-portable.zx` using the shared Zx player; no app source is
needed. `ziran run`, `ziran build`, and `ziran install` do the same as the
kryon commands, because `ziran.toml` sets `tool = "kryon"`. `kryon new`
already wrote `ziran.lock`, so the first build needs no `ziran lock`.

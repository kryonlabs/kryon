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
Each container's `End` is a `defer` right after it opens, so it runs when the
block exits even on an early return. `frame.first` marks a session's first
frame: the draft's bytes are wired and the first tasks are started there. The
new-task field keeps its text in a `TextField.State` over fixed bytes;
`TextField.Value` reads it and `TextField.Clear` empties it.

`src/look.zi` keeps Kryon's default theme in light mode and opts into the
Material preset from the pinned CSS package (`[dependencies.css]`) in dark
mode; `style_pack_lightfield` and `style_pack_classic` are the other presets.

`kryon build portable` exports `build/{{name}}-portable.zx`. Open it with
`ziran run {{name}}-portable.zx` using the shared Zx player; no app source is
needed. `ziran run`, `ziran build`, and `ziran install` do the same as the
kryon commands, because `ziran.toml` sets `tool = "kryon"`. `kryon new`
already wrote `ziran.lock`, so the first build needs no `ziran lock`.

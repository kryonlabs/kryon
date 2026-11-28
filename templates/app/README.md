# {{name}}

A [Kryon](https://git.waozi.xyz/kryonlabs/kryon) application: TSX screens over a Ziran model.

```sh
kryon run            # desktop window (SDL2 + Cairo)
kryon run tui        # the same interface in a terminal
kryon run web        # WebGL2 browser page (Emscripten)
kryon profiles       # list profiles; ziran.toml defines them
kryon install        # ~/.local/bin/{{name}} and a menu entry
```

`kryon run desktop --png build/shot.png --width 390 --height 844 --theme dark`
draws the app offscreen at any size, light or dark, and writes the widget
tree beside the PNG: the quickest way to see a change.

`src/app.tsx` composes the interface. Its `Frame` runs every frame and
declares every widget from the app's state: JSX elements draw in order, so
the interface is just the frame's component tree. `AppBar`, `TaskBox`,
`TaskCard`, `TaskList` and `AddTask` are ordinary typed components, and
`TaskCard` nests `TaskBox` through JSX children; the shared leaves come
from the `kryon/tsx/...` modules, whose `props` are the native widget
props with the session alongside. Widgets report what the user did in
their results: the checkbox's `changed`, the button's `pressed`, the text
field's `committed`, so events wire straight from those values into the
model.

`src/tasks.zi` holds the task list, the new-task draft and the app bar's
menu options, so tests can call it without any widget. `src/look.zi` holds
the dark-mode preference and installs Kryon's default theme in light mode
and the Material preset from the pinned CSS package (`[dependencies.css]`)
in dark mode; `style_pack_lightfield` and `style_pack_classic` are the
other presets.

`kryon build portable` exports `build/{{name}}-portable.zx`. Open it with
`ziran run {{name}}-portable.zx` using the shared Zx player; no app source
is needed. `ziran run`, `ziran build`, and `ziran install` do the same as
the kryon commands, because `ziran.toml` sets `tool = "kryon"`. `kryon new`
already wrote `ziran.lock`, so the first build needs no `ziran lock`.

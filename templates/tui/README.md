# {{name}}

A [Kryon](https://github.com/kryonlabs/kryon) terminal application written
in Ziran: a task list you drive from the keyboard.

```sh
kryon run            # in this terminal: Up/Down, Space, Ctrl+C
kryon run desktop    # the same interface in a window
kryon install        # ~/.local/bin/{{name}}
```

`kryon build portable` exports `build/{{name}}-portable.zib`. Copy or download
that file and open it with `kryon run {{name}}-portable.zib` using the shared
Zib player; no app source is needed.

`src/layout.zi` sizes rows in character cells on the terminal host and in
pixels in a window, so one `Frame` serves both.

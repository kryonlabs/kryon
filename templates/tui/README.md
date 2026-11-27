# {{name}}

A [Kryon](https://github.com/kryonlabs/kryon) terminal application: a task
list you drive from the keyboard, composed in TSX over a Ziran model.

```sh
kryon run            # in this terminal: Up/Down, Space, Ctrl+C
kryon run desktop    # the same interface in a window
kryon install        # ~/.local/bin/{{name}}
```

`src/app.tsx` composes the interface. Its `Frame` runs for every key and
declares the interface from the model: JSX elements draw in order, and the
shared leaves come from the `kryon/tsx/...` modules. The list reports what
the user did in its result (`selected_index`, `scroll_offset`), so events
wire straight into the model.

`src/tasks.zi` holds the tasks, the keyboard handling and the page cursor,
so tests can use them without any widget. The layout helpers from
`kryon/PageLayout` size rows in character cells on the terminal host and
in pixels in a window, so one `Frame` serves both.

`kryon build portable` exports `build/{{name}}-portable.zx`. Copy or
download that file and open it with `ziran run {{name}}-portable.zx` using
the shared Zx player; no app source is needed.

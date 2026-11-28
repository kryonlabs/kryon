# {{name}}

A [Kryon](https://github.com/kryonlabs/kryon) application with three pages
(Home, Library, Settings) behind a tab row: TSX screens over a Ziran model.
Settings change what the library shows, so the template shows how pages
share state.

```sh
kryon run            # desktop window
kryon run tui        # the same pages in a terminal
kryon run web        # WebGL2 browser page (Emscripten)
```

`src/app.tsx` composes the interface: a `Frame` that starts the page
column, draws the title, one `Tab` component per page, and then the page's
own component. Each JSX element draws in order, and the shared leaves come
from the `kryon/tsx/...` modules. Widgets report what the user did in their
results (the tab's `pressed`, the checkbox's `changed`, the list's
`selected_index`), so events wire straight into the model.

`src/library.zi` holds the books, the settings and the page cursor, so
tests can use them without any widget. The layout helpers from
`kryon/PageLayout` size rows in pixels in a window and in character cells
in a terminal, so one `Frame` serves both hosts.

`kryon build portable` exports `build/{{name}}-portable.zx`. Copy or
download that file and open it with `ziran run {{name}}-portable.zx` using
the shared Zx player; no app source is needed.

Add a page by writing a component like `Library` and one more `Tab` line
in `Frame`.

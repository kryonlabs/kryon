# {{name}}

A [Kryon](https://github.com/kryonlabs/kryon) application with three pages
(Home, Library, Settings) behind a tab row. Settings change what the library
shows, so the template shows how pages share state.

```sh
kryon run            # desktop window
kryon run tui        # the same pages in a terminal
kryon run web        # Canvas2D browser page (Emscripten)
```

Add a page by writing a procedure like `Library` and a `Tab` for it in
`Frame`.

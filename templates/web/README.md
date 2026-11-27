# {{name}}

A [Kryon](https://github.com/kryonlabs/kryon) web page: headings, wrapped
paragraphs, links and a like button composed in TSX over a Ziran model,
drawn on a Canvas2D element.

```sh
kryon build          # build/{{name}}-web.html (needs Emscripten)
kryon run desktop    # the same page in a window
```

`src/app.tsx` composes the page. Its `Frame` runs every frame, starts the
centered page column and places one component per row: `Heading` and
`Paragraph` wrap Kryon's `Text` leaf with a semantic kind, and `WebLink`
wraps the link leaf, so the JSX elements draw in order and the shared
leaves come from the `kryon/tsx/...` modules. The button reports what the
user did in its result (`pressed`), so the like event wires straight into
the model.

`src/counter.zi` holds the like count and its text, so tests can call it
without any widget. The layout helpers from `kryon/PageLayout` size the
rows for the page column, and the text rows carry semantic kinds, so the
published page keeps its accessibility tree.

`kryon build portable` exports `build/{{name}}-portable.zx`. Copy or
download that file and open it with `ziran run {{name}}-portable.zx` using
the shared Zx player; no app source is needed. Serve `build/` with any
static file server to publish the page.

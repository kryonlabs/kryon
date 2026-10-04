# {{name}}

A web page written with [Kryon](https://github.com/kryonlabs/kryon) in
Ziran. The DOM host turns headings, paragraphs, links, and buttons into real
HTML elements.

```sh
kryon build            # build/{{name}}-web.html (needs Emscripten)
kryon run canvas       # the same page on a Canvas2D element
kryon run desktop      # the same page in a window
```

`kryon build portable` exports `build/{{name}}-portable.zib`. Copy or download
that file and open it with `kryon run {{name}}-portable.zib` using the shared
Zib player; no app source is needed.

Serve `build/` with any static file server to publish it.

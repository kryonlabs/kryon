// {{name}}: a web page written with Kryon, drawn on a WebGL2 canvas:
// headings, paragraphs, links, and buttons composed from your state in
// src/page/counter.zi.
//
//   kryon build          build/{{name}}-web.html (needs Emscripten)
//   kryon run desktop    the same page in a window
import * as App from "kryon/app";
import * as Starter from "web_starter";

export function Frame(frame: App.Frame): void {
    Starter.Frame(frame);
}

export function main(): s32 {
    return App.Run({title: "{{name}}"}, Frame);
}

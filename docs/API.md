# Kryon API

Kryon is an ordinary Ziran package. An application imports the modules it
uses; no widget names are recognized by the Ziran compiler or `.zib` loader.
The public package imports are declared in [`ziran.toml`](../ziran.toml).

| Import | Surface |
| --- | --- |
| `Kryon` | Geometry, drawing values, sessions, `Text(TextProps)`, and `Button(ButtonProps)` |
| `Widgets` | The core widget catalog, including `Image(ImageProps)`, fields, controls, layout, and tree input |
| `PlotWidget` | Optional plot widget |
| `TableView`, `TreeView` | Optional data views |
| `Kss` | Optional style parsing and installation |
| `Syntax` | Optional source coloring for TextArea spans |

Use `Image(ImageProps)` for UI images. Its asset path, bounds, fit, and alt
text describe the image at the widget boundary. Backend texture calls are
renderer primitives, not alternate widget APIs.

An application using a Kryon host exports a Ziran function with this shape:

```zi
using UI :: #import "Widgets";

#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    label: TextProps
    label.text = "Hello, Kryon"
    Text(session, label)
    return 0
}
```

The selected host owns the process entry point, calls `BeginFrame` and
`EndFrame`, supplies input to the session, and presents paint operations.
Application code owns its product state and calls widgets within `Frame`.
The complete manifest and build commands are in [PROJECTS.md](PROJECTS.md).

Individual `.zi` modules under [`src/ui`](../src/ui) can also be imported by
non-graphical Ziran programs. A program that does not import Kryon uses no
Kryon code. A portable `.zib` links only imported modules and needs a host
binding for any platform effect they call.

[`src/ui/modules.txt`](../src/ui/modules.txt) is the core build inventory.
`make test` checks source, saved `.zir`, `.zib`, and generated C/C++/Go behavior.
See [FEATURE_MATRIX.md](FEATURE_MATRIX.md) for package and host verification.

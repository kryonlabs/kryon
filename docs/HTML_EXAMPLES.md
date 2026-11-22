# Runnable example: searchable card with real CSS

This complete `.zi` program translates a common HTML pattern (`<section class="card">` + `<input type="search">` + `<button class="primary">` + status text) into native Kryon + `css/Css`:

```zi
App :: #import "kryon/app";
Button :: #import "kryon/button";
Card :: #import "kryon/card";
Text :: #import "kryon/text";
TextField :: #import "kryon/text_field";
Theme :: #import "kryon/theme";
CSS :: #import "css/Css";
#import "kryon/control_props"
#import "kryon/geometry"
#import "kryon/session"
#import "kryon/tree"

APP_CSS :: #string CSS
:root {
  --card-bg: #ffffff;
  --border: #cbd5e1;
  --accent: hwb(160deg 2% 41%);
  --focus: hwb(199deg 5% 8% / 95%);
}

@media (prefers-color-scheme: dark) {
  :root {
    --card-bg: #181d25;
    --border: #334155;
    --accent: #34d399;
    --focus: #38bdf8;
  }
}

Card.panel {
  background: var(--card-bg);
  border: 1px solid var(--border);
  border-radius: 12px;
  -kryon-elevation: rest;
}

TextField.query-box {
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 0px 12px;
  outline-color: var(--focus);
}

TextField.query-box:focus {
  border-color: var(--accent);
}

Button.primary {
  background: var(--accent);
  color: #ffffff;
  border-radius: 8px;
  min-block-size: 36px;
}

Button.primary:hover {
  opacity: 92%;
}
CSS;

sheet: CSS.CssSheet;
started: bool;
dark: bool;
query_bytes: [64]u8;
query: TextField.State;
submitted: bool;

main :: () -> s32 {
    return App.Run(App.Window.{.title = "Search Card", .width = 520, .height = 240}, Draw)
}

InstallTheme :: () {
    unused Theme.InstallDefaultTheme(dark)
    CSS.CssSheetReset(*sheet)
    if CSS.CssSheetParse(*sheet, APP_CSS, "app.css", CSS.CssNoImports) {
        env := CSS.CssEnvironmentWithNames(
            CSS.CssDefaultEnvironment(), ifx dark then "dark" else "light",
            "normal", "comfortable", "mouse", "desktop")
        unused CSS.CssSheetInstall(*sheet, env, null)
    }
}

Draw :: (frame: App.Frame) {
    session := Session.{.slot = frame.slot, .epoch = frame.epoch}
    viewport := TreeRootBounds(session)
    if !started {
        query.bytes = query_bytes[:]
        query.focused = true
        InstallTheme()
        started = true
    }

    card := Card.Draw(session, Card.Props.{
        .key = 1, .class_name = CSS.StyleClass("panel"),
        .bounds = Rectangle.{20.0, 20.0, viewport.width - 40.0, viewport.height - 40.0},
        .variant = .CardOutlined
    })
    c := card.content
    Text.Draw(session, Text.Props.{
        .key = 2, .bounds = Rectangle.{c.x, c.y, c.width, 24.0},
        .text = "Filter cluster nodes", .size = 18
    })
    edited := TextField.Bind(session, *query, TextField.Props.{
        .key = 3, .id = 3, .focus_id = 3, .class_name = CSS.StyleClass("query-box"),
        .bounds = Rectangle.{c.x, c.y + 34.0, c.width - 110.0, 38.0},
        .label = "Filter", .placeholder = "Type host or region..."
    })

    if Button.Draw(session, Button.Props.{
        .key = 4, .id = 4, .class_name = CSS.StyleClass("primary"),
        .bounds = Rectangle.{c.x + c.width - 98.0, c.y + 34.0, 98.0, 38.0},
        .label = "Apply", .tone = .ButtonToneAccent, .emphasis = .ButtonEmphasisFilled
    }) != 0 || edited.committed {
        submitted = true
    }

    if Button.Draw(session, Button.Props.{
        .key = 5, .id = 5, .bounds = Rectangle.{c.x, c.y + 84.0, 120.0, 32.0},
        .label = ifx dark then "Light mode" else "Dark mode",
        .emphasis = .ButtonEmphasisOutline
    }) != 0 {
        dark = !dark
        InstallTheme()
    }

    status := ifx submitted && query.length > 0 then TextView(query_bytes[0:query.length]) else "All nodes active"
    Text.Draw(session, Text.Props.{
        .key = 6, .bounds = Rectangle.{c.x, c.y + 128.0, c.width, 22.0},
        .text = status, .size = 14
    })
}
```

For a complete multi-view example with sidebar navigation, KPI cards, scrollable results, loading/empty/error states, responsive wide/narrow breakpoints, and a keyboard-operable settings modal, see [`examples/dashboard/`](../examples/dashboard/README.md).

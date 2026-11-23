# Kryon cheat sheet

Kryon is a Ziran UI library: Kryon calls the app's `Frame` every frame, which
declares every widget from the app's state; widgets report what the user did.
`make cheatsheet-test` builds, shoots and drives every `// file:` block below.

## Start and see

```sh
kryon new notes && cd notes   # app starter: app bar, cards, dark mode; locks itself
kryon run --watch             # the window; rebuilds and relaunches on every save
kryon run shot --png build/shot.png --width 390 --height 844 --theme dark
kryon upgrade                 # an older app: pin this kryon's Ziran, Kryon and CSS, relock
```

No display needed; `build/shot.json` lists each node's kind, role, label and bounds.

## Rules

- Declare every widget every frame; keep state at file scope (`x: s32;`) in a Kryon-free module.
  Widgets need no `.key`: one is known by its calling line and parent, and a loop's
  repeats count in order. Pass `.key = item.id` only to keep state when rows reorder.
- Containers (Layout, Grid, Flow, Modal) place children: never compute `x` or `y`;
  a child declares its size, and zero width fills a column. A Row, Column, Grid
  or Flow without a height sizes to its children when it closes with `End`. If `opened`, draw the
  children, then `End(session)`: a missing `End` fails the whole frame; a Card is such a
  container. A widget given a rectangle with an origin (`bar.trailing_bounds`) is drawn
  there, over the column.
- Size and space come from theme tokens. Act on a result in the same frame.
- Typed text lives in `TextField.State` bytes; zero `length cursor anchor scroll_x` to clear.
- A returned `string` must view file-scope bytes (as `NoteAt` does), never a local buffer.
- `ziran lock` refused "checked package metadata": `[toolchain]` must be the exact Ziran
  Kryon pins (`kryon init` sets it). No window under the driver: build with `KRYON_TEST_DRIVER=1`.

## The project

```toml
# file: ziran.toml
[package]
name = "notes"
entry = "src/app.zi"
module_roots = ["src"]
bridge_modules = ["app"]
links = ["X11"]           # private_display in the driver test uses Xlib
tool = "kryon"
[toolchain]
git = "https://git.waozi.xyz/ziranlang/ziran.git"
ref = "8133bd41ef171e03ce4557b55122eb739548eb72"
[dependencies.kryon]
git = "https://git.waozi.xyz/kryonlabs/kryon.git"
ref = "02cc10e682b83fe7ba9c2ed4638507818d3c9cc0"
[tool.kryon]
default_profile = "desktop"
entry = "src/app.zi"
[tool.kryon.profiles.desktop]
backend = "desktop"
```

```zi
// file: src/notes.zi
#import "std/text_tools"                // state and text; no Kryon import
note_bytes: [8][64]u8;
note_lengths: [8]s32;
note_count: s32;
AddNote :: (text: string) -> bool {
    trimmed := TextTrim(text)
    if trimmed.count == 0 || note_count >= 8 { return false }
    length := ifx trimmed.count > 64 then 64 else trimmed.count
    for i: 0..length - 1 { note_bytes[note_count][i] = trimmed[i] }
    note_lengths[note_count] = cast(s32)length
    note_count += 1
    return true
}
NoteAt :: (index: s32) -> string { return TextView(note_bytes[index][0:note_lengths[index]]) }
NoteCountText :: () -> string { return ifx note_count == 0 then "No notes yet" else "Some notes" }
```

```zi
// file: src/app.zi
Button :: #import "kryon/button";
Card :: #import "kryon/card";
Dropdown :: #import "kryon/dropdown";
Layout :: #import "kryon/layout";
Text :: #import "kryon/text";
TextField :: #import "kryon/text_field";
Theme :: #import "kryon/theme";
TitleBar :: #import "kryon/title_bar";
Toggle :: #import "kryon/toggle";
#import "kryon/control_props"
#import "kryon/geometry"
#import "kryon/session"
#import "kryon/tree"
#import "notes"
#import "widgets"
started: bool;
dark: bool;
showcase: bool;
draft_bytes: [64]u8;
draft: TextField.State;
no_menu: []Dropdown.Option;
#program_export
Frame :: (session: Session, viewport: Rectangle) -> s32 {
    if !started { draft.bytes = draft_bytes[:]; dark = Theme.CurrentTheme().dark; started = true }
    screen := Layout.Draw(session, Layout.Props.{.bounds = viewport})
    if !screen.opened { return 0 }
    AppBar(session)
    body := Layout.Draw(session, Layout.Props.{.gap = Layout.Gap(.ThemeMetricSpace3),
        .gap_override = true, .padding = Layout.Gap(.ThemeMetricSpace4), .padding_override = true})
    if body.opened {
        if showcase { Widgets(session) } else { NotesScreen(session) }
        unused End(session)
    }
    unused End(session)
    return 0
}
AppBar :: (session: Session) {
    height := Theme.CurrentThemeMetric(.ThemeMetricControlHeightLarge) + Theme.CurrentThemeMetric(.ThemeMetricSpace3)
    bar := TitleBar.Draw(session, TitleBar.Props.{.bounds = .{height = height}, .title = "Notes", .strong_title = true, .trailing_width = 120}, no_menu)
    switch := Toggle.Draw(session, Toggle.Props.{.bounds = bar.trailing_bounds, .label = "Dark", .value = dark})   // drawn over the bar
    if switch.changed { dark = switch.value; unused Theme.SetThemeMode(dark) }
}
NotesScreen :: (session: Session) {
    control := Theme.CurrentThemeMetric(.ThemeMetricControlHeightMedium)
    row := Layout.Row(session, Layout.Props.{.bounds = .{height = control},
        .gap = Layout.Gap(.ThemeMetricSpace2), .gap_override = true})
    if row.opened {
        add := Button.Props.{.label = "Add", .tone = .ButtonToneAccent, .emphasis = .ButtonEmphasisFilled}
        add.bounds.width = Button.ButtonMeasure(add).width
        field := TextField.Bind(session, *draft, TextField.Props.{.placeholder = "Write a note",
            .label = "Note", .bounds = .{width = row.content.width - add.bounds.width - cast(float32)row.gap}})
        if Button.Draw(session, add) == 1 || field.committed {
            if AddNote(TextView(draft_bytes[0:draft.length])) {
                draft.length = 0; draft.cursor = 0; draft.anchor = 0; draft.scroll_x = 0
            }
        }
        unused End(session)
    }
    Text.Draw(session, Text.Props.{.bounds = .{height = control}, .text = NoteCountText()})
    for i: 0..note_count - 1 {
        card := Card.Draw(session, Card.Props.{.variant = .CardOutlined})   // a padded column that grows to its content
        if card.opened { Text.Draw(session, Text.Props.{.text = NoteAt(i)}); unused End(session) }
    }
    if Button.Draw(session, Button.Props.{.label = "All widgets", .bounds = .{height = control}}) == 1 { showcase = true }
}
```

## One line per widget

`Button.Draw` returns 1 on a click; value widgets return `.changed` and the value.

```zi
// file: src/widgets.zi
Button :: #import "kryon/button";        // .label .tone .emphasis .size .disabled
Checkbox :: #import "kryon/checkbox";    // .label .checked -> .changed .checked
Chip :: #import "kryon/chip";            // .label .selectable .dismissible -> .changed .dismissed
Layout :: #import "kryon/layout";
ListItem :: #import "kryon/list_item";   // .title .subtitle .leading_name .trailing -> .clicked
Progress :: #import "kryon/progress";    // .min .max .value .label
Radio :: #import "kryon/radio";          // .label .checked -> 1 when picked
Separator :: #import "kryon/separator";  // a rule across the column
Skeleton :: #import "kryon/skeleton";    // .shape .now: a pulsing placeholder
Slider :: #import "kryon/slider";        // DiscreteSlider: .value .min .max -> .changed .value
Text :: #import "kryon/text";            // .text .size .wrap .align .strong
Theme :: #import "kryon/theme";
Toggle :: #import "kryon/toggle";        // .label .value -> .changed .value
#import "kryon/control_props"
#import "kryon/geometry"
#import "kryon/session"
#import "kryon/tree"
#import "layouts"
checked: bool;
choice: s32;
level: s32 = 3;
on: bool = true;
Widgets :: (session: Session) {
    h := Theme.CurrentThemeMetric(.ThemeMetricControlHeightMedium)
    unused Button.Draw(session, Button.Props.{.label = "Save", .bounds = .{height = h}, .tone = .ButtonToneAccent, .emphasis = .ButtonEmphasisFilled})
    box := Checkbox.Draw(session, Checkbox.Props.{.label = "Remember me", .checked = checked, .bounds = .{height = h}})
    if box.changed { checked = box.checked }
    switch := Toggle.Draw(session, Toggle.Props.{.label = "Sync", .value = on, .bounds = .{height = h}})
    if switch.changed { on = switch.value }
    if Radio.Draw(session, Radio.Props.{.label = "Weekly", .checked = choice == 1, .bounds = .{height = h}}) == 1 { choice = 1 }
    moved := Slider.DiscreteSlider(session, Slider.DiscreteSliderProps.{.semantic_label = "Level", .value = level, .min = 1, .max = 5, .bounds = .{height = h}})
    if moved.changed { level = moved.value }
    Progress.Draw(session, Progress.Props.{.max = 5, .value = level, .bounds = .{height = 8}})
    Separator.Draw(session, Separator.Props.{})
    unused Chip.Draw(session, Chip.Props.{.label = "Design", .selectable = true, .selected = on})
    unused ListItem.Draw(session, ListItem.Props.{.title = "Ada", .subtitle = "Profile", .leading_name = "Ada Lovelace", .trailing = .ListTrailingChevron})
    Skeleton.Draw(session, Skeleton.Props.{.shape = .SkeletonLine})
    Layouts(session)
}
```

Also: `kryon/text_field` (`Bind(session, *state, props)`), `kryon/card`
(`.variant` -> `opened`, then `End`), `kryon/title_bar` (`.trailing_width` ->
`.trailing_bounds`), `kryon/box` (`.fill .border .radius`), `kryon/dropdown`
(`Draw(session, props, options)`), `kryon/modal` (`opened` -> `End`,
`.dismissed`), `kryon/avatar` (`.name`), `kryon/image` (`.asset_path .fit`),
`kryon/badge`, `kryon/toast`, `kryon/scroll`; `ziran api kryon/<path>` prints an API.

## Layout recipe

```zi
// file: src/layouts.zi
Button :: #import "kryon/button";
Flow :: #import "kryon/flow";
Grid :: #import "kryon/grid";
Layout :: #import "kryon/layout";
#import "kryon/geometry"
#import "kryon/grid_props"
#import "kryon/session"
#import "kryon/tree"
Layouts :: (session: Session) {
    gap := Layout.Gap(.ThemeMetricSpace2)
    // No sizes up front: a container without a height grows to its children.
    row := Layout.Row(session, Layout.Props.{.gap = gap, .gap_override = true})
    if row.opened {                            // children keep their natural size
        unused Button.Draw(session, Button.Props.{.label = "OK"})
        unused Button.Draw(session, Button.Props.{.label = "Cancel"})
        unused End(session)
    }
    column := Layout.Draw(session, Layout.Props.{.gap = gap, .gap_override = true})
    if column.opened {                         // children stack top to bottom
        for i: 0..1 { unused Button.Draw(session, Button.Props.{.label = "Item"}) }
        unused End(session)
    }
    grid := Layout.Grid(session, GridProps.{.columns = 2, .gap = gap})
    if grid.container.opened {                 // each child takes the next cell
        for i: 0..1 { unused Button.Draw(session, Button.Props.{.label = "Cell"}) }
        Layout.GridSpan(session, grid.container, -1)   // the next child spans the row
        unused Button.Draw(session, Button.Props.{.label = "Wide"})
        unused End(session)
    }
    flow := Flow.Draw(session, Flow.Props.{.gap = gap})
    if flow.opened {                           // wraps onto new lines
        for i: 0..5 { unused Button.Draw(session, Button.Props.{.label = "Chip"}) }
        unused End(session)
    }
}
```

## Theme tokens

| Token | Use |
| --- | --- |
| `Layout.Gap(.ThemeMetricSpace1)` … `Space6` | 4, 8, 12, 16, 20, 24 px of gap or padding |
| `Theme.CurrentThemeMetric(.ThemeMetricControlHeightMedium)` | control height; also `Small`, `Large` |
| `.ThemeMetricTextXS` `TextSM` `TextBody` `TextLG` `Heading1`…`Heading4` | font sizes for `Text.Props.size` |
| `StyleTokenColor("canvas" \| "surface" \| "border" \| "text")` | palette colors (`kryon/drawing_props`) |
| `CssSheetLoad(*sheet, StylePackMaterial())`, `CssSheetInstall` | a look preset from `css/style_pack_material` (also `classic`, `lightfield`); see the starter's `src/look.zi` |
| `CssSheetParse(*sheet, text, "app.css", CssNoImports)`, `CssSheetInstall` | your own rules; a widget or container takes them by `.class_name = StyleClass("note")` |
| `Column.note { padding: 12px; gap: 4px; background: #fff; border: 1px solid #ddd; border-radius: 10px }` | containers paint their CSS box (gradients, box-shadow too) and lay out with it |
| `flex: 1`, `margin-left: auto`, `width: 50%`, `calc()`, `em`/`vw`, nesting, `@media` | sizing in rows and columns; every property: `docs/HTML_TO_KRYON.md` section 4 |
## Drive it in a test

`env KRYON_TEST_DRIVER=1 ziran build && ziran run --project --target=c tests/notes_test.zi`:

```zi
// file: tests/notes_test.zi
#import "notes"
#import "kryon/private_display"
#import "kryon/test_driver_client"
#import "std/file"
failures: s32;
Expect :: (ok: bool, what: string) { if !ok { failures += 1; print("FAIL: %\n", what) } }
main :: () -> s32 {
    Expect(NoteCountText() == "No notes yet", "starts empty")   // the model, directly
    display := OpenPrivateDisplay(800, 600, "build/xvfb.log")
    if !display.valid { print("FAIL: %\n", display.error); return 1 }
    defer ClosePrivateDisplay(*display)
    driver := DriverOpen("build/driver")
    arguments: [1]string = .["./build/notes-desktop"]
    extra: [1]string = .[DriverEnvironment(*driver)]
    app := StartOnDisplay(*display, arguments[:], extra[:], "", "build/app.log")
    if !app.started { print("FAIL: the app did not start\n"); return 1 }
    defer { unused StopOwnedProcess(*app, 5000) }
    DriverWatch(*driver, app.pid, "build/app.log")
    if FindOwnedWindow(*display, app.pid, 15000) == 0 { print("FAIL: no window\n"); return 1 }
    Expect(DriverWait(*driver, .{label = "No notes yet"}, 10000), "empty list")
    Expect(DriverPress(*driver, .{role = "text_field"}), "focus the field")
    Expect(DriverType(*driver, "Buy milk"), "type a note")
    Expect(DriverPress(*driver, .{label = "Add", role = "button"}), "press Add")
    Expect(DriverWait(*driver, .{label = "Buy milk"}, 5000), "the note shows")
    Expect(DriverPress(*driver, .{label = "All widgets", role = "button"}), "open all widgets")
    Expect(DriverWait(*driver, .{label = "Remember me"}, 5000), "the widgets screen shows")
    return ifx failures == 0 then 0 else 1
}
```

Selectors: `.{label = "...", role = "button" | "checkbox" | "text_field" | "text", index = n}`.
`DriverWait` polls until its timeout; a failure prints the labels. Never start Xvfb yourself.

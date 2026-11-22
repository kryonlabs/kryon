# Kryon cheat sheet

Kryon is a Ziran UI library: Kryon calls the app's `Frame` every frame, which
declares every widget from the app's state; widgets report what the user did.
`make cheatsheet-test` builds, shoots and drives every `// file:` block below.

## Start and see

```sh
kryon new notes && cd notes   # app starter: app bar, cards, dark mode; locks itself
kryon run                     # the window
kryon run shot --png build/shot.png --width 390 --height 844 --theme dark
```

No display needed; `build/shot.json` lists each node's kind, role, label and bounds.

## Rules

- Declare every visible widget in every frame. File-scope state lives across
  frames (end each declaration with `;`); keep it in a Kryon-free module.
- Containers (Layout, Grid, Flow, Modal) place children: never compute `x` or `y`;
  a child declares its size, and zero width fills a column. If `opened`, draw the
  children, then `End(session)`: a missing `End` fails the whole frame. Composite
  widgets (Card, TitleBar) draw into a `Layout.Stack` slot: pass `slot.bounds`.
- Size and space come from theme tokens. Act on a result in the same frame:
  `if Button.Draw(...) == 1 { count += 1 }`.
- Typed text lives in the `TextField.State` bytes; clear it by zeroing `length`,
  `cursor`, `anchor`, `scroll_x`. After a button press, press the field again to type.
- A returned `string` must view storage that outlives the call (file-scope
  bytes, as in `notes.zi`), never a local buffer.
- `ziran lock` refused "checked package metadata": `[toolchain]` must be the
  exact Ziran that Kryon pins (`kryon init` sets it). No window under the
  driver: build with `KRYON_TEST_DRIVER=1`. Never start Xvfb yourself.

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
ref = "5db3d3c39425d5dfe4bfe4a84e6f6e2e5a810ad0"
[dependencies.kryon]
git = "https://git.waozi.xyz/kryonlabs/kryon.git"
ref = "cdf23986a0434cfb01513ecca4763b0477d01a7e"
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
    slot := Layout.Stack(session, Layout.Props.{.bounds = .{height = height}})
    if !slot.opened { return }
    bar := TitleBar.Draw(session, TitleBar.Props.{.bounds = slot.bounds, .title = "Notes",
        .strong_title = true, .trailing_width = 120}, no_menu)
    switch := Toggle.Draw(session, Toggle.Props.{.bounds = bar.trailing_bounds, .label = "Dark", .value = dark})
    if switch.changed { dark = switch.value; unused Theme.SetThemeMode(dark) }
    unused End(session)
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
        slot := Layout.Stack(session, Layout.Props.{.bounds = .{height = control + 2.0 * Theme.CurrentThemeMetric(.ThemeMetricSpace3)}})
        if slot.opened {
            card := Card.Draw(session, Card.Props.{.bounds = slot.bounds, .variant = .CardOutlined})
            Text.Draw(session, Text.Props.{.bounds = card.content, .text = NoteAt(i)})
            unused End(session)
        }
    }
    if Button.Draw(session, Button.Props.{.label = "All widgets", .bounds = .{height = control}}) == 1 { showcase = true }
}
```

## One line per widget

`Button.Draw` returns 1 on a click; value widgets return `.changed` and the value.

```zi
// file: src/widgets.zi
Avatar :: #import "kryon/avatar";        // .name .size; initials when no .image
Badge :: #import "kryon/badge";          // .text .tone .dot
Button :: #import "kryon/button";        // .label .tone .emphasis .size .disabled
Checkbox :: #import "kryon/checkbox";    // .label .checked -> .changed .checked
Progress :: #import "kryon/progress";    // .min .max .value .label
Radio :: #import "kryon/radio";          // .label .checked -> 1 when picked
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
    unused Button.Draw(session, Button.Props.{.label = "Save", .bounds = .{height = h},
        .tone = .ButtonToneAccent, .emphasis = .ButtonEmphasisFilled})
    box := Checkbox.Draw(session, Checkbox.Props.{.label = "Remember me", .checked = checked, .bounds = .{height = h}})
    if box.changed { checked = box.checked }
    switch := Toggle.Draw(session, Toggle.Props.{.label = "Sync", .value = on, .bounds = .{height = h}})
    if switch.changed { on = switch.value }
    if Radio.Draw(session, Radio.Props.{.label = "Daily", .checked = choice == 0, .bounds = .{height = h}}) == 1 { choice = 0 }
    if Radio.Draw(session, Radio.Props.{.label = "Weekly", .checked = choice == 1, .bounds = .{height = h}}) == 1 { choice = 1 }
    moved := Slider.DiscreteSlider(session, Slider.DiscreteSliderProps.{.semantic_label = "Level",
        .value = level, .min = 1, .max = 5, .bounds = .{height = h}})
    if moved.changed { level = moved.value }
    Progress.Draw(session, Progress.Props.{.min = 0, .max = 5, .value = level, .label = "Level", .bounds = .{height = 8}})
    unused Badge.Draw(session, Badge.Props.{.text = "New", .bounds = .{width = 64, height = 24}})
    unused Avatar.Draw(session, Avatar.Props.{.name = "Ada Lovelace", .bounds = .{width = 40, height = 40}})
    Layouts(session)
}
```

Also: `kryon/text_field` (`Bind(session, *state, props)`), `kryon/card`
(`.variant` -> `.content`), `kryon/title_bar` (`.trailing_width` ->
`.trailing_bounds`), `kryon/box` (`.fill .border .radius`), `kryon/dropdown`
(`Draw(session, props, options)`), `kryon/modal` (`opened` -> `End`,
`.dismissed`), `kryon/toast`, `kryon/image` (`.asset_path .fit`),
`kryon/scroll`. `ziran api kryon/<path>` prints a module's API.

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
    ok := Button.Props.{.label = "OK"}
    size := Button.ButtonMeasure(ok)           // a child's natural size
    row := Layout.Row(session, Layout.Props.{.bounds = .{height = size.height}, .gap = gap, .gap_override = true})
    if row.opened {                            // row children: width set, height fills
        unused Button.Draw(session, Button.Props.{.label = "OK", .bounds = .{width = size.width}})
        unused Button.Draw(session, Button.Props.{.label = "Cancel", .bounds = .{width = size.width}})
        unused End(session)
    }
    sizes: [2]Rectangle = .[size, size]; measured := Layout.LayoutContentBounds(Rectangle.{}, .FlexColumn, sizes[:], gap)
    column := Layout.Draw(session, Layout.Props.{.bounds = .{height = measured.height}, .gap = gap, .gap_override = true})
    if column.opened {
        unused Button.Draw(session, Button.Props.{.label = "First", .bounds = .{height = size.height}})
        unused Button.Draw(session, Button.Props.{.label = "Second", .bounds = .{height = size.height}})
        unused End(session)
    }
    items: [4]GridItem = .[.{height = 40}, .{height = 40}, .{height = 40}, .{height = 40}]
    grid := Layout.Grid(session, GridProps.{.columns = 2, .gap = gap}, items[:])
    if grid.container.opened {                 // a two-column grid; spans via .column_span
        for i: 0..3 { unused Button.Draw(session, Button.Props.{.label = "Cell"}) }
        unused End(session)
    }
    flow := Flow.Draw(session, Flow.Props.{.bounds = .{height = 2.0 * size.height}, .gap = gap})
    if flow.opened {                           // wraps measured children into rows
        for i: 0..5 { unused Button.Draw(session, Button.Props.{.label = "Chip", .bounds = size}) }
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
| `.tone = .ButtonToneAccent`, `.emphasis = .ButtonEmphasisFilled` | the primary action |

## Drive it in a test

Build with `env KRYON_TEST_DRIVER=1 ziran build`, then run
`ziran run --project --target=c tests/notes_test.zi`; it starts its own display.

```zi
// file: tests/notes_test.zi
#import "notes"
#import "kryon/private_display"
#import "kryon/test_driver_client"
#import "std/file"
failures: s32;
Expect :: (ok: bool, what: string) {
    if !ok { failures += 1; print("FAIL: %\n", what) }
}
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

Selectors: `.{label = "...", role = "button" | "checkbox" | "text_field" | "text",
index = n}`. `DriverWait` polls until its timeout; a failure prints the labels.

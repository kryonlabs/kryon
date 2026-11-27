# Kryon cheat sheet

Author screens in TSX and import native Ziran models directly. The common compiler checks both;
there is no generated Ziran screen or JavaScript runtime. `Frame` composes the real retained widgets.
`make cheatsheet-test` builds, captures and drives every named source block below.

## Start and see

```sh
kryon new notes && cd notes
kryon run --watch
kryon run desktop --png build/shot.png --width 390 --height 844 --theme dark
ziran ir --project -o build/ir
```

Install the current tools with `make install-user`. Capture uses the desktop renderer on a private
display; `build/shot.json` contains node roles, labels and bounds. `ziran pkg list` lists imports.

## Rules

- Keep persistent domain state and actions in Ziran; TSX owns composition and presentation state.
  The singleton screen below owns its draft and controls. Reusable stateful components need an
  independent mounted instance; a module variable would share state between all instances.
- Compose with JSX statements: containers invoke checked scoped children while open and close them
  deterministically; a child declares size and its container places it, with theme tokens for
  spacing. To read a widget's immediate bool or record result, call the same imported component as
  a normal typed function, as in `Button({session, props: add})`; a JSX statement yields no value.
- Handle results in the current frame. Buttons return booleans; value widgets return typed changes.
  Keys must remain stable when items reorder; never use an index for reorderable data.
- Native storage crosses through checked intrinsics that keep ordinary TypeScript syntax:
  `nativeRef(draft)` borrows a mutable record, `nativeSlice(menu, s32(0), s32(0))` borrows an array
  range, and `nativeCallerLocation()` is a default parameter's caller site. There is no `any` and
  no second schema to maintain; the compiler checks the calls against the native declarations.
- Initialize app resources on `frame.first`. A text field owns editing storage. `Value` borrows it;
  the native action copies a saved note before `Clear` releases the draft contents.
- Native integers and handles keep their explicit types. `number` is binary64; `s32(value)` and
  `float32(value)` are checked narrowings, and wide ids use `u64(n)`. Borrowed text must outlive
  its use, including deferred callbacks.

## The project

```toml
# file: ziran.toml
[package]
name = "notes"
entry = "src/app.tsx"
module_roots = ["src"]
links = ["X11"] # the native private-display test uses Xlib
tool = "kryon"
[toolchain]
git = "https://git.waozi.xyz/ziranlang/ziran.git"
ref = "c1d3e887f239ce9f4da6d9233463c779fb5e48cf"
[dependencies.kryon]
git = "https://git.waozi.xyz/kryonlabs/kryon.git"
ref = "e8b42fd4cb56ce388d8c8a0ca603c206385358f7"
[dependencies.css]
git = "https://git.waozi.xyz/kryonlabs/css.git"
ref = "e091a4339eeb0018dbf175df70709683248b7461"
[tool.kryon]
default_profile = "desktop"
[tool.kryon.profiles.desktop]
backend = "desktop"
```

## The model

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
NoteCount :: () -> s32 { return note_count }
NoteCountText :: () -> string { return ifx note_count == 0 then "No notes yet" else "Some notes" }
```

## The screen

```tsx
// file: src/app.tsx
import * as App from "kryon/app";
import * as CSS from "css/Css";
import * as Theme from "kryon/theme";
import { ThemeMetricRole as Metric } from "kryon/theme";
import { Session } from "kryon/session";
import { LayoutContainer, Gap } from "kryon/layout";
import { Option } from "kryon/dropdown";
import { Props as ButtonProps, ButtonMeasure } from "kryon/button";
import { ButtonTone, ButtonEmphasis } from "kryon/control_props";
import { CardVariant } from "kryon/card";
import * as Editing from "kryon/text_field";
import { Column, ResolvedRow } from "kryon/tsx/layout";
import { Button } from "kryon/tsx/button";
import { Card } from "kryon/tsx/card";
import { Text } from "kryon/tsx/text";
import { TextField } from "kryon/tsx/text_field";
import { TitleBar } from "kryon/tsx/title_bar";
import { Toggle } from "kryon/tsx/toggle";
import { Widgets, Controls } from "widgets";
import * as Notes from "notes";
let dark: boolean = false;
let showcase: boolean = false;
let draft: Editing.State = {};
let sheet: CSS.CssSheet = {};
let overrides: CSS.CssOverrides = {};
let controls: Controls = {checked: false, on: true, choice: s32(0), level: s32(3)};
function AppBar(session: Session): void {
    const menu: [Option] = [{}];
    const height = Theme.CurrentThemeMetric(Metric.ThemeMetricControlHeightLarge) + float32(Gap(Metric.ThemeMetricSpace3));
    const bar = TitleBar({session: session, options: nativeSlice(menu, s32(0), s32(0)),
        props: {bounds: {height: height}, title: "Notes", strong_title: true, trailing_width: s32(120)}});
    const toggle = Toggle({session: session, props: {bounds: bar.trailing_bounds, label: "Dark", value: dark}});
    if (toggle.changed) { dark = toggle.value; Theme.SetThemeMode(dark); }
}
function NotesScreen(session: Session): void {
    const height = Theme.CurrentThemeMetric(Metric.ThemeMetricControlHeightMedium);
    <ResolvedRow session={session} props={{bounds: {height: height}, gap: Gap(Metric.ThemeMetricSpace2), gap_override: true}}
        children={(row: LayoutContainer): void => {
            const add: ButtonProps = {label: "Add", tone: ButtonTone.ButtonToneAccent, emphasis: ButtonEmphasis.ButtonEmphasisFilled};
            add.bounds.width = ButtonMeasure(add).width;
            const field = TextField({session: session, state: nativeRef(draft), props: {placeholder: "Write a note", label: "Note",
                bounds: {width: row.content.width - add.bounds.width - float32(row.gap)}}});
            if (Button({session: session, props: add}) || field.committed) {
                if (Notes.AddNote(Editing.Value(draft))) { Editing.Clear(nativeRef(draft)); }
            }
        }}/>;
    <Text session={session} props={{bounds: {height: height}, text: Notes.NoteCountText()}}/>;
    for (let index = s32(0); index < Notes.NoteCount(); index += s32(1)) {
        <Card session={session} props={{variant: CardVariant.CardOutlined, class_name: CSS.StyleClass("note")}}>
            <Text session={session} props={{text: Notes.NoteAt(index)}}/>
        </Card>;
    }
    if (Button({session: session, props: {label: "All widgets", bounds: {height: height}}})) { showcase = true; }
}
function Body(session: Session): void {
    if (showcase) { <Widgets session={session} state={nativeRef(controls)}/>; }
    else { NotesScreen(session); }
}
export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    if (frame.first) {
        dark = Theme.CurrentTheme().dark;
        if (CSS.CssSheetParse(nativeRef(sheet), "Card.note { border-radius: 14px; gap: 4px; }", "app.css", CSS.CssNoImports)) {
            CSS.CssSheetInstall(nativeRef(sheet), CSS.CssDefaultEnvironment(), nativeRef(overrides));
        }
    }
    <Column session={session} props={{bounds: frame.viewport}}>
        {AppBar(session)}
        <Column session={session} props={{gap: Gap(Metric.ThemeMetricSpace3), gap_override: true,
            padding: Gap(Metric.ThemeMetricSpace4), padding_override: true}}>
            {Body(session)}
        </Column>
    </Column>;
}
export function main(): s32 { return App.Run({title: "Notes"}, Frame); }
```

## Typed widget results

```tsx
// file: src/widgets.tsx
import { Session } from "kryon/session";
import { ThemeMetricRole, CurrentThemeMetric } from "kryon/theme";
import { ButtonTone, ButtonEmphasis } from "kryon/control_props";
import { Button } from "kryon/tsx/button";
import { Checkbox } from "kryon/tsx/checkbox";
import { Toggle } from "kryon/tsx/toggle";
import { Radio } from "kryon/tsx/radio";
import { DiscreteSlider } from "kryon/tsx/slider";
import { Progress } from "kryon/tsx/progress";
import { Separator } from "kryon/tsx/separator";
import { Layouts } from "layouts";
export interface Controls { checked: boolean; on: boolean; choice: s32; level: s32; }
export interface WidgetsProps { session: Session; state: NativeRef<Controls>; }
export function Widgets(widget: WidgetsProps): void {
    const session = widget.session;
    const state = widget.state;
    const height = CurrentThemeMetric(ThemeMetricRole.ThemeMetricControlHeightMedium);
    <Button session={session} props={{label: "Save", bounds: {height: height},
        tone: ButtonTone.ButtonToneAccent, emphasis: ButtonEmphasis.ButtonEmphasisFilled}}/>;
    const box = Checkbox({session: session, props: {label: "Remember me", checked: state.checked, bounds: {height: height}}});
    if (box.changed) { state.checked = box.checked; }
    const toggle = Toggle({session: session, props: {label: "Sync", value: state.on, bounds: {height: height}}});
    if (toggle.changed) { state.on = toggle.value; }
    if (Radio({session: session, props: {id: s32(1), label: "Weekly", checked: state.choice === s32(1), bounds: {height: height}}}) === s32(1)) {
        state.choice = s32(1);
    }
    const moved = DiscreteSlider({session: session, props: {semantic_label: "Level", value: state.level,
        min: s32(1), max: s32(5), bounds: {height: height}}});
    if (moved.changed) { state.level = moved.value; }
    <Progress session={session} props={{max: s32(5), value: state.level, bounds: {height: 8}}}/>;
    <Separator session={session} props={{}}/>;
    <Layouts session={session}/>;
}
```

Import optional widgets selectively. Their `props` types come from the actual native declarations;
`ziran api --project --imports-only src/app.tsx` lists the checked imports.

## Layout recipe

```tsx
// file: src/layouts.tsx
import { Session } from "kryon/session";
import { GridResult, GridSpan, Gap } from "kryon/layout";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { ResolvedGrid } from "kryon/tsx/grid";
import { Flow } from "kryon/tsx/flow";
import { Button } from "kryon/tsx/button";
export interface LayoutProps { session: Session; }
export function Layouts(layout: LayoutProps): void {
    const session = layout.session;
    const gap = Gap(ThemeMetricRole.ThemeMetricSpace2);
    <Column session={session} props={{gap: gap, gap_override: true}} children={(): void => {
        for (let index = s32(0); index < s32(2); index += s32(1)) { <Button session={session} props={{label: "Item"}}/>; }
    }}/>;
    <ResolvedGrid session={session} props={{columns: s32(2), gap: gap}} children={(grid: GridResult): void => {
        for (let index = s32(0); index < s32(2); index += s32(1)) { <Button session={session} props={{label: "Cell"}}/>; }
        GridSpan(session, grid.container, s32(-1));
        <Button session={session} props={{label: "Wide"}}/>;
    }}/>;
    <Flow session={session} props={{gap: gap}} children={(): void => {
        for (let index = s32(0); index < s32(6); index += s32(1)) { <Button session={session} props={{label: "Chip"}}/>; }
    }}/>;
}
```

Spacing tokens `ThemeMetricSpace1` through `Space6` resolve to 4–24 pixels. Text and control size
roles come from `kryon/theme`. CSS classes, presets and responsive rules use the native CSS package;
see [theming](THEMING.md) and [CSS/layout properties](HTML_TO_KRYON.md).

## Drive it in a test

`env KRYON_TEST_DRIVER=1 ziran tool kryon build desktop && ziran run --project --target=c tests/notes_test.zi`:

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

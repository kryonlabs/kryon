import * as App from "kryon/app";
import * as Native from "kryon/inline_edit";
import { Session } from "kryon/session";
import { InlineEdit } from "kryon/tsx/inline_edit";
import { EditorBounds, EditorExample, EditorPress } from "editor_example";
import { ReadEditorInput } from "editor_input";

let exampleDisabled: boolean = false;
let initialized: boolean = false;
let standaloneState: Native.InlineEditState = nativeInit<Native.InlineEditState>({});
let galleryState: Native.InlineEditState = nativeInit<Native.InlineEditState>({});

function DrawEditor(frame: App.Frame, gallery: boolean): void {
    if (!initialized) {
        standaloneState = Native.InlineEditStateFor("Launch checklist");
        galleryState = Native.InlineEditStateFor("Launch checklist");
        galleryState.editing = true;
        galleryState.focused = true;
        initialized = true;
    }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    let bounds = EditorBounds(frame.viewport);
    bounds.height = float32(42);
    const state = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const key = gallery ? u64(602) : u64(0);
    const press = EditorPress(session, bounds, s32(702), "Edit document title", exampleDisabled, false);
    const input = ReadEditorInput(session, bounds);
    let text = input.text;
    text.blur = input.outside_press;
    <InlineEdit state={state} widget={{session: session, props: {key: key, bounds: bounds,
        value: "Launch checklist", placeholder: "Document title", input: text,
        disabled: exampleDisabled, activate: press.clicked, focus_visible: true, height: bounds.height, font_size: 17}}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="InlineEdit">{DrawEditor(frame, false)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawEditor(frame, true);
}

export function main(): s32 {
    return App.Run({title: "inlineedit"}, Frame);
}

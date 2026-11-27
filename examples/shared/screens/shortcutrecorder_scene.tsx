import * as App from "kryon/app";
import { KeyEnter, KeyModifierControl } from "kryon/input_protocol";
import * as Native from "kryon/shortcut_recorder";
import { Session } from "kryon/session";
import { ShortcutRecorder } from "kryon/tsx/shortcut_recorder";
import { EditorBounds, EditorExample, EditorPress } from "editor_example";
import { ReadEditorInput } from "editor_input";

let exampleDisabled: boolean = false;
let initialized: boolean = false;
let standaloneState: Native.ShortcutRecorderState = nativeInit<Native.ShortcutRecorderState>({});
let galleryState: Native.ShortcutRecorderState = nativeInit<Native.ShortcutRecorderState>({});

function DrawEditor(frame: App.Frame, gallery: boolean): void {
    if (!initialized) {
        standaloneState = Native.ShortcutRecorderStateFor(s32(KeyEnter), s32(KeyModifierControl));
        galleryState = Native.ShortcutRecorderStateFor(s32(KeyEnter), s32(KeyModifierControl));
        galleryState.focused = true;
        initialized = true;
    }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    let bounds = EditorBounds(frame.viewport);
    bounds.height = float32(42);
    const state = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const key = gallery ? u64(605) : u64(0);
    const press = EditorPress(session, bounds, s32(705), "Record a shortcut", exampleDisabled, false);
    const input = ReadEditorInput(session, bounds);
    // The host reports standalone Shift, Control, Alt and Meta key events before a chord.
    const modifierOnly = input.key >= s32(340) && input.key <= s32(347);
    <ShortcutRecorder state={state} widget={{session: session, props: {key: key, bounds: bounds,
        placeholder: "Record a shortcut", key_event: modifierOnly ? s32(0) : input.key,
        modifiers: input.modifiers, activate: press.clicked, outside_press: input.outside_press,
        disabled: exampleDisabled, height: bounds.height, font_size: 16, width: bounds.width}}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="ShortcutRecorder">{DrawEditor(frame, false)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawEditor(frame, true);
}

export function main(): s32 {
    return App.Run({title: "shortcutrecorder"}, Frame);
}

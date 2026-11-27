import * as App from "kryon/app";
import { KeyDown, KeyUp } from "kryon/input_protocol";
import { PressResult } from "kryon/press";
import * as Native from "kryon/scrub_field";
import { Session } from "kryon/session";
import { ScrubField } from "kryon/tsx/scrub_field";
import { EditorBounds, EditorExample, EditorPress } from "editor_example";
import { ReadEditorInput } from "editor_input";

interface ScrubGesture {
    tracking: boolean;
    moving: boolean;
    previousX: float32;
}

let exampleDisabled: boolean = false;
let initialized: boolean = false;
let standaloneState: Native.ScrubFieldState = nativeInit<Native.ScrubFieldState>({});
let galleryState: Native.ScrubFieldState = nativeInit<Native.ScrubFieldState>({});
let standaloneGesture: ScrubGesture = {tracking: false, moving: false, previousX: 0};
let galleryGesture: ScrubGesture = {tracking: false, moving: false, previousX: 0};

function ScrubDelta(gesture: NativeRef<ScrubGesture>, press: PressResult): float32 {
    if (exampleDisabled || (!press.down && !press.released)) {
        gesture.tracking = false;
        gesture.moving = false;
        return float32(0);
    }
    if (!gesture.tracking) {
        gesture.tracking = press.down;
        gesture.moving = false;
        gesture.previousX = press.x;
        return float32(0);
    }
    const delta = press.x - gesture.previousX;
    if (delta < float32(-3) || delta > float32(3)) {
        gesture.moving = true;
    }
    gesture.tracking = press.down;
    if (gesture.moving) {
        gesture.previousX = press.x;
        return delta;
    }
    return float32(0);
}

function DrawEditor(frame: App.Frame, gallery: boolean): void {
    if (!initialized) {
        standaloneState = Native.ScrubFieldStateFor(float32(5.25), s32(2));
        galleryState = Native.ScrubFieldStateFor(float32(5.25), s32(2));
        initialized = true;
    }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    let bounds = EditorBounds(frame.viewport);
    bounds.height = float32(42);
    const state = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const gesture = gallery ? nativeRef(galleryGesture) : nativeRef(standaloneGesture);
    const key = gallery ? u64(603) : u64(0);
    const press = EditorPress(session, bounds, s32(703), "Adjust duration", exampleDisabled, true);
    const input = ReadEditorInput(session, bounds);
    const delta = ScrubDelta(gesture, press);
    let text = input.text;
    const activate = !state.editing && ((press.clicked && !gesture.moving) || text.commit);
    text.commit = text.commit && !activate;
    text.blur = input.outside_press;
    <ScrubField state={state} widget={{session: session, props: {key: key, bounds: bounds,
        label: "Duration", suffix: "minutes", initial_value: 5.25, minimum: 0, maximum: 90,
        step: 0.25, precision: s32(2), input: text, fine_divisor: 10, coarse_multiplier: 5,
        up: input.key === s32(KeyUp), down: input.key === s32(KeyDown), shift: text.shift, activate: activate,
        drag_active: delta !== float32(0), drag_delta: delta, disabled: exampleDisabled, height: bounds.height}}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="ScrubField">{DrawEditor(frame, false)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawEditor(frame, true);
}

export function main(): s32 {
    return App.Run({title: "scrubfield"}, Frame);
}

import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { TimePicker } from "kryon/tsx/time_picker";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneTime: s32 = s32(570);
let galleryTime: s32 = s32(570);

function DrawTime(frame: App.Frame, bounds: Rectangle, value: s32): s32 {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const result = TimePicker({session: session, props: {bounds: bounds, label: "Start time",
        value: value, now: s32(570), disabled: exampleDisabled}});
    return result.value;
}

function DrawStandaloneTime(frame: App.Frame): void {
    standaloneTime = DrawTime(frame, nativeInit<Rectangle>({height: 44}), standaloneTime);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="TimePicker">{DrawStandaloneTime(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    galleryTime = DrawTime(frame, SelectionBounds(frame.viewport, 64, 44), galleryTime);
}

export function main(): s32 {
    return App.Run({title: "timepicker"}, Frame);
}

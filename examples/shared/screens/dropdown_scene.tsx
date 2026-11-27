import * as App from "kryon/app";
import * as NativeDropdown from "kryon/dropdown";
import { Rectangle } from "kryon/geometry";
import { Session } from "kryon/session";
import { Dropdown } from "kryon/tsx/dropdown";
import { EditorExample } from "editor_example";
import { UseEscapeForEditing } from "editor_input";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneChoice: s32 = s32(1);
let galleryChoice: s32 = s32(1);

function DrawChoice(frame: App.Frame, bounds: Rectangle, choice: s32, key: u64, open: boolean): s32 {
    UseEscapeForEditing();
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const options: [NativeDropdown.Option, NativeDropdown.Option, NativeDropdown.Option] = [
        nativeInit<NativeDropdown.Option>({label: "Morning"}), nativeInit<NativeDropdown.Option>({label: "Afternoon"}), nativeInit<NativeDropdown.Option>({label: "Evening"})];
    const result = Dropdown({session: session, props: {key: key, bounds: bounds, selected_index: choice,
        disabled: exampleDisabled, open: open}, options: nativeSlice(options)});
    return result.selected_index;
}

function DrawStandaloneChoice(frame: App.Frame): void {
    standaloneChoice = DrawChoice(frame, nativeInit<Rectangle>({height: 44}), standaloneChoice, u64(0), false);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="Dropdown">{DrawStandaloneChoice(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    galleryChoice = DrawChoice(frame, SelectionBounds(frame.viewport, 64, 44), galleryChoice, u64(0), false);
}

export function GalleryOpenFrame(frame: App.Frame): void {
    galleryChoice = DrawChoice(frame, SelectionBounds(frame.viewport, 64, 44), galleryChoice, u64(47), false);
}

export function GalleryStaticOpenFrame(frame: App.Frame): void {
    galleryChoice = DrawChoice(frame, {x: 24, y: 64, width: 432, height: 44}, galleryChoice, u64(47), true);
}

export function main(): s32 {
    return App.Run({title: "dropdown"}, Frame);
}

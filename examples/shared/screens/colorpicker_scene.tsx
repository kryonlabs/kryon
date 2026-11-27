import * as App from "kryon/app";
import * as NativeColorPicker from "kryon/color_picker";
import { Session } from "kryon/session";
import { ColorPicker } from "kryon/tsx/color_picker";
import { EditorExample } from "editor_example";
import { SelectionBounds } from "selection_bounds";

let exampleDisabled: boolean = false;
let standaloneColor: [float32, float32, float32, float32] = [0.18, 0.52, 0.68, 1.0];
let galleryColor: [float32, float32, float32, float32] = [0.18, 0.52, 0.68, 1.0];

function DrawColor(frame: App.Frame, props: NativeColorPicker.Props): NativeColorPicker.ColorPickerResult {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    return ColorPicker({session: session, props: props});
}

function DrawStandaloneColor(frame: App.Frame): void {
    const result = DrawColor(frame, nativeInit<NativeColorPicker.Props>({bounds: {height: 208}, label: "Accent", channels: s32(4),
        values: standaloneColor, disabled: exampleDisabled}));
    standaloneColor = result.values;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="ColorPicker">{DrawStandaloneColor(frame)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    const bounds = SelectionBounds(frame.viewport, 64, 208);
    const result = DrawColor(frame, nativeInit<NativeColorPicker.Props>({bounds: bounds, label: "Accent", channels: s32(4),
        values: galleryColor, disabled: exampleDisabled}));
    galleryColor = result.values;
}

export function main(): s32 {
    return App.Run({title: "colorpicker"}, Frame);
}

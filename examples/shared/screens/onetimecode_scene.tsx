import * as App from "kryon/app";
import { Rectangle } from "kryon/geometry";
import * as Native from "kryon/one_time_code";
import { Session } from "kryon/session";
import { OneTimeCode } from "kryon/tsx/one_time_code";
import { EditorBounds, EditorExample, EditorPress } from "editor_example";
import { ReadEditorInput } from "editor_input";

let exampleDisabled: boolean = false;
let initialized: boolean = false;
let standaloneState: Native.OneTimeCodeState = nativeInit<Native.OneTimeCodeState>({});
let galleryState: Native.OneTimeCodeState = nativeInit<Native.OneTimeCodeState>({});

function DrawEditor(frame: App.Frame, gallery: boolean): void {
    if (!initialized) {
        standaloneState = Native.OneTimeCodeStateFor(s32(6), "482913", true);
        galleryState = Native.OneTimeCodeStateFor(s32(6), "482913", true);
        initialized = true;
    }
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const bounds = EditorBounds(frame.viewport);
    const state = gallery ? nativeRef(galleryState) : nativeRef(standaloneState);
    const key = gallery ? u64(601) : u64(0);
    let style = Native.OneTimeCodeDefaultStyle();
    const narrow = bounds.width < float32(340);
    style.slot_width = narrow ? float32(28) : float32(40);
    style.slot_height = float32(48);
    style.gap = narrow ? float32(4) : float32(7);
    style.group_gap = float32(14);
    const codeWidth = style.slot_width * float32(6) + style.gap * float32(5) + style.group_gap;
    const codeBounds: Rectangle = {x: bounds.x + (bounds.width - codeWidth) / float32(2), y: bounds.y,
        width: codeWidth, height: style.slot_height};
    const press = EditorPress(session, codeBounds, s32(701), "Enter the one-time code", exampleDisabled, false);
    const input = ReadEditorInput(session, codeBounds);
    <OneTimeCode state={state} widget={{session: session, props: {key: key, bounds: bounds,
        length: s32(6), disabled: exampleDisabled, focus: press.clicked, blur: input.outside_press,
        text: input.text.text, left: input.text.left,
        right: input.text.right, backspace: input.text.backspace, delete_forward: input.text.delete_forward,
        escape: input.text.escape, style: style}}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    <EditorExample frame={frame} title="OneTimeCode">{DrawEditor(frame, false)}</EditorExample>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawEditor(frame, true);
}

export function main(): s32 {
    return App.Run({title: "onetimecode"}, Frame);
}

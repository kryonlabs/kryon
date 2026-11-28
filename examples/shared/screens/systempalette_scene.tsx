import * as App from "kryon/app";
import { ButtonEmphasis, ButtonTone } from "kryon/control_props";
import { StyleTokenColor } from "kryon/drawing_props";
import { Session } from "kryon/session";
import { FormatInteger } from "std/number_text";
import { Vec } from "std/vec";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { TextField } from "kryon/tsx/text_field";
import { Text } from "kryon/tsx/text";
import { Toggle } from "kryon/tsx/toggle";
import * as Model from "textfield_model";

let exampleDisabled: boolean = false;
let notifications: boolean = true;
let activations: s32 = s32(0);
let digits: Vec<u8> = nativeInit<Vec<u8>>({});

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

function DrawActivationCount(session: Session): void {
    if (digits.count === s64(0) && !VecResize(digits, s64(24))) {
        return;
    }
    const label: Vec<u8> = nativeInit<Vec<u8>>({});
    if (!BuilderAppend(label, "Activations: ")) {
        return;
    }
    if (!BuilderAppend(label, FormatInteger(s64(activations), VecSlice(digits, s64(0), digits.count)))) {
        return;
    }
    <Text session={session} props={{bounds: {x: 24, y: 252, width: 432, height: 36},
        text: TextView(VecSlice(label, s64(0), label.count))}}/>;
}

function DrawControls(session: Session): void {
    if (Button({session: session, props: {key: u64(10), bounds: {x: 24, y: 64, width: 208, height: 44},
        label: "Continue", tone: ButtonTone.ButtonToneAccent, disabled: exampleDisabled}})) {
        activations += s32(1);
    }
    if (Button({session: session, props: {key: u64(11), bounds: {x: 248, y: 64, width: 208, height: 44},
        label: "Tonal action", emphasis: ButtonEmphasis.ButtonEmphasisSoft, disabled: exampleDisabled}})) {
        activations += s32(1);
    }
    <TextField session={session} state={Model.GalleryField(false)} props={{key: u64(12),
        bounds: {x: 24, y: 128, width: 432, height: 44}, disabled: exampleDisabled}}/>;
    notifications = (Toggle({session: session, props: {key: u64(13), bounds: {x: 24, y: 196, width: 432, height: 44},
        label: "Notifications", value: notifications, disabled: exampleDisabled}})).value;
    DrawActivationCount(session);
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    DrawControls(session);
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    DrawControls(session);
}

export function main(): s32 {
    return App.Run({title: "System palette"}, Frame);
}

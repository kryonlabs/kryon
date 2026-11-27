import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Spinbox } from "kryon/tsx/spinbox";
import { Text } from "kryon/tsx/text";
import { FormatInteger } from "std/number_text";

let exampleDisabled: boolean = false;
let standaloneMinutes: s32 = s32(25);
let galleryMinutes: s32 = s32(25);

function DrawMinutes(session: Session, bounds: Rectangle, minutes: s32): s32 {
    let digits: [u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8] = [u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0)];
    const valueText = FormatInteger(s64(minutes), nativeSlice(digits));
    const result = Spinbox({session: session, props: {bounds: bounds, label: "Minutes", value: minutes,
        min: s32(0), max: s32(60), value_text: valueText, disabled: exampleDisabled}});
    return result.value;
}

function DrawStandaloneMinutes(session: Session): void {
    standaloneMinutes = DrawMinutes(session, nativeInit<Rectangle>({height: 44}), standaloneMinutes);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Spinbox", size: s32(23)}}/>
        {DrawStandaloneMinutes(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    galleryMinutes = DrawMinutes(session, {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 44}, galleryMinutes);
}

export function main(): s32 {
    return App.Run({title: "spinbox"}, Frame);
}

import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { DiscreteSlider } from "kryon/tsx/slider";
import { Text } from "kryon/tsx/text";
import { FormatInteger } from "std/number_text";

let exampleDisabled: boolean = false;
let standaloneStep: s32 = s32(4);
let galleryStep: s32 = s32(4);

function DrawStep(session: Session, step: s32, captionBounds: Rectangle): s32 {
    const result = DiscreteSlider({session: session, props: {bounds: {height: 80}, label: "Step size",
        value: step, min: s32(1), max: s32(10), disabled: exampleDisabled}});
    let digits: [u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8] = [u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0)];
    <Text session={session} props={{bounds: captionBounds, text: FormatInteger(s64(result.value), nativeSlice(digits))}}/>;
    return result.value;
}

function DrawStandaloneStep(session: Session, viewport: Rectangle): void {
    standaloneStep = DrawStep(session, standaloneStep, {
        x: viewport.x + float32(28) * viewport.width / float32(480), y: viewport.y + 180,
        width: float32(420) * viewport.width / float32(480), height: 38});
}

function DrawGalleryStep(session: Session): void {
    galleryStep = DrawStep(session, galleryStep, nativeInit<Rectangle>({height: 38}));
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
        <Text session={session} props={{bounds: {height: 36}, text: "DiscreteSlider", size: s32(23)}}/>
        {DrawStandaloneStep(session, viewport)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + float32(24) * viewport.width / float32(480),
        y: viewport.y + 64, width: float32(432) * (viewport.width / float32(480))},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        {DrawGalleryStep(session)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "discreteslider"}, Frame);
}

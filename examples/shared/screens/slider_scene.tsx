import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { Slider } from "kryon/tsx/slider";
import { Progress } from "kryon/tsx/progress";

let exampleDisabled: boolean = false;
let standaloneVolume: float32 = float32(65);
let galleryVolume: float32 = float32(65);

function DrawVolume(session: Session, value: float32): float32 {
    const volume = Slider({session: session, props: {bounds: {height: 80}, label: "Volume",
        value: value, max: 100, disabled: exampleDisabled}});
    <Progress session={session} props={{bounds: {height: 16}, value: s32(volume.value), max: s32(100)}}/>;
    return volume.value;
}

function DrawStandaloneVolume(session: Session): void {
    standaloneVolume = DrawVolume(session, standaloneVolume);
}

function DrawGalleryVolume(session: Session): void {
    galleryVolume = DrawVolume(session, galleryVolume);
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
        <Text session={session} props={{bounds: {height: 36}, text: "Slider", size: s32(23)}}/>
        {DrawStandaloneVolume(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace6), gap_override: true}}>
        {DrawGalleryVolume(session)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "slider"}, Frame);
}

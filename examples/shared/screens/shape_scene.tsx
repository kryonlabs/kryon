import * as App from "kryon/app";
import { ColorFromPacked, StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Circle, Ring, Shape, Triangle } from "kryon/tsx/shape";
import { Text } from "kryon/tsx/text";

function DrawShapes(session: Session): void {
    const ink = ColorFromPacked(u32(0x1E738AFF));
    <Circle session={session} props={{center: {x: 88, y: 128}, radius: 38, fill: ink}}/>;
    <Ring session={session} props={{center: {x: 202, y: 128}, inner_radius: 27, outer_radius: 38,
        start_angle: -90, end_angle: 200, color: ink}}/>;
    <Triangle session={session} props={{first: {x: 292, y: 166}, second: {x: 330, y: 90},
        third: {x: 368, y: 166}, color: ink}}/>;
    <Shape session={session} props={{start: {x: 28, y: 220}, end: {x: 432, y: 220}, thickness: 2, color: ink}}/>;
}

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Circle Ring Triangle Line", size: s32(23)}}/>
        {DrawShapes(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    DrawShapes({slot: frame.slot, epoch: frame.epoch});
}

export function main(): s32 {
    return App.Run({title: "shape"}, Frame);
}

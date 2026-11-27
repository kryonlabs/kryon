import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { PanedView } from "kryon/tsx/paned_view";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let split: s32 = s32(210);

function DrawPanes(session: Session, bounds: Rectangle, disabled: boolean): void {
    const panes = PanedView({session: session, props: {bounds: bounds, split: split, has_split: true,
        vertical: true, disabled: disabled}});
    split = panes.split;
    <Box session={session} props={{bounds: panes.first, fill: {r: u8(230), g: u8(239), b: u8(242), a: u8(255)}, radius: 8}}/>;
    <Box session={session} props={{bounds: panes.second, fill: {r: u8(222), g: u8(237), b: u8(235), a: u8(255)}, radius: 8}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "PanedView", size: s32(23)}}/>
        {DrawPanes(session, nativeInit<Rectangle>({height: 208}), disabled)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    DrawPanes(session, {x: viewport.x + viewport.width * float32(24) / float32(480), y: viewport.y + 64,
        width: viewport.width * float32(432) / float32(480), height: 208}, exampleDisabled);
}

export function main(): s32 {
    return App.Run({title: "panedview"}, Frame);
}

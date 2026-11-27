import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { RecordGalleryClick } from "kryon/examples/gallery_clicks";
import { Rectangle } from "kryon/geometry";
import { Gap, LayoutContainer } from "kryon/layout";
import { Session } from "kryon/session";
import { TextAlign } from "kryon/text_align";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column, ResolvedStack, Stack } from "kryon/tsx/layout";
import { Press } from "kryon/tsx/press";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let clicks: s32 = s32(0);

interface PressAreaProps {
    session: Session;
    bounds: Rectangle;
    disabled: boolean;
    onClick: (clicked: boolean) => void;
}

function PressArea(area: PressAreaProps): void {
    <Box session={area.session} props={{bounds: area.bounds, fill: StyleTokenColor("surface"),
        border: StyleTokenColor("border"), radius: 6}}/>;
    <Text session={area.session} props={{bounds: area.bounds, text: "Press this area",
        align: TextAlign.TextAlignCenter}}/>;
    const result = Press({session: area.session, props: {bounds: area.bounds, label: "Press and hold",
        disabled: area.disabled}});
    area.onClick(result.clicked);
}

function RecordClick(clicked: boolean): void {
    if (clicked) {
        clicks += s32(1);
    }
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
        <Text session={session} props={{bounds: {height: 36}, text: "Press", size: s32(23)}}/>
        <ResolvedStack session={session} props={{bounds: {height: 44}}}
            children={(stack: LayoutContainer): void => {
                <PressArea session={session} bounds={stack.bounds} disabled={disabled} onClick={RecordClick}/>;
            }}/>
        <Text session={session} props={{bounds: {height: 38}, text: "Press is an invisible interaction area"}}/>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: float32(432) * (viewport.width / float32(480))},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Stack session={session} props={{bounds: {height: 164}}}>
            <PressArea session={session} bounds={{}} disabled={disabled} onClick={RecordGalleryClick}/>
        </Stack>
        <Text session={session} props={{bounds: {height: 38}, text: "Press is an invisible interaction area"}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "press"}, Frame);
}

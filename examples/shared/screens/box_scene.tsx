import * as App from "kryon/app";
import { StyleField } from "kryon/control_props";
import { ColorFromPacked, StyleTokenColor } from "kryon/drawing_props";
import { Gap, LayoutContainer } from "kryon/layout";
import { Session } from "kryon/session";
import { TextAlign } from "kryon/text_align";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column, ResolvedStack, StyledBox } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Box", size: s32(23)}}/>
        <ResolvedStack session={session} props={{bounds: {height: 164}}} children={(area: LayoutContainer): void => {
            <Box session={session} props={{bounds: area.bounds,
                fill: ColorFromPacked(u32(0xEBF4F5FF)), border: ColorFromPacked(u32(0x87A4AAFF)), radius: 12}}/>;
            <Text session={session} props={{bounds: area.bounds, text: "A surface with rounded edges",
                vertical_align: TextAlign.TextAlignCenter}}/>;
        }}/>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <StyledBox session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 164}}}
        face={{fields: u32(StyleField.StyleBackground) + u32(StyleField.StyleBorder) + u32(StyleField.StyleRadius),
            background: u32(0xEBF4F5FF), border: u32(0x87A4AAFF), radius: 12}}>
        <Text session={session} props={{bounds: {height: 38}, text: "A surface with rounded edges"}}/>
    </StyledBox>;
}

export function main(): s32 {
    return App.Run({title: "box"}, Frame);
}

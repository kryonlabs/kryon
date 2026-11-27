import * as App from "kryon/app";
import { ButtonTone } from "kryon/control_props";
import { ColorFromPacked, StyleTokenColor } from "kryon/drawing_props";
import { Gap, LayoutContainer } from "kryon/layout";
import { Session } from "kryon/session";
import { TextAlign } from "kryon/text_align";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { Column, ResolvedStack, Stack } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let clicks: s32 = s32(0);

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
    const width = float32(164) * viewport.width / float32(480);
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Stack", size: s32(23)}}/>
        <ResolvedStack session={session} props={{bounds: {height: 208}, gap: s32(12), padding: s32(8)}}
            children={(stack: LayoutContainer): void => {
                RecordClick(Button({session: session, props: {bounds: {width: width, height: 44},
                    label: "First item", disabled: disabled}}));
                RecordClick(Button({session: session, props: {bounds: {x: stack.bounds.x + 80, y: stack.bounds.y + 48,
                    width: 164, height: 44}, label: "Second item", tone: ButtonTone.ButtonToneAccent, disabled: disabled}}));
            }}/>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Stack session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 208},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace3), padding: Gap(ThemeMetricRole.ThemeMetricSpace2)}}>
        <Box session={session} props={{bounds: {}, fill: ColorFromPacked(u32(0xEBF4F5FF)),
            border: ColorFromPacked(u32(0x87A4AAFF)), radius: 12}}/>
        <Text session={session} props={{bounds: {}, text: "Layered content", align: TextAlign.TextAlignCenter,
            disabled: disabled}}/>
    </Stack>;
}

export function main(): s32 {
    return App.Run({title: "stack"}, Frame);
}

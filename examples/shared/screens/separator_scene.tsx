import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Bullet } from "kryon/separator";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { TreeCurrentParentBounds } from "kryon/tree";
import { Box } from "kryon/tsx/box";
import { Column, Row, Stack } from "kryon/tsx/layout";
import { Separator } from "kryon/tsx/separator";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Separator and Bullet", size: s32(23)}}/>
        <Separator session={session} props={{bounds: {height: 44}, label: "or continue", disabled: disabled}}/>
        <Row session={session} props={{gap: Gap(ThemeMetricRole.ThemeMetricSpace2), gap_override: true}}>
            <Stack session={session} props={{bounds: {width: 16, height: 16}}}>
                {Bullet(session, TreeCurrentParentBounds(session))}
            </Stack>
            <Text session={session} props={{bounds: {height: 16}, text: "A clear dividing line"}}/>
        </Row>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        <Separator session={session} props={{bounds: {height: 44}, label: "or continue", disabled: disabled}}/>
        <Row session={session} props={{bounds: {height: 44},
            gap: Gap(ThemeMetricRole.ThemeMetricSpace3), gap_override: true}}>
            {Bullet(session, {width: 16, height: 44})}
            <Text session={session} props={{bounds: {height: 44}, text: "A clear dividing line"}}/>
        </Row>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "separator"}, Frame);
}

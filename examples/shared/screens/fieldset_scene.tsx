import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap, LayoutContainer } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Fieldset } from "kryon/tsx/fieldset";
import { Column, ResolvedStack } from "kryon/tsx/layout";
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
        <Text session={session} props={{bounds: {height: 36}, text: "Fieldset", size: s32(23)}}/>
        <ResolvedStack session={session} props={{bounds: {height: 188}}} children={(area: LayoutContainer): void => {
            <Fieldset session={session} props={{bounds: area.bounds, title: "Preferences"}}/>;
            <Column session={session} props={{padding: Gap(ThemeMetricRole.ThemeMetricSpace6), padding_override: true}}>
                <Text session={session} props={{bounds: {height: 38}, text: "Keep related choices together"}}/>
            </Column>;
        }}/>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace3), gap_override: true}}>
        <Fieldset session={session} props={{bounds: {height: 188}, title: "Preferences"}}/>
        <Text session={session} props={{bounds: {height: 38}, text: "Keep related choices together"}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "fieldset"}, Frame);
}

import * as App from "kryon/app";
import { ButtonTone } from "kryon/control_props";
import { StyleTokenColor } from "kryon/drawing_props";
import { RecordGalleryClick } from "kryon/examples/gallery_clicks";
import { Rectangle } from "kryon/geometry";
import { Gap, LayoutContainer } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { Column, ResolvedGroup } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let clicks: s32 = s32(0);

function RecordStandaloneClick(clicked: boolean): void {
    if (clicked) {
        clicks += s32(1);
    }
}

function DrawItems(session: Session, content: Rectangle, width: float32, disabled: boolean,
    recordClick: (clicked: boolean) => void): void {
    recordClick(Button({session: session, props: {bounds: {x: content.x, y: content.y, width: width, height: 44},
        label: "First item", disabled: disabled}}));
    recordClick(Button({session: session, props: {bounds: {x: content.x, y: content.y + 60, width: width, height: 44},
        label: "Second item", tone: ButtonTone.ButtonToneAccent, disabled: disabled}}));
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
        <Text session={session} props={{bounds: {height: 36}, text: "Group", size: s32(23)}}/>
        <ResolvedGroup session={session} props={{bounds: {height: 208}, gap: s32(12), padding: s32(8)}}
            children={(group: LayoutContainer): void => { DrawItems(session, group.content, width, disabled, RecordStandaloneClick); }}/>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <ResolvedGroup session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 208},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace3), padding: Gap(ThemeMetricRole.ThemeMetricSpace2)}}
        children={(group: LayoutContainer): void => {
            DrawItems(session, group.content, group.content.width * float32(0.4), disabled, RecordGalleryClick);
        }}/>;
}

export function main(): s32 {
    return App.Run({title: "group"}, Frame);
}

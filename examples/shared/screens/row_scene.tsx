import * as App from "kryon/app";
import { ButtonTone } from "kryon/control_props";
import { StyleTokenColor } from "kryon/drawing_props";
import { RecordGalleryClick } from "kryon/examples/gallery_clicks";
import { Gap, LayoutMetricsFor } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { TreeCurrentParent, TreeSubmittedNodeAt } from "kryon/tree";
import { Box } from "kryon/tsx/box";
import { Button } from "kryon/tsx/button";
import { Column, Row } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let clicks: s32 = s32(0);

function RecordStandaloneClick(clicked: boolean): void {
    if (clicked) {
        clicks += s32(1);
    }
}

function DrawItems(session: Session, disabled: boolean, width: float32, recordClick: (clicked: boolean) => void): void {
    recordClick(Button({session: session, props: {label: "First item", disabled: disabled,
        bounds: {width: width, height: 44}}}));
    recordClick(Button({session: session, props: {label: "Second item", disabled: disabled,
        bounds: {width: width, height: 44}, tone: ButtonTone.ButtonToneAccent}}));
}

function GalleryContentWidth(session: Session): float32 {
    const row = TreeSubmittedNodeAt(session, TreeCurrentParent(session));
    return LayoutMetricsFor(row.bounds, row.layout_gap, row.layout_padding).content.width * float32(0.4);
}

function DrawGalleryItems(session: Session, disabled: boolean): void {
    DrawItems(session, disabled, GalleryContentWidth(session), RecordGalleryClick);
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    const itemWidth = float32(164) * viewport.width / float32(480);
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Row", size: s32(23)}}/>
        <Row session={session} props={{bounds: {height: 208}, gap: s32(12), padding: s32(8)}}>
            {DrawItems(session, disabled, itemWidth, RecordStandaloneClick)}
        </Row>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Row session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 208},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace3), padding: Gap(ThemeMetricRole.ThemeMetricSpace2)}}>
        {DrawGalleryItems(session, disabled)}
    </Row>;
}

export function main(): s32 {
    return App.Run({title: "row"}, Frame);
}

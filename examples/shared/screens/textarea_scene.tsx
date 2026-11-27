import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { TextArea } from "kryon/tsx/text_area";
import { Text } from "kryon/tsx/text";
import * as Model from "textarea_model";

let exampleDisabled: boolean = false;

function DrawArea(session: Session, bounds: Rectangle, gallery: boolean): void {
    const props = Model.Prepare(session, bounds, exampleDisabled, gallery);
    const result = TextArea({session: session, props: props});
    Model.ApplyResult(gallery, result);
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
        <Text session={session} props={{bounds: {height: 36}, text: "TextArea", size: s32(23)}}/>
        {DrawArea(session, nativeInit<Rectangle>({height: 188}), false)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    DrawArea(session, {x: viewport.x + float32(24) * viewport.width / float32(480), y: viewport.y + 64,
        width: float32(432) * (viewport.width / float32(480)), height: 188}, true);
}

export function main(): s32 {
    return App.Run({title: "textarea"}, Frame);
}

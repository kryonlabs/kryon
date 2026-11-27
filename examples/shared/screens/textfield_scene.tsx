import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { ControlledTextField, TextField } from "kryon/tsx/text_field";
import * as Model from "textfield_model";

let exampleDisabled: boolean = false;

function DrawControlledFields(session: Session): void {
    const input = Model.BeginControlledFields(session);
    const filled = ControlledTextField({session: session, props: Model.ControlledProps(false, input, exampleDisabled)});
    Model.ApplyControlledResult(false, filled);
    const empty = ControlledTextField({session: session, props: Model.ControlledProps(true, input, exampleDisabled)});
    Model.ApplyControlledResult(true, empty);
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
        <Text session={session} props={{bounds: {height: 36}, text: "TextField", size: s32(23)}}/>
        {DrawControlledFields(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace3), gap_override: true}}>
        <TextField session={session} state={Model.GalleryField(false)} props={{key: u64(10),
            label: "Filled field", placeholder: "Write something...", disabled: exampleDisabled}}/>
        <TextField session={session} state={Model.GalleryField(true)} props={{key: u64(11),
            label: "Empty field", placeholder: "Write something...", disabled: exampleDisabled}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "textfield"}, Frame);
}

import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { Checkbox } from "kryon/tsx/checkbox";

interface Choices {
    checked: boolean;
    reminder: boolean;
}

let exampleDisabled: boolean = false;
let standaloneChoices: Choices = {checked: true, reminder: false};
let galleryChoices: Choices = {checked: true, reminder: false};

function DrawChoices(session: Session, choices: NativeRef<Choices>): void {
    choices.checked = (Checkbox({session: session, props: {key: u64(10), bounds: {height: 44},
        label: "Remember my choice", checked: choices.checked, disabled: exampleDisabled}})).checked;
    choices.reminder = (Checkbox({session: session, props: {key: u64(11), bounds: {height: 44},
        label: "Use a gentle reminder", checked: choices.reminder, disabled: exampleDisabled}})).checked;
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
        <Text session={session} props={{bounds: {height: 36}, text: "Checkbox", size: s32(23)}}/>
        {DrawChoices(session, nativeRef(standaloneChoices))}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        {DrawChoices(session, nativeRef(galleryChoices))}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "checkbox"}, Frame);
}

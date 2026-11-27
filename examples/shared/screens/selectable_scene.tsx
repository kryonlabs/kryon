import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Selectable } from "kryon/tsx/selectable";
import { Text } from "kryon/tsx/text";

interface Choices {
    selected: boolean;
    another: boolean;
}

let exampleDisabled: boolean = false;
let standaloneChoices: Choices = {selected: true, another: false};
let galleryChoices: Choices = {selected: true, another: false};

function DrawChoices(session: Session, choices: Choices): Choices {
    const selected = Selectable({session: session, props: {key: u64(10), bounds: {height: 44},
        label: "Selected item", selected: choices.selected, disabled: exampleDisabled}});
    const another = Selectable({session: session, props: {key: u64(11), bounds: {height: 44},
        label: "Another item", selected: choices.another, disabled: exampleDisabled}});
    return {selected: selected.selected, another: another.selected};
}

function DrawStandaloneChoices(session: Session): void {
    standaloneChoices = DrawChoices(session, standaloneChoices);
}

function DrawGalleryChoices(session: Session): void {
    galleryChoices = DrawChoices(session, galleryChoices);
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
        <Text session={session} props={{bounds: {height: 36}, text: "Selectable", size: s32(23)}}/>
        {DrawStandaloneChoices(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + float32(24) * viewport.width / float32(480),
        y: viewport.y + 64, width: float32(432) * (viewport.width / float32(480))},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        {DrawGalleryChoices(session)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "selectable"}, Frame);
}

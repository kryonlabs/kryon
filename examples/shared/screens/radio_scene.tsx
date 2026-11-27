import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { Radio } from "kryon/tsx/radio";

let exampleDisabled: boolean = false;
let standaloneBalanced: boolean = true;
let galleryBalanced: boolean = true;

function DrawOptions(session: Session, balanced: boolean): boolean {
    const balancedId = s32(1);
    const compactId = s32(2);
    if (Radio({session: session, props: {key: u64(10), id: balancedId, bounds: {height: 44},
        label: "Balanced", checked: balanced, disabled: exampleDisabled}}) === balancedId) {
        balanced = true;
    }
    if (Radio({session: session, props: {key: u64(11), id: compactId, bounds: {height: 44},
        label: "Compact", checked: !balanced, disabled: exampleDisabled}}) === compactId) {
        balanced = false;
    }
    return balanced;
}

function DrawStandaloneBalanced(session: Session): void {
    standaloneBalanced = DrawOptions(session, standaloneBalanced);
}

function DrawGalleryBalanced(session: Session): void {
    galleryBalanced = DrawOptions(session, galleryBalanced);
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
        <Text session={session} props={{bounds: {height: 36}, text: "Radio", size: s32(23)}}/>
        {DrawStandaloneBalanced(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        {DrawGalleryBalanced(session)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "radio"}, Frame);
}

import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { Toggle } from "kryon/tsx/toggle";

interface Modes {
    notifications: boolean;
    quiet: boolean;
}

let exampleDisabled: boolean = false;
let standaloneModes: Modes = {notifications: true, quiet: false};
let galleryModes: Modes = {notifications: true, quiet: false};

function DrawModes(session: Session, modes: NativeRef<Modes>): void {
    modes.notifications = (Toggle({session: session, props: {key: u64(10), bounds: {height: 44},
        label: "Notifications", value: modes.notifications, disabled: exampleDisabled}})).value;
    modes.quiet = (Toggle({session: session, props: {key: u64(11), bounds: {height: 44},
        label: "Quiet mode", value: modes.quiet, disabled: exampleDisabled}})).value;
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
        <Text session={session} props={{bounds: {height: 36}, text: "Toggle", size: s32(23)}}/>
        {DrawModes(session, nativeRef(standaloneModes))}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        {DrawModes(session, nativeRef(galleryModes))}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "toggle"}, Frame);
}

import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Option } from "kryon/dropdown";
import { RecordGalleryClick } from "kryon/examples/gallery_clicks";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { TitleBar } from "kryon/tsx/title_bar";

let clicks: s32 = s32(0);

function DrawLibraryTitle(session: Session, bounds: Rectangle): boolean {
    const options: [Option] = [nativeInit<Option>({})];
    const result = TitleBar({session: session, props: {bounds: bounds, title: "Library",
        leading_label: "Back", has_leading_action: true}, options: nativeSlice(options, s32(0), s32(0))});
    return result.clicked_leading;
}

function DrawStandaloneTitle(session: Session): void {
    if (DrawLibraryTitle(session, nativeInit<Rectangle>({height: 44}))) {
        clicks += s32(1);
    }
}

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "TitleBar", size: s32(23)}}/>
        {DrawStandaloneTitle(session)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    RecordGalleryClick(DrawLibraryTitle(session, {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: float32(432) * (viewport.width / float32(480)), height: 44}));
}

export function main(): s32 {
    return App.Run({title: "titlebar"}, Frame);
}

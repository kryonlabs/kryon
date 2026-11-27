import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { ScrollResult } from "kryon/scroll";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Scroll } from "kryon/tsx/scroll";
import { Text } from "kryon/tsx/text";

let offset: s32 = s32(0);

function DrawRows(session: Session, bounds: Rectangle): void {
    const view = Scroll({session: session, props: {bounds: bounds, content_height: s32(420), scroll_offset: offset},
        children: (result: ScrollResult): void => {
            for (let row: s32 = s32(0); row < s32(10); row += s32(1)) {
                <Text session={session} props={{bounds: {x: result.content.x + 12,
                    y: result.content.y + float32(row) * float32(40), width: 390, height: 32},
                    text: "Scroll to see the next row"}}/>;
            }
        }});
    offset = view.scroll_offset;
}

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Scroll", size: s32(23)}}/>
        {DrawRows(session, nativeInit<Rectangle>({height: 188}))}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    DrawRows(session, {x: viewport.x + viewport.width * float32(24) / float32(480), y: viewport.y + 64,
        width: viewport.width * float32(432) / float32(480), height: 188});
}

export function main(): s32 {
    return App.Run({title: "scroll"}, Frame);
}

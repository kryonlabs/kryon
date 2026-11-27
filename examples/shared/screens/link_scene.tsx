import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { RecordGalleryClick } from "kryon/examples/gallery_clicks";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Link } from "kryon/tsx/link";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let clicks: s32 = s32(0);

interface DocumentationLinkProps {
    session: Session;
    bounds: Rectangle;
    disabled: boolean;
}

function DocumentationLink(link: DocumentationLinkProps): boolean {
    const result = Link({session: link.session, props: {bounds: link.bounds,
        text: "Read the documentation", link: "https://kryonlabs.com/docs.html", disabled: link.disabled}});
    return result.activated;
}

function DrawStandaloneLink(session: Session, disabled: boolean): void {
    if (DocumentationLink({session: session, bounds: nativeInit<Rectangle>({height: 44}), disabled: disabled})) {
        clicks += s32(1);
    }
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Link", size: s32(23)}}/>
        {DrawStandaloneLink(session, disabled)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    RecordGalleryClick(DocumentationLink({session: session, disabled: exampleDisabled,
        bounds: {x: viewport.x + viewport.width * float32(24) / float32(480), y: viewport.y + 64,
            width: float32(432) * (viewport.width / float32(480)), height: 44}}));
}

export function main(): s32 {
    return App.Run({title: "link"}, Frame);
}

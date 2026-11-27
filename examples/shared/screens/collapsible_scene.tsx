import * as App from "kryon/app";
import { CollapsibleHeader } from "kryon/collapsible";
import { StyleTokenColor } from "kryon/drawing_props";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { TransitionMotionState } from "kryon/transition";
import { Box } from "kryon/tsx/box";
import { ClippedBox } from "kryon/tsx/clipped_box";
import { Collapsible } from "kryon/tsx/collapsible";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

const description: string = "The section is open. Click its header to fold it.";
let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let details: boolean = true;
let galleryDetails: boolean = true;
let motion: TransitionMotionState = nativeInit<TransitionMotionState>({});
let headerStorage: [CollapsibleHeader] = [nativeInit<CollapsibleHeader>({})];

function DrawDetails(session: Session, disabled: boolean, reducedMotion: boolean): void {
    const now = float64(CaptureFrameInput(session).elapsed_ms) / float64(1000);
    const view = Collapsible({session: session, props: {motion: nativeRef(motion), now: now, reduced_motion: reducedMotion,
        bounds: {height: 188}, label: "More details", open: details, has_open: true, disabled: disabled},
        headers: nativeSlice(headerStorage, s32(0), s32(0))});
    details = view.open;
    const content = view.content;
    if (content.height > float32(0)) {
        <ClippedBox session={session} props={{bounds: content}} clip={content}>
            <Text session={session} props={{bounds: {x: content.x + 12, y: content.y + 12,
                width: content.width - 24, height: 60}, text: description, size: s32(16)}}/>
        </ClippedBox>;
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    const reducedMotion = exampleReducedMotion;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Collapsible", size: s32(23)}}/>
        {DrawDetails(session, disabled, reducedMotion)}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const scale = viewport.width / float32(480);
    const view = Collapsible({session: session, props: {bounds: {x: viewport.x + float32(24) * viewport.width / float32(480),
        y: viewport.y + 64, width: float32(432) * scale, height: 188}, label: "More details",
        open: galleryDetails, has_open: true, disabled: exampleDisabled}, headers: nativeSlice(headerStorage, s32(0), s32(0))});
    galleryDetails = view.open;
    if (galleryDetails) {
        <Text session={session} props={{bounds: {x: viewport.x + float32(28) * viewport.width / float32(480), y: viewport.y + 124,
            width: float32(420) * scale, height: 38}, text: description}}/>;
    }
}

export function main(): s32 {
    return App.Run({title: "collapsible"}, Frame);
}

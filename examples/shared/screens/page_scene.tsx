import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { Rectangle } from "kryon/geometry";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Column } from "kryon/tsx/layout";
import { Heading, Page, ParagraphText, Section } from "kryon/tsx/page";
import { Text } from "kryon/tsx/text";

function DrawPage(session: Session, bounds: Rectangle, sectionHeight: float32,
    headingWidth: float32, paragraphWidth: float32): void {
    <Page session={session} props={{bounds: bounds, title: "A quiet workspace"}}>
        <Section session={session} props={{label: "Reading", bounds: {height: sectionHeight}}}>
            <Heading session={session} props={{text: "Small, thoughtful details", level: s32(2),
                bounds: {width: headingWidth, height: 38}}}/>
            <ParagraphText session={session} props={{text: "A page groups content, a section gives it meaning, and the type stays easy to read.",
                bounds: {width: paragraphWidth, height: 112}}}/>
        </Section>
    </Page>;
}

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Page Section Heading ParagraphText", size: s32(23)}}/>
        {DrawPage(session, nativeInit<Rectangle>({height: 208}), float32(188), float32(420), float32(410))}
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    DrawPage(session, {x: viewport.x + viewport.width * float32(24) / float32(480), y: viewport.y + 64,
        width: viewport.width * float32(432) / float32(480), height: 208}, float32(184), float32(0), float32(0));
}

export function main(): s32 {
    return App.Run({title: "page"}, Frame);
}

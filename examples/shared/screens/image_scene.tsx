import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { ImageFit } from "kryon/image_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Image } from "kryon/tsx/image";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

export function SetExampleOptions(_disabled: boolean, _reducedMotion: boolean): void {
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Box session={session} props={{bounds: viewport, fill: StyleTokenColor("canvas")}}/>;
    <Column session={session} props={{bounds: viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace6),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{bounds: {height: 36}, text: "Image", size: s32(23)}}/>
        <Image session={session} props={{asset_path: "assets/alpine-lake.webp",
            alt_text: "Orange canoe on a turquoise alpine lake at sunrise", bounds: {height: 168},
            fit: ImageFit.ImageFitCover, radius: 12, smooth: true}}/>
        <Text session={session} props={{bounds: {height: 28}, text: "Alpine morning", size: s32(18)}}/>
        <Text session={session} props={{bounds: {height: 26},
            text: "Cover fit / rounded corners / smooth scaling", size: s32(12)}}/>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: float32(432) * (viewport.width / float32(480))}}}>
        <Image session={session} props={{asset_path: "assets/pfp.webp", alt_text: "Kryon profile image atlas",
            bounds: {width: 192, height: 192}, smooth: true}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "image"}, Frame);
}

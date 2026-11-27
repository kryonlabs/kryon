import * as App from "kryon/app";
import * as NativeCard from "kryon/card";
import { StyleTokenColor } from "kryon/drawing_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { Card, ResolvedCard } from "kryon/tsx/card";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

interface CardCaptionProps {
    session: Session;
    props: NativeCard.Props;
    text: string;
}

function CardCaption(sample: CardCaptionProps): void {
    const session = sample.session;
    const text = sample.text;
    <ResolvedCard session={session} props={sample.props} children={(card: NativeCard.CardResult): void => {
        <Text session={session} props={{bounds: card.content, text: text}}/>;
    }}/>;
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
        <Text session={session} props={{bounds: {height: 36}, text: "Card", size: s32(23)}}/>
        <Card session={session} props={{variant: NativeCard.CardVariant.CardFilled}}>
            <Text session={session} props={{bounds: {height: 20}, text: "Filled card"}}/>
        </Card>
        <Card session={session} props={{variant: NativeCard.CardVariant.CardElevated}}>
            <Text session={session} props={{bounds: {height: 20}, text: "Elevated card"}}/>
        </Card>
        <Card session={session} props={{variant: NativeCard.CardVariant.CardOutlined}}>
            <Text session={session} props={{bounds: {height: 20}, text: "Outlined card"}}/>
        </Card>
    </Column>;
}

export function GalleryFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace5), gap_override: true}}>
        <CardCaption session={session} props={{key: u64(100), bounds: {height: 64}, variant: NativeCard.CardVariant.CardFilled}} text="Filled card"/>
        <CardCaption session={session} props={{key: u64(101), bounds: {height: 64}, variant: NativeCard.CardVariant.CardElevated}} text="Elevated card"/>
        <CardCaption session={session} props={{key: u64(102), bounds: {height: 64}, variant: NativeCard.CardVariant.CardOutlined}} text="Outlined card"/>
    </Column>;
}

export function GalleryThemeFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)},
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <CardCaption session={session} props={{key: u64(10), bounds: {height: 64}}} text="One modern style"/>
        <Text session={session} props={{bounds: {height: 38}, text: "Colors and light/dark remain customizable."}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "card"}, Frame);
}

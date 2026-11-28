import * as App from "kryon/app";
import { ButtonMeasure } from "kryon/button";
import { ButtonEmphasis, ButtonTone } from "kryon/control_props";
import { GalleryClickCount, RecordGalleryClick } from "kryon/examples/gallery_clicks";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Button } from "kryon/tsx/button";
import { Grid } from "kryon/tsx/grid";
import { Column, Row } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { FormatInteger } from "std/number_text";
import { Vec } from "std/vec";

let exampleDisabled: boolean = false;
let clicks: s32 = s32(0);
let digits: Vec<u8> = nativeInit<Vec<u8>>({});

interface ButtonVariantsProps {
    session: Session;
    disabled: boolean;
    tone: ButtonTone;
}

function ButtonVariants(variants: ButtonVariantsProps): void {
    if (<Button session={variants.session} props={{disabled: variants.disabled, bounds: {height: 44},
        label: "Continue", tone: variants.tone, emphasis: ButtonEmphasis.ButtonEmphasisFilled}}/>) {
        clicks += s32(1);
    }
    if (<Button session={variants.session} props={{disabled: variants.disabled, bounds: {height: 44},
        label: "Continue", tone: variants.tone, emphasis: ButtonEmphasis.ButtonEmphasisSoft}}/>) {
        clicks += s32(1);
    }
    if (<Button session={variants.session} props={{disabled: variants.disabled, bounds: {height: 44},
        label: "Continue", tone: variants.tone, emphasis: ButtonEmphasis.ButtonEmphasisOutline}}/>) {
        clicks += s32(1);
    }
}

function DrawCount(session: Session, disabled: boolean): void {
    if (digits.count === s64(0) && !VecResize(digits, s64(24))) {
        return;
    }
    const label = FormatInteger(s64(clicks), VecSlice(digits, s64(0), digits.count));
    <Text session={session} props={{key: u64(2n), disabled: disabled, bounds: {height: 36}, text: label}}/>;
}

function DrawGalleryCount(session: Session, height: float32): void {
    if (digits.count === s64(0) && !VecResize(digits, s64(24))) {
        return;
    }
    let labelBytes: Vec<u8> = nativeInit<Vec<u8>>({});
    const count = FormatInteger(s64(GalleryClickCount()), VecSlice(digits, s64(0), digits.count));
    if (!BuilderAppend(labelBytes, "Clicks: ") || !BuilderAppend(labelBytes, count)) {
        return;
    }
    const label = TextView(VecSlice(labelBytes, s64(0), labelBytes.count));
    <Text session={session} props={{bounds: {height: height}, text: label}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Button", size: s32(28)}}/>
        <Grid session={session} props={{columns: s32(2), gap: Gap(ThemeMetricRole.ThemeMetricSpace3)}}>
            <ButtonVariants session={session} disabled={disabled} tone={ButtonTone.ButtonToneNeutral}/>
            <ButtonVariants session={session} disabled={disabled} tone={ButtonTone.ButtonToneAccent}/>
            <ButtonVariants session={session} disabled={disabled} tone={ButtonTone.ButtonToneDanger}/>
            <ButtonVariants session={session} disabled={disabled} tone={ButtonTone.ButtonToneSuccess}/>
        </Grid>
        {DrawCount(session, disabled)}
    </Column>;
}

export function GalleryCoreFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    const gap = Gap(ThemeMetricRole.ThemeMetricSpace2);
    const height = ButtonMeasure({label: "Continue", disabled: disabled}).height;
    <Column session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480)}, gap: gap, gap_override: true}}>
        <Row session={session} props={{bounds: {height: height}, gap: gap, gap_override: true}}>
            {RecordGalleryClick(<Button session={session} props={{label: "Continue", disabled: disabled,
                tone: ButtonTone.ButtonToneNeutral}}/>)}
            {RecordGalleryClick(<Button session={session} props={{label: "Continue", disabled: disabled,
                tone: ButtonTone.ButtonToneAccent}}/>)}
            {RecordGalleryClick(<Button session={session} props={{label: "Continue", disabled: disabled,
                tone: ButtonTone.ButtonToneDanger}}/>)}
            {RecordGalleryClick(<Button session={session} props={{label: "Continue", disabled: disabled,
                tone: ButtonTone.ButtonToneSuccess}}/>)}
        </Row>
        {DrawGalleryCount(session, height)}
    </Column>;
}

interface GalleryEmphasisPairProps {
    session: Session;
    disabled: boolean;
    emphasis: ButtonEmphasis;
}

function GalleryEmphasisPair(pair: GalleryEmphasisPairProps): void {
    RecordGalleryClick(<Button session={pair.session} props={{bounds: {height: 40}, label: "Neutral action",
        disabled: pair.disabled, emphasis: pair.emphasis}}/>);
    RecordGalleryClick(<Button session={pair.session} props={{bounds: {height: 40}, label: "Accent action",
        disabled: pair.disabled, emphasis: pair.emphasis, tone: ButtonTone.ButtonToneAccent}}/>);
}

export function GalleryEmphasisFrame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const viewport = frame.viewport;
    const disabled = exampleDisabled;
    <Grid session={session} props={{bounds: {x: viewport.x + viewport.width * float32(24) / float32(480),
        y: viewport.y + 64, width: viewport.width * float32(432) / float32(480), height: 248}, columns: s32(2),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true,
        row_gap: Gap(ThemeMetricRole.ThemeMetricSpace3), row_gap_override: true}}>
        <GalleryEmphasisPair session={session} disabled={disabled} emphasis={ButtonEmphasis.ButtonEmphasisFilled}/>
        <GalleryEmphasisPair session={session} disabled={disabled} emphasis={ButtonEmphasis.ButtonEmphasisSoft}/>
        <GalleryEmphasisPair session={session} disabled={disabled} emphasis={ButtonEmphasis.ButtonEmphasisOutline}/>
        <GalleryEmphasisPair session={session} disabled={disabled} emphasis={ButtonEmphasis.ButtonEmphasisGhost}/>
        <GalleryEmphasisPair session={session} disabled={disabled} emphasis={ButtonEmphasis.ButtonEmphasisLink}/>
    </Grid>;
}

export function main(): s32 {
    const result = App.Run({title: "button"}, Frame);
    VecFree(digits);
    return result;
}

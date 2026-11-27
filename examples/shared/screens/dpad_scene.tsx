import * as App from "kryon/app";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { TextAlign } from "kryon/text_align";
import { ThemeMetricRole } from "kryon/theme";
import { DirectionPad } from "kryon/tsx/direction_pad";
import { Column, Row } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { FormatInteger } from "std/number_text";

let exampleDisabled: boolean = false;
let steps: s32 = s32(0);

function DrawControl(session: Session, disabled: boolean): void {
    const now = float64(CaptureFrameInput(session).elapsed_ms) / float64(1000);
    const result = DirectionPad({session: session, props: {disabled: disabled,
        bounds: {width: 180, height: 180}, label: "Move", now: now}});
    if (result.step) {
        steps += s32(1);
    }
}

function DrawStepCount(session: Session, disabled: boolean): void {
    let digits: [u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8] = [u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0)];
    const label = FormatInteger(s64(steps), nativeSlice(digits));
    <Row session={session} props={{}}>
        <Text session={session} props={{disabled: disabled, bounds: {width: 120, height: 28}, text: "Steps",
            size: s32(14), vertical_align: TextAlign.TextAlignCenter}}/>
        <Text session={session} props={{disabled: disabled, bounds: {width: 60, height: 28}, text: label,
            size: s32(14), align: TextAlign.TextAlignEnd, vertical_align: TextAlign.TextAlignCenter}}/>
    </Row>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace3), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Direction pad", size: s32(28)}}/>
        {DrawControl(session, disabled)}
        {DrawStepCount(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "dpad"}, Frame);
}

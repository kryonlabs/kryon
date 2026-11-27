import * as App from "kryon/app";
import { StyleTokenColor } from "kryon/drawing_props";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap, LayoutContainer } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Box } from "kryon/tsx/box";
import { SwipeArea } from "kryon/tsx/joystick";
import { Column, ResolvedStack } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { FormatInteger } from "std/number_text";

let exampleDisabled: boolean = false;
let swipes: s32 = s32(0);

function DrawSwipeSurface(session: Session, disabled: boolean): void {
    const now = float64(CaptureFrameInput(session).elapsed_ms) / float64(1000);
    <ResolvedStack session={session} props={{bounds: {height: 180}}} children={(area: LayoutContainer): void => {
        <Box session={session} props={{bounds: area.bounds, fill: StyleTokenColor("surface"), radius: 12}}/>;
        <Column session={session} props={{padding: Gap(ThemeMetricRole.ThemeMetricSpace4), padding_override: true}}>
            <Text session={session} props={{disabled: disabled, bounds: {height: 36}, text: "Swipe here"}}/>
        </Column>;
        const result = SwipeArea({session: session, props: {disabled: disabled, bounds: area.bounds, label: "Swipe here", now: now}});
        if (result.swiped >= s32(0)) {
            swipes += s32(1);
        }
    }}/>;
    let digits: [u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8, u8] = [u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0), u8(0)];
    const count = FormatInteger(s64(swipes), nativeSlice(digits));
    <Text session={session} props={{disabled: disabled, bounds: {height: 36}, text: count}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Swipe area", size: s32(28)}}/>
        {DrawSwipeSurface(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "swipearea"}, Frame);
}

import * as App from "kryon/app";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { CurrentTheme, ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { LoadingIndicator } from "kryon/tsx/loading_indicator";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    const reducedMotion = exampleReducedMotion || CurrentTheme().motion.reduced_motion;
    const now = reducedMotion ? 0.0 : float64(CaptureFrameInput(session).elapsed_ms) / 1000.0;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40},
            text: "Loading indicator", size: s32(28)}}/>
        <LoadingIndicator session={session} props={{bounds: {height: 180}, now: now, scale: 1.0}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "loading"}, Frame);
}

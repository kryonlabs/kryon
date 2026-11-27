import * as App from "kryon/app";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { ProgressRingAdvance, ProgressRingState } from "kryon/progress_ring";
import { Session } from "kryon/session";
import { CurrentTheme, ThemeMetricRole } from "kryon/theme";
import { Button } from "kryon/tsx/button";
import { Column } from "kryon/tsx/layout";
import { ProgressRing } from "kryon/tsx/progress_ring";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let exampleReducedMotion: boolean = false;
let state: ProgressRingState = nativeInit<ProgressRingState>({});
let value: float32 = float32(0.25);

function DrawProgressControls(session: Session, disabled: boolean): void {
    const delta = float32(CaptureFrameInput(session).delta_ms) / float32(1000);
    const reducedMotion = exampleReducedMotion || CurrentTheme().motion.reduced_motion;
    ProgressRingAdvance(nativeRef(state), value, delta, reducedMotion);
    <ProgressRing session={session} state={nativeRef(state)} props={{bounds: {width: 180, height: 180}, label: "Progress",
        value: value, show_percent: true, reduced_motion: reducedMotion}}/>;
    if (Button({session: session, props: {disabled: disabled, bounds: {width: 160, height: 40}, label: "Advance"}})) {
        value += float32(0.25);
        if (value > float32(1)) {
            value = float32(0);
        }
    }
}

export function SetExampleOptions(disabled: boolean, reducedMotion: boolean): void {
    exampleDisabled = disabled;
    exampleReducedMotion = reducedMotion;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Progress ring", size: s32(28)}}/>
        {DrawProgressControls(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "progressring"}, Frame);
}

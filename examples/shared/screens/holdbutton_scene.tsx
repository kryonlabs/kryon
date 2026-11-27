import * as App from "kryon/app";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { HoldButtonState } from "kryon/hold_button";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { HoldButton } from "kryon/tsx/hold_button";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let state: HoldButtonState = nativeInit<HoldButtonState>({});
let fired: boolean = false;

function DrawConfirmation(session: Session, disabled: boolean): void {
    const now = float32(float64(CaptureFrameInput(session).elapsed_ms) / float64(1000));
    const result = HoldButton({state: nativeRef(state), widget: {session: session, props: {now: now, duration: 1,
        button: {disabled: disabled, bounds: {width: 240, height: 48}, label: "Hold to confirm"}}}});
    if (result.fired) {
        fired = true;
    }
    <Text session={session} props={{disabled: disabled, bounds: {height: 36},
        text: fired ? "Confirmed" : "Hold for one second"}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Hold button", size: s32(28)}}/>
        {DrawConfirmation(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "holdbutton"}, Frame);
}

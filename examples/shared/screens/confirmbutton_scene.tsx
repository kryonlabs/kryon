import * as App from "kryon/app";
import { ConfirmButtonState } from "kryon/confirm_button";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { ConfirmButton } from "kryon/tsx/confirm_button";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let state: ConfirmButtonState = nativeInit<ConfirmButtonState>({});
let confirmed: boolean = false;

function DrawConfirmation(session: Session, disabled: boolean): void {
    const now = float32(float64(CaptureFrameInput(session).elapsed_ms) / float64(1000));
    const result = ConfirmButton({state: nativeRef(state), widget: {session: session, props: {button: {disabled: disabled,
        bounds: {width: 220, height: 44}, label: "Delete item"},
        confirm_button: {disabled: disabled, label: "Confirm delete"}, now: now, timeout: 3}}});
    if (result.committed) {
        confirmed = true;
    }
    let label = "Ready";
    if (confirmed) {
        label = "Item deleted";
    }
    <Text session={session} props={{disabled: disabled, bounds: {height: 36}, text: label}}/>;
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Confirm button", size: s32(28)}}/>
        {DrawConfirmation(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "confirmbutton"}, Frame);
}

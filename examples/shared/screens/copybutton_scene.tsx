import * as App from "kryon/app";
import { CopyButtonState } from "kryon/copy_button";
import { CaptureFrameInput } from "kryon/frame_snapshot";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { CopyButton } from "kryon/tsx/copy_button";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let state: CopyButtonState = nativeInit<CopyButtonState>({});

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    const now = float32(float64(CaptureFrameInput(session).elapsed_ms) / float64(1000));
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Copy button", size: s32(28)}}/>
        <CopyButton state={nativeRef(state)} widget={{session: session, props: {button: {disabled: disabled,
            bounds: {width: 240, height: 44}, label: "Copy website address"}, text: "https://github.com/kryonlabs/kryon",
            now: now, success_label: "Copied", failure_label: "Copy failed"}}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "copybutton"}, Frame);
}

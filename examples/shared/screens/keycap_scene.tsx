import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Keycap } from "kryon/tsx/keycap";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport,
        padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Keycap", size: s32(28)}}/>
        <Keycap session={session} props={{disabled: disabled, bounds: {width: 160, height: 44}, keys: "Ctrl+K"}}/>
        <Keycap session={session} props={{disabled: disabled, bounds: {width: 160, height: 44}, keys: "Shift+Enter"}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "keycap"}, Frame);
}

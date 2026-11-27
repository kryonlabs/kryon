import { AlertState } from "kryon/alert";
import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Alert } from "kryon/tsx/alert";
import { Button } from "kryon/tsx/button";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let state: AlertState = nativeInit<AlertState>({});

function DrawShowAlertButton(session: Session, disabled: boolean): void {
    if (Button({session: session, props: {disabled: disabled, bounds: {width: 160, height: 40}, label: "Show alert"}})) {
        state.dismissed = false;
    }
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
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Alert", size: s32(28)}}/>
        <Alert session={session} state={nativeRef(state)} props={{disabled: disabled, bounds: {height: 180},
            title: "Changes saved", description: "Your settings are up to date.", dismissible: true}}/>
        {DrawShowAlertButton(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "alert"}, Frame);
}

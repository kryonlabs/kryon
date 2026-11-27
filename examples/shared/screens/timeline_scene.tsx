import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";
import { Timeline } from "kryon/tsx/timeline";

let exampleDisabled: boolean = false;

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    const disabled = exampleDisabled;
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Timeline", size: s32(28)}}/>
        <Timeline session={session} props={{disabled: disabled, bounds: {height: 100}, title: "Project created",
            time: "09:00", description: "The workspace is ready."}}/>
        <Timeline session={session} props={{disabled: disabled, bounds: {height: 100}, title: "First component added",
            time: "09:15", description: "Button is available to use.", last: true}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "timeline"}, Frame);
}

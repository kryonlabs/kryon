import * as App from "kryon/app";
import { BadgeTone } from "kryon/badge";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Badge } from "kryon/tsx/badge";
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
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Badge", size: s32(28)}}/>
        <Badge session={session} props={{disabled: disabled, bounds: {width: 140, height: 36},
            text: "Ready", tone: BadgeTone.BadgeToneNeutral, dot: true, solid: true}}/>
        <Badge session={session} props={{disabled: disabled, bounds: {width: 140, height: 36},
            text: "Ready", tone: BadgeTone.BadgeToneInfo, dot: true, solid: false}}/>
        <Badge session={session} props={{disabled: disabled, bounds: {width: 140, height: 36},
            text: "Ready", tone: BadgeTone.BadgeToneSuccess, dot: true, solid: true}}/>
        <Badge session={session} props={{disabled: disabled, bounds: {width: 140, height: 36},
            text: "Ready", tone: BadgeTone.BadgeToneWarning, dot: true, solid: false}}/>
        <Badge session={session} props={{disabled: disabled, bounds: {width: 140, height: 36},
            text: "Ready", tone: BadgeTone.BadgeToneDanger, dot: true, solid: true}}/>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "badge"}, Frame);
}

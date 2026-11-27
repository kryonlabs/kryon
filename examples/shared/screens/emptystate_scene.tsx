import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { EmptyState } from "kryon/tsx/empty_state";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let created: boolean = false;

function DrawProjectState(session: Session, disabled: boolean): void {
    if (created) {
        <Text session={session} props={{disabled: disabled, bounds: {height: 180}, text: "Project created"}}/>;
    } else {
        const result = EmptyState({session: session, props: {disabled: disabled, bounds: {height: 224},
            title: "No projects yet", description: "Create your first project.", action: "Create project"}});
        if (result.action) {
            created = true;
        }
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
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Empty state", size: s32(28)}}/>
        {DrawProjectState(session, disabled)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "emptystate"}, Frame);
}

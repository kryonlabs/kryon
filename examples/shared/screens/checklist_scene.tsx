import * as App from "kryon/app";
import { Gap } from "kryon/layout";
import { OnboardingChecklistTask } from "kryon/onboarding_checklist_task";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Column } from "kryon/tsx/layout";
import { OnboardingChecklist } from "kryon/tsx/onboarding_checklist";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let done: [boolean, boolean, boolean] = [false, false, false];

function DrawTasks(session: Session): void {
    const tasks: [OnboardingChecklistTask, OnboardingChecklistTask, OnboardingChecklistTask] = [
        nativeInit<OnboardingChecklistTask>({id: u64(1), title: "Create a project", done: done[s32(0)]}),
        nativeInit<OnboardingChecklistTask>({id: u64(2), title: "Add a component", done: done[s32(1)]}),
        nativeInit<OnboardingChecklistTask>({id: u64(3), title: "Run the app", done: done[s32(2)]}),
    ];
    const result = OnboardingChecklist({session: session, props: {disabled: exampleDisabled,
        bounds: {height: 224}, title: "Get started", tasks: nativeSlice(tasks)}});
    const index = result.toggled_index;
    if (index >= s32(0) && index < s32(3)) {
        done[index] = !done[index];
    }
}

export function SetExampleOptions(disabled: boolean, _reducedMotion: boolean): void {
    exampleDisabled = disabled;
}

export function Frame(frame: App.Frame): void {
    const session: Session = {slot: frame.slot, epoch: frame.epoch};
    <Column session={session} props={{bounds: frame.viewport, padding: Gap(ThemeMetricRole.ThemeMetricSpace5),
        gap: Gap(ThemeMetricRole.ThemeMetricSpace4), gap_override: true}}>
        <Text session={session} props={{disabled: exampleDisabled, bounds: {height: 40},
            text: "Onboarding checklist", size: s32(28)}}/>
        {DrawTasks(session)}
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "checklist"}, Frame);
}

import * as App from "kryon/app";
import { ButtonTone } from "kryon/control_props";
import { Gap } from "kryon/layout";
import { Session } from "kryon/session";
import { ThemeMetricRole } from "kryon/theme";
import { Chip } from "kryon/tsx/chip";
import { Flow } from "kryon/tsx/flow";
import { Column } from "kryon/tsx/layout";
import { Text } from "kryon/tsx/text";

let exampleDisabled: boolean = false;
let filters: [boolean, boolean, boolean, boolean] = [true, false, false, false];
let tags: [boolean, boolean, boolean] = [true, true, true];

function DrawFilters(session: Session, disabled: boolean): void {
    const names: [string, string, string, string] = ["All", "Design", "Code", "Docs"];
    for (let index = s32(0); index < s32(4); index = index + s32(1)) {
        const chip = Chip({session: session, props: {label: names[index], selectable: true,
            selected: filters[index], tone: ButtonTone.ButtonToneAccent, disabled: disabled}});
        if (chip.changed) {
            filters[index] = chip.selected;
        }
    }
}

function DrawTags(session: Session, disabled: boolean): void {
    const names: [string, string, string] = ["Draft", "Shared", "Pinned"];
    for (let index = s32(0); index < s32(3); index = index + s32(1)) {
        if (tags[index]) {
            const chip = Chip({session: session, props: {label: names[index], dismissible: true, disabled: disabled}});
            if (chip.dismissed) {
                tags[index] = false;
            }
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
        <Text session={session} props={{disabled: disabled, bounds: {height: 40}, text: "Chip", size: s32(28)}}/>
        <Flow session={session} props={{gap: s32(8)}}>
            {DrawFilters(session, disabled)}
        </Flow>
        <Flow session={session} props={{gap: s32(8)}}>
            {DrawTags(session, disabled)}
        </Flow>
    </Column>;
}

export function main(): s32 {
    return App.Run({title: "chip"}, Frame);
}
